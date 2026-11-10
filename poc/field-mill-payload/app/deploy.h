/*
 * deploy.h - mill deployment sequence (team decision 2026-10 D1).
 *
 *   STOWED -> (in COAST for DEPLOY_DELAY_MS) EXTENDING -> (extended) SETTLING
 *          -> (MILL_SETTLE_MS) READY: motor may run
 *   EXTENDING -> (not extended after DEPLOY_CONFIRM_TIMEOUT_MS) FAILED: motor stays off
 *
 * Pure logic: the caller passes the flight state and the actuator's
 * "extended" reading, and acts on the returned action. The mills are never
 * retracted in flight.
 */
#ifndef DEPLOY_H
#define DEPLOY_H

#include <stdbool.h>
#include <stdint.h>
#include "state_machine.h"

typedef enum {
    DEP_STOWED = 0,
    DEP_EXTENDING,
    DEP_SETTLING,
    DEP_READY,
    DEP_FAILED
} deploy_phase_t;

typedef enum {
    DEP_ACT_NONE = 0,
    DEP_ACT_EXTEND,               /* call drv_deploy_extend() */
    DEP_ACT_CONFIRMED,            /* extension seen: log it */
    DEP_ACT_FAILED                /* not extended in time: log it, motor stays off */
} deploy_action_t;

typedef struct {
    deploy_phase_t phase;
    uint32_t t_phase;             /* entry time of the current phase */
    uint32_t t_window;            /* first step in a deploy-allowed state, 0 = not yet */
} deploy_t;

void deploy_init(deploy_t *d);
deploy_action_t deploy_step(deploy_t *d, flight_state_t state, bool extended, uint32_t now_ms);
/* Mills out and settled: the motor may run. */
bool deploy_mill_ready(const deploy_t *d);
/* Extension commanded and seen (SETTLING or READY). */
bool deploy_extended(const deploy_t *d);
const char *deploy_phase_name(deploy_phase_t p);

#endif /* DEPLOY_H */
