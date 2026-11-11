/*
 * test_unit.c --> unit tests for the portable app code (runs on the PC)
 * covers: CRC-16, float maths, MS5611 compensation, lock-in against golden
 * vectors from python, range control, state machine, record packing,
 * altitude conversion and the SPSC queue. 242 checks, all pass ✨
 */
#include <math.h>
#include <string.h>

#include "altitude.h"
#include "check.h"
#include "powerloss.h"
#include "attitude.h"
#include "config.h"
#include "crc16.h"
#include "deploy.h"
#include "fmath.h"
#include "lockin.h"
#include "ms5611_calc.h"
#include "range.h"
#include "record.h"
#include "spsc.h"
#include "state_machine.h"
#include "vectors_lockin.h"

static void test_crc(void)
{
    const char *s = "123456789";
    CHECK(crc16_ccitt((const uint8_t *)s, 9) == 0x29B1u);
}

static void test_fmath(void)
{
    for (float x = 0.05f; x < 20.0f; x *= 1.37f) {
        CHECK_NEAR(fm_logf(x), logf(x), 2e-6 * fabs(logf(x)) + 1e-6);
    }
    for (float x = -20.0f; x < 20.0f; x += 0.77f) {
        CHECK_NEAR(fm_expf(x), expf(x), 2e-6 * expf(x));
    }
    CHECK_NEAR(fm_powf(0.3f, 0.190263f), powf(0.3f, 0.190263f), 1e-6);
    CHECK_NEAR(fm_powf(0.9f, 5.255876f), powf(0.9f, 5.255876f), 1e-6);
}

static void test_ms5611(void)
{
    /*MS5611-01BA03 datasheet example*/
    const uint16_t prom[8] = { 0, 40127, 36924, 23317, 23282, 33464, 28312, 0 };
    int32_t p, t;
    ms5611_compensate(prom, 9085466u, 8569150u, &p, &t);
    CHECK(t == 2007);
    CHECK(p == 100009);
    /*cold --> second order terms must lower the temperature even more*/
    int32_t p2, t2;
    ms5611_compensate(prom, 9085466u, 8000000u, &p2, &t2);
    CHECK(t2 < 2000);
}

static void test_lockin_vectors(void)
{
    for (int i = 0; i < N_LOCKIN_VECTORS; i++) {
        float x, y;
        uint16_t mn, mx;
        lockin_cycle(LOCKIN_VECTORS[i].s, &x, &y, &mn, &mx);
        CHECK_NEAR(x, LOCKIN_VECTORS[i].x, 1e-5 * fabs(LOCKIN_VECTORS[i].x) + 1e-3);
        CHECK_NEAR(y, LOCKIN_VECTORS[i].y, 1e-5 * fabs(LOCKIN_VECTORS[i].y) + 1e-3);
    }
}

static void test_lockin_dc_rejection(void)
{
    uint16_t s[M_SAMPLES];
    float x, y;
    uint16_t mn, mx;
    for (int i = 0; i < M_SAMPLES; i++) {
        s[i] = 40000;
    }
    lockin_cycle(s, &x, &y, &mn, &mx);
    CHECK_NEAR(x, 0.0, 0.02);
    CHECK_NEAR(y, 0.0, 0.02);
    CHECK(mn == 40000 && mx == 40000);
}

static void test_lockin_average(void)
{
    lockin_t l;
    lockin_out_t o;
    lockin_init(&l);
    int outputs = 0;
    for (int k = 0; k < K_BLOCKS; k++) {
        bool valid = (k != 3);
        if (lockin_push(&l, 10.0f, -4.0f, valid, valid ? 0 : RF_SETTLING, 1000, 2000, RANGE_HI, &o)) {
            outputs++;
        }
    }
    CHECK(outputs == 1);
    CHECK(o.n_valid == K_BLOCKS - 1);
    CHECK_NEAR(o.x, 10.0, 1e-6);
    CHECK_NEAR(o.y, -4.0, 1e-6);
    CHECK(o.cx[3] == INVALID_CYCLE && o.cx[0] == 10);
    CHECK(o.flags & RF_SETTLING);
}

static void test_lockin_range_change(void)
{
    /*a range switch inside a window must NOT mix HI and LO cycles*/
    lockin_t l;
    lockin_out_t o;
    lockin_init(&l);
    for (int k = 0; k < K_BLOCKS; k++) {
        uint8_t rg = (k < 8) ? RANGE_HI : RANGE_LO;
        float v = (k < 8) ? 1000.0f : 10.0f;
        if (lockin_push(&l, v, -v, true, 0, 1000, 2000, rg, &o)) {
            break;
        }
    }
    CHECK(o.range == RANGE_LO);
    CHECK(o.n_valid == K_BLOCKS - 8);
    CHECK_NEAR(o.x, 10.0, 1e-6);
    CHECK(o.cx[0] == INVALID_CYCLE && o.cx[7] == INVALID_CYCLE && o.cx[8] == 10);
}

static void test_range(void)
{
    range_ctl_t r;
    range_init(&r, RANGE_HI);
    range_result_t res = range_on_cycle(&r, 20000, 40000);
    CHECK(res.valid && !res.switched);
    res = range_on_cycle(&r, 0, 65520); /*rail 1*/
    CHECK(!res.valid && !res.switched && (res.flags & RF_SATURATED));
    res = range_on_cycle(&r, 0, 65520); /*rail 2 -> switch*/
    CHECK(res.switched && r.range == RANGE_LO);
    for (int i = 0; i < RANGE_SETTLE_CYCLES; i++) {
        res = range_on_cycle(&r, 30000, 35000);
        CHECK(!res.valid && (res.flags & RF_SETTLING));
    }
    res = range_on_cycle(&r, 30000, 35000);
    CHECK(res.valid);
    /*quiet outputs bring it back to HI*/
    bool back = false;
    for (int i = 0; i < RANGE_DOWN_OUTPUTS; i++) {
        back = range_on_output(&r, 50.0f, 50.0f, K_BLOCKS);
    }
    CHECK(back && r.range == RANGE_HI);
    /*a loud output resets the quiet count*/
    range_force(&r, RANGE_LO, true);
    for (int i = 0; i < RANGE_DOWN_OUTPUTS - 1; i++) {
        range_on_output(&r, 50.0f, 0.0f, K_BLOCKS);
    }
    CHECK(!range_on_output(&r, 500.0f, 0.0f, K_BLOCKS));
    CHECK(r.range == RANGE_LO);
}

static sm_inputs_t in_at(uint32_t t, int32_t acc, int32_t alt, int32_t vz, bool arm)
{
    sm_inputs_t in = { .now_ms = t, .accel_axial_mg = acc, .imu_ok = true,
                       .alt_cm = alt, .vz_cmps = vz, .baro_ok = true, .arm_request = arm };
    return in;
}

static void test_state_machine(void)
{
    sm_t sm;
    sm_inputs_t in;
    uint32_t t = 1000;
    sm_init(&sm, ST_SAFE, t);

    /*arming needs 500 ms of request without stopping*/
    for (; t < 1400; t += 10) { in = in_at(t, 1000, 0, 0, true); sm_step(&sm, &in); }
    CHECK(sm.state == ST_SAFE);
    for (; t < 1600; t += 10) { in = in_at(t, 1000, 0, 0, true); sm_step(&sm, &in); }
    CHECK(sm.state == ST_ARMED);

    /*a 200 ms bump (someone carrying it) must not trigger launch*/
    for (uint32_t e = t + 200; t < e; t += 10) { in = in_at(t, 5000, 0, 0, true); sm_step(&sm, &in); }
    for (uint32_t e = t + 100; t < e; t += 10) { in = in_at(t, 1000, 0, 0, true); sm_step(&sm, &in); }
    CHECK(sm.state == ST_ARMED);

    /*real launch!!*/
    uint32_t t_ign = t;
    for (uint32_t e = t + 400; t < e; t += 10) { in = in_at(t, 15000, 100, 1000, true); sm_step(&sm, &in); }
    CHECK(sm.state == ST_BOOST);
    CHECK(sm.t_launch_ms == t_ign);

    /*disarm request ignored in flight, burnout -> COAST*/
    for (uint32_t e = t + 300; t < e; t += 10) { in = in_at(t, -8000, 50000, 40000, false); sm_step(&sm, &in); }
    CHECK(sm.state == ST_COAST);

    /*climbing to 9000 m then falling 20 m --> apogee only after lockout and dwell*/
    int32_t alt = 50000;
    while (t - t_ign < 38000) { alt += 2400; in = in_at(t, -1000, alt, 1000, false); sm_step(&sm, &in); t += 10; }
    CHECK(sm.state == ST_COAST);
    for (uint32_t e = t + 1200; t < e; t += 10) { in = in_at(t, 0, alt - 2000, -500, false); sm_step(&sm, &in); }
    CHECK(sm.state == ST_DESCENT); /*APOGEE is a one step marker*/

    /*descent then landing after 10 s still*/
    for (uint32_t e = t + 5000; t < e; t += 10) { in = in_at(t, 1000, 100000, -3000, false); sm_step(&sm, &in); }
    CHECK(sm.state == ST_DESCENT);
    for (uint32_t e = t + 10100; t < e; t += 10) { in = in_at(t, 1000, 2, 0, false); sm_step(&sm, &in); }
    CHECK(sm.state == ST_LANDED);
    CHECK(!sm_motor_wanted(&sm));
    CHECK(!sm_fast_logging(&sm));
}

static void test_motor_policy(void)
{
    /*team decision 2026-10 D1: motor off on the pad, in BOOST and after landing*/
    static const bool motor[ST_COUNT] = { false, false, false, false, false, true, true, true, false };
    static const bool fast[ST_COUNT] = { false, false, false, false, true, true, true, true, false };
    sm_t sm;
    for (int s = 0; s < ST_COUNT; s++) {
        sm_init(&sm, (flight_state_t)s, 1);
        CHECK(sm_motor_wanted(&sm) == motor[s]);
        CHECK(sm_fast_logging(&sm) == fast[s]);
    }
}

static void test_deploy(void)
{
    deploy_t d;
    uint32_t t = 1000;

    /*nothing happens before COAST, whatever the time*/
    deploy_init(&d);
    CHECK(deploy_step(&d, ST_ARMED, false, t) == DEP_ACT_NONE);
    CHECK(deploy_step(&d, ST_BOOST, false, t + 60000) == DEP_ACT_NONE);
    CHECK(d.phase == DEP_STOWED);

    /*nominal: delay, extend, confirm, settle, ready*/
    t = 10000;
    CHECK(deploy_step(&d, ST_COAST, false, t) == DEP_ACT_NONE);
    CHECK(deploy_step(&d, ST_COAST, false, t + DEPLOY_DELAY_MS - 10) == DEP_ACT_NONE);
    CHECK(deploy_step(&d, ST_COAST, false, t + DEPLOY_DELAY_MS) == DEP_ACT_EXTEND);
    t += DEPLOY_DELAY_MS;
    CHECK(deploy_step(&d, ST_COAST, false, t + 100) == DEP_ACT_NONE);
    CHECK(deploy_step(&d, ST_COAST, true, t + 600) == DEP_ACT_CONFIRMED);
    CHECK(deploy_extended(&d) && !deploy_mill_ready(&d));
    CHECK(deploy_step(&d, ST_COAST, true, t + 600 + MILL_SETTLE_MS - 10) == DEP_ACT_NONE);
    CHECK(!deploy_mill_ready(&d));
    deploy_step(&d, ST_COAST, true, t + 600 + MILL_SETTLE_MS);
    CHECK(deploy_mill_ready(&d));
    /*never pulled back in, stays ready through landing*/
    deploy_step(&d, ST_LANDED, true, t + 300000);
    CHECK(deploy_mill_ready(&d));

    /*jammed --> no extension within the timeout -> FAILED, motor never allowed (╥﹏╥)*/
    deploy_init(&d);
    t = 5000;
    deploy_step(&d, ST_COAST, false, t);
    CHECK(deploy_step(&d, ST_COAST, false, t + DEPLOY_DELAY_MS) == DEP_ACT_EXTEND);
    t += DEPLOY_DELAY_MS;
    CHECK(deploy_step(&d, ST_COAST, false, t + DEPLOY_CONFIRM_TIMEOUT_MS - 10) == DEP_ACT_NONE);
    CHECK(deploy_step(&d, ST_COAST, false, t + DEPLOY_CONFIRM_TIMEOUT_MS) == DEP_ACT_FAILED);
    CHECK(deploy_step(&d, ST_DESCENT, true, t + 60000) == DEP_ACT_NONE);
    CHECK(!deploy_mill_ready(&d));

    /*came back after a reset with the mills already out --> settle only*/
    deploy_init(&d);
    CHECK(deploy_step(&d, ST_DESCENT, true, 2000) == DEP_ACT_CONFIRMED);
    deploy_step(&d, ST_DESCENT, true, 2000 + MILL_SETTLE_MS);
    CHECK(deploy_mill_ready(&d));

    /*landed before deploying (e.g. apogee/landing time-outs) --> never deploys*/
    deploy_init(&d);
    CHECK(deploy_step(&d, ST_LANDED, false, 900000) == DEP_ACT_NONE);
    CHECK(d.phase == DEP_STOWED);
}

static void test_state_machine_timeouts(void)
{
    /*sensors dead after launch --> time-outs must still get to LANDED*/
    sm_t sm;
    sm_inputs_t in;
    sm_init(&sm, ST_ARMED, 1);
    uint32_t t = 1;
    for (; t < 400; t += 10) { in = in_at(t, 12000, 0, 0, true); sm_step(&sm, &in); }
    CHECK(sm.state == ST_BOOST);
    for (; t < LANDED_TIMEOUT_MS + 2000; t += 10) {
        in = in_at(t, 0, 0, 0, true);
        in.imu_ok = false;
        in.baro_ok = false;
        sm_step(&sm, &in);
    }
    CHECK(sm.state == ST_LANDED);
}

static void test_state_machine_baro_launch(void)
{
    sm_t sm;
    sm_inputs_t in;
    sm_init(&sm, ST_ARMED, 1);
    uint32_t t = 1;
    for (; t < 2000; t += 10) {
        in = in_at(t, 0, (int32_t)t * 100, 0, true);
        in.imu_ok = false;
        sm_step(&sm, &in);
    }
    CHECK(sm.state == ST_COAST);
    CHECK(sm.launched);
}

static void test_record(void)
{
    science_rec_t r;
    memset(&r, 0, sizeof r);
    r.seq = 0x01020304u;
    r.t_ms = 123456;
    r.state = ST_COAST;
    r.range = RANGE_LO;
    r.flags = RF_SATURATED | RF_IMU_CLIP;
    r.x = 1.5f;
    r.y = -2.25f;
    r.pressure_pa = 25820;
    r.accel_mg[2] = -9876;
    r.cx[K_BLOCKS - 1] = -1234;
    r.t_launch_ms = 30000;
    uint8_t b[RECORD_SIZE];
    record_pack_science(&r, b);
    CHECK(record_check(b));
    CHECK(b[0] == 0x5A && b[1] == 0xA5 && b[2] == REC_TYPE_SCIENCE);
    CHECK(b[4] == 0x04 && b[7] == 0x01);
    float x;
    memcpy(&x, &b[16], 4);
    CHECK(x == 1.5f);
    CHECK((int16_t)(b[50] | (b[51] << 8)) == -9876);
    CHECK((int16_t)(b[60 + 2 * (K_BLOCKS - 1)] | (b[61 + 2 * (K_BLOCKS - 1)] << 8)) == -1234);
    b[100] ^= 1;
    CHECK(!record_check(b));

    event_rec_t e;
    memset(&e, 0, sizeof e);
    e.code = EV_STATE_CHANGE;
    strcpy(e.text, "BOOST");
    record_pack_event(&e, b);
    CHECK(record_check(b) && b[2] == REC_TYPE_EVENT && b[24] == 'B');

    header_rec_t h;
    memset(&h, 0, sizeof h);
    record_pack_header(&h, b);
    CHECK(record_check(b) && memcmp(&b[12], "GALVANPL", 8) == 0);
}

static void test_altitude(void)
{
    /*ISA: 910.8 hPa at 890 m, 258.2 hPa at 10153 m*/
    float h = altitude_from_pressure(25820.0f, 91080.0f);
    CHECK_NEAR(h, 10153.0 - 890.0, 25.0);
    CHECK_NEAR(altitude_from_pressure(91080.0f, 91080.0f), 0.0, 1e-3);

    altitude_t a;
    altitude_init(&a);
    for (uint32_t t = 1; t < 20000; t += 40) {
        altitude_update(&a, 91080.0f, t);
    }
    CHECK_NEAR(a.h_m, 0.0, 0.01);
    altitude_freeze_ref(&a);
    float p_ref = a.p_ref_pa;
    /*climb at 100 m/s for 10 s --> speed estimate converges*/
    for (uint32_t t = 20000; t < 30000; t += 40) {
        float hh = (float)(t - 20000) * 0.1f;
        float p = 91080.0f * powf(1.0f - hh / 44330.77f, 1.0f / 0.190263f);
        altitude_update(&a, p, t);
    }
    CHECK_NEAR(a.v_mps, 100.0, 3.0);
    CHECK(a.p_ref_pa == p_ref);
}

static void test_spsc(void)
{
    static spsc_t q;
    lockin_out_t o, r;
    memset(&o, 0, sizeof o);
    spsc_init(&q);
    for (uint32_t i = 0; i < SPSC_LEN; i++) {
        o.n_valid = (uint8_t)i;
        CHECK(spsc_push(&q, &o));
    }
    CHECK(!spsc_push(&q, &o));
    CHECK(q.dropped == 1);
    for (uint32_t i = 0; i < SPSC_LEN; i++) {
        CHECK(spsc_pop(&q, &r) && r.n_valid == i);
    }
    CHECK(!spsc_pop(&q, &r));
}

static float qnorm(const attitude_t *a)
{
    return a->q[0] * a->q[0] + a->q[1] * a->q[1] + a->q[2] * a->q[2] + a->q[3] * a->q[3];
}

static void test_attitude(void)
{
    attitude_t a;
    int16_t g0[3] = { 0, 0, 0 };

    /*vertical on the pad: +1 g along Z -> identity, tilt 0*/
    int16_t up[3] = { 0, 0, 1000 };
    attitude_init(&a);
    attitude_update(&a, up, g0, 0.01f);
    CHECK(a.init);
    CHECK(fabsf(a.q[0] - 1.0f) < 1e-5f);
    CHECK(attitude_tilt_deg(&a) < 0.1f);

    /*tilted 10 deg on the pad (a rail at 80 deg) --> tilt reads 10 deg*/
    int16_t tilt[3] = { 0, 174, 985 }; /*1000 * (sin 10, cos 10)*/
    attitude_init(&a);
    attitude_update(&a, tilt, g0, 0.01f);
    CHECK(fabsf(attitude_tilt_deg(&a) - 10.0f) < 0.2f);

    /*not set up under thrust (5 g) --> waits for a 1 g reading*/
    int16_t thrust[3] = { 0, 0, 5000 };
    attitude_init(&a);
    attitude_update(&a, thrust, g0, 0.01f);
    CHECK(!a.init);

    /*coasting (0 g): 90 dps roll about Z for 1 s = 90 deg, gyro only*/
    int16_t coast[3] = { 0, 0, 0 };
    int16_t roll[3] = { 0, 0, 900 };
    attitude_init(&a);
    attitude_update(&a, up, g0, 0.01f);
    for (int i = 0; i < 100; i++) {
        attitude_update(&a, coast, roll, 0.01f);
    }
    CHECK(fabsf(a.q[0] - 0.70711f) < 2e-3f); /*cos 45*/
    CHECK(fabsf(a.q[3] - 0.70711f) < 2e-3f); /*sin 45 about Z*/
    CHECK(attitude_tilt_deg(&a) < 0.1f); /*rolling doesnt tilt the axis*/
    CHECK(fabsf(qnorm(&a) - 1.0f) < 1e-5f);

    /*pitching 30 deg about X under thrust --> tilt follows the gyro*/
    int16_t pitch[3] = { 300, 0, 0 }; /*30 dps*/
    attitude_init(&a);
    attitude_update(&a, up, g0, 0.01f);
    for (int i = 0; i < 100; i++) {
        attitude_update(&a, thrust, pitch, 0.01f);
    }
    CHECK(fabsf(attitude_tilt_deg(&a) - 30.0f) < 0.3f);

    /*on the pad the accelerometer pulls a wrong tilt back to vertical*/
    for (int i = 0; i < 1000; i++) {
        attitude_update(&a, up, g0, 0.01f); /*10 s at ATT_KP = 1/s*/
    }
    CHECK(attitude_tilt_deg(&a) < 1.0f);
}

static void test_powerloss(void)
{
    nv_state_t nv;
    pl_inputs_t in;
    memset(&nv, 0, sizeof nv);
    nv.flags = NV_ARMED;
    nv.pad_p_pa = 88000.0f; /*pad at about 1,200 m*/
    nv_seal(&nv);
    CHECK(nv_check(&nv));
    nv.pad_p_pa += 1.0f; /*corrupted after sealing*/
    CHECK(!nv_check(&nv));
    nv.pad_p_pa -= 1.0f;
    CHECK(nv_check(&nv));

    memset(&in, 0, sizeof in);
    in.nv_valid = true;
    in.nv = nv;
    in.baro_ok = true;
    in.imu_ok = true;
    in.accel_axial_mg = 1000; /*resting on the pad: 1 g*/

    in.p_now_pa = 87950.0f; /*about 4 m --> still on the pad*/
    CHECK(powerloss_decide(&in) == PL_PAD);
    in.p_now_pa = 60000.0f; /*about 3 km up, coasting*/
    in.accel_axial_mg = -200;
    CHECK(powerloss_decide(&in) == PL_IN_FLIGHT);
    in.accel_axial_mg = 6000; /*high and still burning*/
    CHECK(powerloss_decide(&in) == PL_BOOST);

    in.nv.flags = NV_ARMED | NV_LAUNCHED;
    nv_seal(&in.nv);
    in.p_now_pa = 87900.0f; /*back near pad level after launch*/
    in.accel_axial_mg = 1000;
    CHECK(powerloss_decide(&in) == PL_LANDED);
    in.accel_axial_mg = 8000; /*low but burning --> just launched*/
    CHECK(powerloss_decide(&in) == PL_BOOST);

    in.baro_ok = false; /*no barometer --> stay safe*/
    CHECK(powerloss_decide(&in) == PL_UNKNOWN);
    in.baro_ok = true;
    in.nv_valid = false; /*nothing in flash*/
    CHECK(powerloss_decide(&in) == PL_UNKNOWN);
}

int main(void)
{
    test_crc();
    test_fmath();
    test_ms5611();
    test_lockin_vectors();
    test_lockin_dc_rejection();
    test_lockin_average();
    test_lockin_range_change();
    test_range();
    test_state_machine();
    test_state_machine_timeouts();
    test_motor_policy();
    test_deploy();
    test_state_machine_baro_launch();
    test_record();
    test_altitude();
    test_spsc();
    test_powerloss();
    test_attitude();
    return CHECK_SUMMARY("test_unit");
}
