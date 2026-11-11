/*
 * payload.c --> glues the app together: sensors, state machine, lock-in queue
 * and logging
 *
 * who owns what: payload_adc_cycle_isr() runs in the ADC DMA interrupt and
 * owns g_lockin and g_range, everything else runs in the main loop. the two
 * sides only share the SPSC queue and the volatile request/flag words
 * biggest file, good luck (｡•́︿•̀｡)
 */
#include "payload.h"
#include "altitude.h"
#include "attitude.h"
#include "config.h"
#include "console.h"
#include "deploy.h"
#include "drivers.h"
#include "lockin.h"
#include "logger.h"
#include "persist.h"
#include "powerloss.h"
#include "range.h"
#include "record.h"
#include "spsc.h"

#include <string.h>

/*---- ISR owned ----*/
static lockin_t g_lockin;
static range_ctl_t g_range;
static spsc_t g_queue;
static volatile int8_t g_range_request = -1; /*-1 none, 0 HI, 1 LO, 2 AUTO*/

/*---- main loop owned: zeroed by payload_init(), like .bss after reset ----*/
static struct {
    sm_t sm;
    deploy_t dep;
    nv_state_t nv; /*power loss record, copy of whats in flash*/
    bool nv_valid;
    uint32_t t_nv_refresh;
    bool pl_pending; /*powered up with the arm pin out --> deciding*/
    uint32_t t_pl_start;
    altitude_t alt;
    attitude_t att;
    logger_t log;
    payload_stats_t stats;
    int32_t p_pa;
    int16_t baro_t_cc;
    uint32_t t_baro;
    bool baro_seen;
    int16_t acc_mg[3];
    int16_t gyr_dps10[3];
    uint32_t t_imu;
    bool imu_seen;
    uint16_t vbat_mv;
    bool low_batt;
    bool console_arm;
    bool motor_on;
    uint16_t sticky_flags;
    uint32_t last_slips;
    uint32_t last_dropped;
    uint8_t last_range;
    uint32_t t_imu_due, t_sm_due, t_slow_log, t_fast_log, t_vbat, t_status;
    uint32_t t_selftest;
    uint16_t spin_min, spin_max;
    uint32_t spin_outputs;
} g;

/*------------------------------------------------------------------*/
/*interrupt side*/
/*------------------------------------------------------------------*/

void payload_adc_cycle_isr(const uint16_t *samples)
{
    float x, y;
    uint16_t mn, mx;
    lockin_out_t out;

    int8_t req = g_range_request;
    if (req >= 0) {
        g_range_request = -1;
        if (req == 2) {
            range_force(&g_range, g_range.range, true);
        } else {
            range_force(&g_range, (uint8_t)req, false);
            drv_frontend_set_range((uint8_t)req);
        }
    }

    lockin_cycle(samples, &x, &y, &mn, &mx);
    range_result_t rr = range_on_cycle(&g_range, mn, mx);
    if (rr.switched) {
        drv_frontend_set_range(g_range.range);
    }
    bool at_speed = drv_motor_at_speed();
    uint16_t cflags = rr.flags | (at_speed ? 0u : RF_SETTLING);
    if (lockin_push(&g_lockin, x, y, rr.valid && at_speed, cflags, mn, mx, g_range.range, &out)) {
        if (range_on_output(&g_range, out.x, out.y, out.n_valid)) {
            drv_frontend_set_range(g_range.range);
        }
        (void)spsc_push(&g_queue, &out);
    }
}

/*------------------------------------------------------------------*/
/*helpers*/
/*------------------------------------------------------------------*/

static void con_time(uint32_t now)
{
    con_str("[");
    con_fixed((int32_t)now, 3);
    con_str("] ");
}

static void log_event(uint8_t code, uint8_t arg8, uint16_t arg16, int32_t a, int32_t b, const char *text)
{
    event_rec_t e = { 0 };
    uint8_t buf[RECORD_SIZE];
    e.seq = logger_next_seq(&g.log);
    e.t_ms = drv_millis();
    e.code = code;
    e.arg8 = arg8;
    e.arg16 = arg16;
    e.a = a;
    e.b = b;
    for (int i = 0; text && i < 31 && text[i]; i++) {
        e.text[i] = text[i];
    }
    record_pack_event(&e, buf);
    logger_put(&g.log, buf);
    g.stats.event_records++;
}

static uint16_t sensor_flags(uint32_t now)
{
    uint16_t f = g.sticky_flags;
    if (!g.baro_seen || now - g.t_baro > 200u) {
        f |= RF_BARO_STALE;
    }
    if (!g.imu_seen || now - g.t_imu > 100u) {
        f |= RF_IMU_STALE;
    }
    for (int i = 0; i < 3; i++) {
        if (g.acc_mg[i] >= 31900 || g.acc_mg[i] <= -31900) {
            f |= RF_IMU_CLIP;
        }
    }
    if (g.low_batt) {
        f |= RF_LOW_BATT;
    }
    if (g.log.retried) {
        f |= RF_SD_RETRY;
    }
    uint32_t slips = drv_motor_slip_count();
    if (slips != g.last_slips) {
        f |= RF_STEP_SLIP;
    }
    if (g_queue.dropped != g.last_dropped) {
        f |= RF_QUEUE_OVERFLOW;
        g.last_dropped = g_queue.dropped;
    }
    if (g.motor_on && drv_motor_at_speed() && drv_motor_rev_period_us() == 0) {
        f |= RF_INDEX_MISSING;
    }
    if (!g.motor_on) {
        f |= RF_MOTOR_OFF;
    }
    if (drv_deploy_is_extended()) {
        f |= RF_DEPLOYED;
    }
    return f;
}

static void write_science(uint32_t now, const lockin_out_t *o)
{
    science_rec_t r = { 0 };
    uint8_t buf[RECORD_SIZE];
    uint32_t slips = drv_motor_slip_count();

    r.seq = logger_next_seq(&g.log);
    r.t_ms = now;
    r.state = (uint8_t)g.sm.state;
    r.flags = sensor_flags(now);
    if (o) {
        r.range = o->range;
        r.flags |= o->flags;
        r.x = o->x;
        r.y = o->y;
        r.raw_min = o->raw_min;
        r.raw_max = o->raw_max;
        r.n_valid = o->n_valid;
        for (int i = 0; i < K_BLOCKS; i++) {
            r.cx[i] = o->cx[i];
            r.cy[i] = o->cy[i];
        }
    } else {
        r.range = g.last_range;
        for (int i = 0; i < K_BLOCKS; i++) {
            r.cx[i] = INVALID_CYCLE;
            r.cy[i] = INVALID_CYCLE;
        }
    }
    r.slip_count = (uint8_t)(slips > 255u ? 255u : slips);
    r.vbat_mv = g.vbat_mv;
    r.pressure_pa = g.p_pa;
    r.baro_temp_cc = g.baro_t_cc;
    r.mcu_temp_cc = drv_adc_mcu_temp_cc();
    r.alt_cm = (int32_t)(g.alt.h_m * 100.0f);
    float vz = g.alt.v_mps * 10.0f;
    r.vz_dms = (int16_t)(vz > 32767.0f ? 32767 : (vz < -32767.0f ? -32767 : vz));
    for (int i = 0; i < 3; i++) {
        r.accel_mg[i] = g.acc_mg[i];
        r.gyro_dps10[i] = g.gyr_dps10[i];
    }
    r.rotor_period_us = drv_motor_rev_period_us();
    r.index_phase = drv_motor_index_phase();
    r.p_ref_pa = (int32_t)g.alt.p_ref_pa;
    r.t_launch_ms = g.sm.launched ? g.sm.t_launch_ms : 0u;
    for (int i = 0; i < 4; i++) {
        r.quat[i] = (int16_t)(g.att.q[i] * 30000.0f);
    }
    r.att_ok = g.att.init ? 1u : 0u;

    if (slips != g.last_slips) {
        log_event(EV_STEP_SLIP, 0, 0, (int32_t)slips, 0, "step slip");
        g.last_slips = slips;
    }
    record_pack_science(&r, buf);
    logger_put(&g.log, buf);
    g.stats.science_records++;
}

static void set_motor(bool on, uint32_t now)
{
    if (on == g.motor_on) {
        return;
    }
    g.motor_on = on;
    if (on) {
        lockin_init(&g_lockin);
        drv_motor_start();
        drv_adc_start();
    } else {
        drv_adc_stop();
        drv_motor_stop();
    }
    con_time(now);
    con_str(on ? "MOTOR ON" : "MOTOR OFF");
    con_nl();
    log_event(EV_MOTOR, on ? 1u : 0u, 0, 0, 0, on ? "motor on" : "motor off");
}

static void print_status(uint32_t now)
{
    con_time(now);
    con_str("STATUS ");
    con_str(sm_state_name(g.sm.state));
    con_str(" alt=");
    con_fixed((int32_t)(g.alt.h_m * 10.0f), 1);
    con_str("m vz=");
    con_fixed((int32_t)(g.alt.v_mps * 10.0f), 1);
    con_str("m/s ax=");
    con_i32(g.acc_mg[IMU_AXIAL_AXIS] * IMU_AXIAL_SIGN);
    con_str("mg p=");
    con_i32(g.p_pa);
    con_str("Pa rng=");
    con_str(g.last_range == RANGE_HI ? "HI" : "LO");
    con_str(" rec=");
    con_u32(g.log.written);
    con_str(" err=");
    con_u32(g.log.errors);
    con_nl();
}

/*---- power loss record (team decision 2026-10 D4, app/powerloss.h) ----*/

static void nv_store(uint32_t flags, float pad_p_pa)
{
    g.nv.flags = flags;
    g.nv.pad_p_pa = pad_p_pa;
    g.nv.write_count++;
    nv_seal(&g.nv);
    g.nv_valid = drv_nv_write(&g.nv, sizeof g.nv);
}

static void nv_clear(void)
{
    if (g.nv_valid && g.nv.flags != 0u) {
        nv_store(0u, 0.0f);
    }
}

/*pad pressure written on arming and refreshed while armed, so the saved
 * reference is never more than NV_PAD_REFRESH_MS behind the weather*/
static void nv_pad_tick(uint32_t now)
{
    if (g.sm.state != ST_ARMED || !g.alt.ref_valid) {
        return;
    }
    if (!(g.nv_valid && (g.nv.flags & NV_ARMED) && g.nv.pad_p_pa > 0.0f) ||
        now - g.t_nv_refresh >= NV_PAD_REFRESH_MS) {
        g.t_nv_refresh = now;
        nv_store(NV_ARMED, g.alt.p_ref_pa);
    }
}

static void on_state_change(uint32_t now, flight_state_t old)
{
    con_time(now);
    con_str("STATE ");
    con_str(sm_state_name(old));
    con_str(" -> ");
    con_str(sm_state_name(g.sm.state));
    con_nl();
    log_event(EV_STATE_CHANGE, (uint8_t)g.sm.state, (uint16_t)old,
              g.sm.launched ? (int32_t)(now - g.sm.t_launch_ms) : -1, g.sm.alt_max_cm,
              sm_state_name(g.sm.state));
    if (g.sm.launched) {
        altitude_freeze_ref(&g.alt);
    }
    if (g.sm.state == ST_LANDED || g.sm.state == ST_APOGEE) {
        logger_flush(&g.log);
    }
    if (old == ST_ARMED && g.sm.state == ST_SAFE) {
        nv_clear(); /*disarmed on the pad*/
    }
    if (g.sm.launched && g.nv_valid && !(g.nv.flags & NV_LAUNCHED)) {
        nv_store(g.nv.flags | NV_ARMED | NV_LAUNCHED, g.alt.p_ref_pa); /*ref is frozen now*/
    }
}

/*called from the 100 Hz slot while g.pl_pending: after POWERLOSS_DECIDE_MS
 * of barometer and IMU data, work out where we are (powerloss_decide) and
 * go into that state. nothing moves before this*/
static void powerloss_try_decide(uint32_t now)
{
    bool baro_ok = g.baro_seen && (now - g.t_baro) <= 200u;
    if (now - g.t_pl_start < POWERLOSS_DECIDE_MS ||
        (!baro_ok && now - g.t_pl_start < POWERLOSS_BARO_WAIT_MS)) {
        return;
    }
    pl_inputs_t in;
    in.nv_valid = g.nv_valid;
    in.nv = g.nv;
    in.baro_ok = baro_ok;
    in.p_now_pa = (float)g.p_pa;
    in.imu_ok = g.imu_seen && (now - g.t_imu) <= 100u;
    in.accel_axial_mg = (int32_t)g.acc_mg[IMU_AXIAL_AXIS] * IMU_AXIAL_SIGN;
    pl_verdict_t v = powerloss_decide(&in);

    float alt_m = (g.nv_valid && baro_ok) ? altitude_from_pressure(in.p_now_pa, g.nv.pad_p_pa) : 0.0f;
    int32_t alt_cm = (int32_t)(alt_m * 100.0f);
    g.pl_pending = false;
    flight_state_t old = g.sm.state;
    if (v == PL_BOOST || v == PL_IN_FLIGHT || v == PL_LANDED) {
        /*use the saved pad reference and restart the altitude filter from
         * the current height (no jump through the alpha-beta filter)*/
        altitude_set_ref(&g.alt, g.nv.pad_p_pa, true);
        g.alt.init = false;
        flight_state_t st = v == PL_BOOST ? ST_BOOST : (v == PL_IN_FLIGHT ? ST_COAST : ST_LANDED);
        /*time since launch is unknown, so all the time-outs and the apogee
         * lock-out restart from now. NOT skipping the lock-out matters: a cut
         * while still transonic would let the barometer's Mach 1 pressure
         * error fake a fall (found in my flight sim tests, scary). cost: a
         * cut near apogee gets its APOGEE marker up to APOGEE_LOCKOUT_MS late,
         * the mills run through COAST and DESCENT either way*/
        sm_resume(&g.sm, st, 0u, alt_cm, now);
        g.sticky_flags |= RF_RESUMED;
        g.stats.resumed = true;
    } else {
        sm_init(&g.sm, ST_ARMED, now); /*pad or unknown --> wait for launch, mills in*/
    }
    log_event(EV_POWER_RECOVERY, (uint8_t)v, 0, alt_cm, (int32_t)(g.nv_valid ? g.nv.flags : 0u),
              "power-on, arm pin out");
    con_time(now);
    con_str("POWER-ON RECOVERY: ");
    con_str(powerloss_name(v));
    con_nl();
    on_state_change(now, old);
}

static void finish_selftest(uint32_t now)
{
    uint16_t f = g.stats.selftest_faults;
#if SELFTEST_SPIN_RETRACTED
    if (!drv_motor_at_speed()) {
        f |= STF_MOTOR;
    }
    if (drv_motor_rev_period_us() == 0) {
        f |= STF_INDEX;
    }
    /*mid rail check: retracted, the mill is shielded and the waveform must
     * sit near mid scale*/
    if (g.spin_outputs == 0 || g.spin_min < ADC_MID_COUNTS / 2 || g.spin_max > ADC_MID_COUNTS + ADC_MID_COUNTS / 2) {
        f |= STF_ADC_MID;
    }
#else
    f |= STF_NO_SPIN;
#endif
    g.stats.selftest_faults = f;
    set_motor(false, now);
    log_event(EV_SELFTEST, 0, f, 0, 0, f ? "selftest faults" : "selftest pass");
    con_time(now);
    con_str("SELFTEST faults=0x");
    {
        static const char hex[] = "0123456789ABCDEF";
        char h[5] = { hex[(f >> 12) & 15], hex[(f >> 8) & 15], hex[(f >> 4) & 15], hex[f & 15], 0 };
        con_str(h);
    }
    con_nl();
    flight_state_t old = g.sm.state;
    sm_init(&g.sm, ST_SAFE, now);
    on_state_change(now, old);
}

static void step_deploy(uint32_t now)
{
    int32_t since = g.sm.launched ? (int32_t)(now - g.sm.t_launch_ms) : -1;
    const char *msg = 0;
    uint8_t code = 0;
    switch (deploy_step(&g.dep, g.sm.state, drv_deploy_is_extended(), now)) {
    case DEP_ACT_EXTEND:
        drv_deploy_extend();
        code = DEPLOY_EV_COMMANDED;
        msg = "deploy commanded";
        break;
    case DEP_ACT_CONFIRMED:
        code = DEPLOY_EV_EXTENDED;
        msg = "mills extended";
        if (g.nv_valid && !(g.nv.flags & NV_DEPLOYED)) {
            nv_store(g.nv.flags | NV_DEPLOYED, g.nv.pad_p_pa);
        }
        break;
    case DEP_ACT_FAILED:
        code = DEPLOY_EV_FAILED;
        msg = "deploy failed, motor off";
        break;
    default:
        return;
    }
    log_event(EV_DEPLOY, code, 0, since, 0, msg);
    con_time(now);
    con_str("DEPLOY ");
    con_str(msg);
    con_nl();
}

static void update_led(uint32_t now)
{
    uint32_t ph;
    bool on;
    switch (g.sm.state) {
    case ST_SELFTEST:
        on = true;
        break;
    case ST_SAFE:
        on = (g.stats.selftest_faults & ~STF_NO_SPIN) ? ((now / 50u) & 1u) : ((now % 2000u) < 100u);
        break;
    case ST_ARMED:
        on = (now % 500u) < 250u;
        break;
    case ST_LANDED:
        ph = now % 2000u;
        on = (ph < 600u) && ((ph / 100u) % 2u == 0u);
        break;
    default:
        on = true;
        break;
    }
    drv_led_set(on);
}

/*------------------------------------------------------------------*/
/*public*/
/*------------------------------------------------------------------*/

void payload_init(void)
{
    persist_t ps;
    header_rec_t h = { 0 };
    uint8_t buf[RECORD_SIZE];

    memset(&g, 0, sizeof g);
    g.spin_min = 0xFFFFu;
    g_range_request = -1;

    drv_time_init();
    drv_gpio_init();
    drv_uart_init();
    con_nl();
    con_str("Galvan field mill payload fw ");
    con_u32(FW_VERSION_MAJOR);
    con_str(".");
    con_u32(FW_VERSION_MINOR);
    con_str(" ");
    con_str(FW_GIT_HASH);
    con_nl();

    lockin_init(&g_lockin);
    range_init(&g_range, RANGE_HI);
    spsc_init(&g_queue);
    altitude_init(&g.alt);
    attitude_init(&g.att);

    bool intact = persist_load(&ps);
    bool resume = intact && ps.state >= ST_BOOST && ps.state <= ST_LANDED;
    g.nv_valid = drv_nv_read(&g.nv, sizeof g.nv) && nv_check(&g.nv);
    if (!g.nv_valid) {
        memset(&g.nv, 0, sizeof g.nv);
    }

    uint32_t now = drv_millis();
    sm_init(&g.sm, ST_INIT, now);

    if (!logger_init(&g.log)) {
        g.stats.selftest_faults |= STF_STORAGE;
    }
    if (!drv_baro_init()) {
        g.stats.selftest_faults |= STF_BARO;
    }
    if (!drv_imu_init()) {
        g.stats.selftest_faults |= STF_IMU;
    }
    drv_adc_init();
    drv_motor_init();
    drv_deploy_init();
    deploy_init(&g.dep);
    drv_frontend_set_range(RANGE_HI);
    g.last_range = RANGE_HI;
    g.vbat_mv = drv_adc_vbat_mv();
    if (g.vbat_mv < VBAT_LOW_MV) {
        g.stats.selftest_faults |= STF_VBAT;
    }

    h.seq = logger_next_seq(&g.log);
    h.t_ms = now;
    h.reset_cause = drv_reset_cause();
    h.boot_count = ps.boot_count;
    h.resumed_state = resume ? ps.state : 0xFFu;
    h.selftest_faults = g.stats.selftest_faults;
    h.p_ref_pa = resume ? (int32_t)ps.p_ref_pa : 0;
    record_pack_header(&h, buf);
    logger_put(&g.log, buf);

    con_str("log file: ");
    con_str(g.log.ok ? g.log.name : "(none)");
    con_nl();

    if (resume) {
        /*reset mid flight --> skip the self test spin and carry on*/
        sm_resume(&g.sm, (flight_state_t)ps.state, ps.ms_since_launch, ps.alt_max_cm, now);
        altitude_set_ref(&g.alt, ps.p_ref_pa, true);
        g.sticky_flags |= RF_RESUMED;
        g.stats.resumed = true;
        log_event(EV_RESUMED, ps.state, 0, (int32_t)ps.ms_since_launch, 0, "resumed after reset");
        con_str("RESUMED in ");
        con_str(sm_state_name(g.sm.state));
        con_nl();
    } else if (drv_arm_switch_armed()) {
        /*powered up with the arm pin already out and no RAM state --> maybe
         * a power cut in flight. log straight away, no self test spin, nothing
         * moves until powerloss_try_decide() has looked at the sensors*/
        g.pl_pending = true;
        g.t_pl_start = now;
        con_str("POWER-ON WITH ARM PIN OUT: checking where we are");
        con_nl();
    } else {
        nv_clear(); /*pin in --> on the bench or pad, safe*/
        sm_init(&g.sm, ST_SELFTEST, now);
        g.t_selftest = now;
        on_state_change(now, ST_INIT);
#if SELFTEST_SPIN_RETRACTED
        set_motor(true, now);
#else
        finish_selftest(now);
#endif
    }
    g.t_imu_due = g.t_sm_due = g.t_slow_log = g.t_fast_log = g.t_vbat = g.t_status = now;
    drv_watchdog_init();
}

void payload_loop_once(void)
{
    uint32_t now = drv_millis();
    lockin_out_t out;
    char line[24];

    /*console*/
    switch (console_poll(line, sizeof line)) {
    case CMD_STATUS: print_status(now); break;
    case CMD_ARM: g.console_arm = true; log_event(EV_COMMAND, 0, 0, 0, 0, line); break;
    case CMD_DISARM: g.console_arm = false; log_event(EV_COMMAND, 0, 0, 0, 0, line); break;
    case CMD_RANGE_HI: g_range_request = 0; break;
    case CMD_RANGE_LO: g_range_request = 1; break;
    case CMD_RANGE_AUTO: g_range_request = 2; break;
    case CMD_VERSION: con_str(FW_GIT_HASH); con_nl(); break;
    case CMD_UNKNOWN: con_str("? STATUS ARM DISARM RANGE HI|LO|AUTO VERSION"); con_nl(); break;
    default: break;
    }

    drv_motor_tick(now);
    drv_deploy_tick(now);

    /*IMU at 100 Hz*/
    if ((int32_t)(now - g.t_imu_due) >= 0) {
        g.t_imu_due += IMU_PERIOD_MS;
        if (drv_imu_read(g.acc_mg, g.gyr_dps10)) {
            g.t_imu = now;
            g.imu_seen = true;
            attitude_update(&g.att, g.acc_mg, g.gyr_dps10, (float)IMU_PERIOD_MS * 0.001f);
        }
    }

    /*barometer, the driver paces its own conversions*/
    {
        int32_t p;
        int16_t t;
        if (drv_baro_poll(now, &p, &t)) {
            g.p_pa = p;
            g.baro_t_cc = t;
            g.t_baro = now;
            g.baro_seen = true;
            altitude_update(&g.alt, (float)p, now);
        }
    }

    /*battery at 1 Hz*/
    if (now - g.t_vbat >= 1000u) {
        g.t_vbat = now;
        g.vbat_mv = drv_adc_vbat_mv();
        if (!g.low_batt && g.vbat_mv < VBAT_LOW_MV) {
            g.low_batt = true;
            log_event(EV_LOW_BATT, 0, g.vbat_mv, 0, 0, "battery low");
        }
    }

    /*state machine at 100 Hz*/
    if ((int32_t)(now - g.t_sm_due) >= 0) {
        g.t_sm_due += SM_PERIOD_MS;
        if (g.pl_pending) {
            powerloss_try_decide(now);
        } else if (g.sm.state == ST_SELFTEST) {
            if (now - g.t_selftest >= SELFTEST_SPIN_MS) {
                finish_selftest(now);
            }
        } else {
            sm_inputs_t in;
            in.now_ms = now;
            in.accel_axial_mg = (int32_t)g.acc_mg[IMU_AXIAL_AXIS] * IMU_AXIAL_SIGN;
            in.imu_ok = g.imu_seen && (now - g.t_imu) <= 100u;
            in.alt_cm = (int32_t)(g.alt.h_m * 100.0f);
            in.vz_cmps = (int32_t)(g.alt.v_mps * 100.0f);
            in.baro_ok = g.baro_seen && (now - g.t_baro) <= 200u;
            in.arm_request = drv_arm_switch_armed() || g.console_arm;
            flight_state_t old = g.sm.state;
            if (sm_step(&g.sm, &in)) {
                on_state_change(now, old);
            }
            if (g.sm.launched) {
                persist_save((uint8_t)g.sm.state, now - g.sm.t_launch_ms, g.sm.alt_max_cm, g.alt.p_ref_pa);
            }
            step_deploy(now);
            set_motor(sm_motor_wanted(&g.sm) && deploy_mill_ready(&g.dep), now);
            nv_pad_tick(now);
        }
    }

    /*lock-in outputs from the ISR*/
    while (spsc_pop(&g_queue, &out)) {
        g.stats.outputs++;
        if (out.range != g.last_range) {
            g.stats.range_switches++;
            log_event(EV_RANGE_SWITCH, out.range, 0, 0, 0, out.range == RANGE_HI ? "range HI" : "range LO");
            con_time(now);
            con_str(out.range == RANGE_HI ? "RANGE -> HI" : "RANGE -> LO");
            con_nl();
            g.last_range = out.range;
        }
        if (g.sm.state == ST_SELFTEST) {
            g.spin_outputs++;
            if (out.raw_min < g.spin_min) g.spin_min = out.raw_min;
            if (out.raw_max > g.spin_max) g.spin_max = out.raw_max;
        }
        if (sm_fast_logging(&g.sm) || g.sm.state == ST_SELFTEST) {
            write_science(now, &out);
        }
    }

    /*sensor only records while the motor is off: 10 Hz in flight (BOOST,
     * COAST before deployment), 1 Hz on the pad and after landing*/
    if (!g.motor_on && g.sm.state != ST_SELFTEST) {
        if (sm_fast_logging(&g.sm) || g.pl_pending) {
            if (now - g.t_fast_log >= FAST_LOG_PERIOD_MS) {
                g.t_fast_log = now;
                write_science(now, 0);
            }
        } else if (now - g.t_slow_log >= SAFE_LOG_PERIOD_MS) {
            g.t_slow_log = now;
            write_science(now, 0);
        }
    }

#ifdef PAYLOAD_VERBOSE
    if (now - g.t_status >= 1000u) {
        g.t_status = now;
        print_status(now);
    }
#endif

    logger_tick(&g.log, now);
    g.stats.storage_errors = g.log.errors;
    update_led(now);
    drv_watchdog_kick();
}

flight_state_t payload_state(void)
{
    return g.sm.state;
}

const payload_stats_t *payload_stats(void)
{
    return &g.stats;
}
