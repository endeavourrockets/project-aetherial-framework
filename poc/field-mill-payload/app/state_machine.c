/*
 *state_machine.c, see state_machine.h
 *every transition needs a condition to hold continuously for a dwell
 *time, like this a single noisy sample cannot change state
 *time-outs guarantee also make sure it doesnt get stuck in a state forever if a sensor fails 
 */
#include "state_machine.h"
#include "config.h"

#define ARM_DEBOUNCE_MS   500u

static const char *const NAMES[ST_COUNT] = {
    "INIT", "SELFTEST", "SAFE", "ARMED", "BOOST", "COAST", "APOGEE", "DESCENT", "LANDED"
};

const char *sm_state_name(flight_state_t s)
{
    return (s < ST_COUNT) ? NAMES[s] : "?";
}

/*track how long 'cond' has been continuously true. 'now' is never 0 in
 *use (the tick counter starts at 1 ms or later), so 0 means "false". 
 */
static bool held(uint32_t *t_since, bool cond, uint32_t now, uint32_t dwell)
{
    if (!cond) {
        *t_since = 0;
        return false;
    }
    if (*t_since == 0) {
        *t_since = now ? now : 1u;
    }
    return (now - *t_since) >= dwell;
}

static void clear_timers(sm_t *sm)
{
    sm->t_arm_cond = sm->t_disarm_cond = sm->t_launch_cond = sm->t_baro_launch_cond = 0;
    sm->t_burnout_cond = sm->t_apogee_cond = sm->t_landed_cond = 0;
}

static void enter(sm_t *sm, flight_state_t s, uint32_t now)
{
    sm->state = s;
    sm->t_state_ms = now;
    clear_timers(sm);
}

void sm_init(sm_t *sm, flight_state_t start, uint32_t now_ms)
{
    sm->launched = false;
    sm->t_launch_ms = 0;
    sm->alt_max_cm = 0;
    enter(sm, start, now_ms);
}

void sm_resume(sm_t *sm, flight_state_t state, uint32_t ms_since_launch,
               int32_t alt_max_cm, uint32_t now_ms)
{
    sm_init(sm, state, now_ms);
    sm->launched = true;
    sm->t_launch_ms = now_ms - ms_since_launch;
    sm->alt_max_cm = alt_max_cm;
}

bool sm_step(sm_t *sm, const sm_inputs_t *in)
{
    const uint32_t now = in->now_ms;
    const flight_state_t old = sm->state;
    const uint32_t since_launch = sm->launched ? now - sm->t_launch_ms : 0;

    switch (sm->state) {
    case ST_SAFE:
        if (held(&sm->t_arm_cond, in->arm_request, now, ARM_DEBOUNCE_MS)) {
            enter(sm, ST_ARMED, now);
        }
        break;

    case ST_ARMED: {
        if (held(&sm->t_disarm_cond, !in->arm_request, now, ARM_DEBOUNCE_MS)) {
            enter(sm, ST_SAFE, now);
            break;
        }
        bool accel = in->imu_ok && in->accel_axial_mg >= LAUNCH_ACCEL_MG;
        bool baro = in->baro_ok && in->alt_cm >= LAUNCH_BARO_RISE_CM;
        bool by_accel = held(&sm->t_launch_cond, accel, now, LAUNCH_DWELL_MS);
        bool by_baro = held(&sm->t_baro_launch_cond, baro, now, 500u);
        if (by_accel || by_baro) {
            uint32_t t0 = by_accel ? sm->t_launch_cond : sm->t_baro_launch_cond;
            sm->launched = true;
            sm->t_launch_ms = t0;
            sm->alt_max_cm = in->alt_cm;
            /*a baro only detection is late: the burn is probably over*/
            enter(sm, by_accel ? ST_BOOST : ST_COAST, now);
        }
        break;
    }

    case ST_BOOST: {
        bool burnout = in->imu_ok && in->accel_axial_mg < BURNOUT_ACCEL_MG;
        if (held(&sm->t_burnout_cond, burnout, now, BURNOUT_DWELL_MS) ||
            since_launch >= BOOST_MAX_MS) {
            enter(sm, ST_COAST, now);
        }
        break;
    }

    case ST_COAST: {
        if (in->baro_ok && in->alt_cm > sm->alt_max_cm) {
            sm->alt_max_cm = in->alt_cm;
        }
        bool falling = in->baro_ok && since_launch >= APOGEE_LOCKOUT_MS &&
                       (sm->alt_max_cm - in->alt_cm) >= APOGEE_DESCENT_CM;
        if (held(&sm->t_apogee_cond, falling, now, APOGEE_DWELL_MS) ||
            since_launch >= APOGEE_TIMEOUT_MS) {
            enter(sm, ST_APOGEE, now);
        }
        break;
    }

    case ST_APOGEE:
        enter(sm, ST_DESCENT, now); /*one step marker state*/
        break;

    case ST_DESCENT: {
        int32_t v = in->vz_cmps < 0 ? -in->vz_cmps : in->vz_cmps;
        bool still = in->baro_ok && v < LANDED_VSPEED_CMPS;
        if (held(&sm->t_landed_cond, still, now, LANDED_DWELL_MS) ||
            since_launch >= LANDED_TIMEOUT_MS) {
            enter(sm, ST_LANDED, now);
        }
        break;
    }

    case ST_LANDED:
    case ST_INIT:
    case ST_SELFTEST:
    default:
        break;
    }
    return sm->state != old;
}

/*The motor may only run with the mills deployed, i.e. from COAST until
 *landing. payload.c additionally waits for the
 *deploy sequence (deploy.h) before starting it. Off in SAFE, ARMED, BOOST
 *and LANDED*/
bool sm_motor_wanted(const sm_t *sm)
{
    switch (sm->state) {
    case ST_COAST:
    case ST_APOGEE:
    case ST_DESCENT:
        return true;
    default:
        return false;
    }
}

/*10 Hz records in flight, including BOOST (sensor-only, motor off)*/
bool sm_fast_logging(const sm_t *sm)
{
    return sm->state == ST_BOOST || sm_motor_wanted(sm);
}
