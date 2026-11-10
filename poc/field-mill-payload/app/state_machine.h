/*
 * state_machine.h - payload flight state machine (XLR-PAY-002 section 5).
 *
 *   INIT -> SELFTEST -> SAFE <-> ARMED -> BOOST -> COAST -> APOGEE
 *        -> DESCENT -> LANDED
 *
 * INIT and SELFTEST are driven by payload.c; this module owns SAFE onwards.
 * The payload never commands anything on the vehicle: the states only
 * change what the payload measures, how often it logs and when its own
 * mills deploy (deploy.h).
 */
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ST_INIT = 0,
    ST_SELFTEST,
    ST_SAFE,
    ST_ARMED,
    ST_BOOST,
    ST_COAST,
    ST_APOGEE,
    ST_DESCENT,
    ST_LANDED,
    ST_COUNT
} flight_state_t;

typedef struct {
    uint32_t now_ms;
    int32_t  accel_axial_mg;      /* specific force along the body axis */
    bool     imu_ok;
    int32_t  alt_cm;              /* filtered altitude above the pad */
    int32_t  vz_cmps;             /* filtered vertical speed */
    bool     baro_ok;
    bool     arm_request;         /* switch armed OR console ARM */
} sm_inputs_t;

typedef struct {
    flight_state_t state;
    uint32_t t_state_ms;          /* time of entry into the current state */
    uint32_t t_launch_ms;         /* first sample of the launch condition */
    bool     launched;
    int32_t  alt_max_cm;
    /* condition timers: time at which a condition first became true, 0 = false */
    uint32_t t_arm_cond, t_disarm_cond, t_launch_cond, t_baro_launch_cond;
    uint32_t t_burnout_cond, t_apogee_cond, t_landed_cond;
} sm_t;

void sm_init(sm_t *sm, flight_state_t start, uint32_t now_ms);
/* Resume after a reset in flight: state and time since launch are restored. */
void sm_resume(sm_t *sm, flight_state_t state, uint32_t ms_since_launch,
               int32_t alt_max_cm, uint32_t now_ms);
/* Advance the machine. Returns true if the state changed. */
bool sm_step(sm_t *sm, const sm_inputs_t *in);
/* May the shutter motor run in this state (once the mills are deployed)? */
bool sm_motor_wanted(const sm_t *sm);
/* Full-rate (10 Hz) logging wanted? Otherwise 1 Hz. */
bool sm_fast_logging(const sm_t *sm);
const char *sm_state_name(flight_state_t s);

#endif /* STATE_MACHINE_H */
