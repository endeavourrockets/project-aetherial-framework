/*
 * deploy.c --> see deploy.h
 */
#include "deploy.h"
#include "config.h"

static const char *const NAMES[] = { "STOWED", "EXTENDING", "SETTLING", "READY", "FAILED" };

const char *deploy_phase_name(deploy_phase_t p)
{
    return (p <= DEP_FAILED) ? NAMES[p] : "?";
}

static void go(deploy_t *d, deploy_phase_t p, uint32_t now)
{
    d->phase = p;
    d->t_phase = now;
}

void deploy_init(deploy_t *d)
{
    d->phase = DEP_STOWED;
    d->t_phase = 0;
    d->t_window = 0;
}

/*deploying is allowed from COAST until landing. if a reset resumes in APOGEE
 * or DESCENT the delay counts from the first step after the reset*/
static bool window(flight_state_t s)
{
    return s == ST_COAST || s == ST_APOGEE || s == ST_DESCENT;
}

deploy_action_t deploy_step(deploy_t *d, flight_state_t state, bool extended, uint32_t now)
{
    switch (d->phase) {
    case DEP_STOWED:
        if (!window(state)) {
            d->t_window = 0;
            break;
        }
        if (extended) {
            /*already out (reset mid flight) --> only wait the settle time*/
            go(d, DEP_SETTLING, now);
            return DEP_ACT_CONFIRMED;
        }
        if (d->t_window == 0) {
            d->t_window = now ? now : 1u;
        }
        if (now - d->t_window >= DEPLOY_DELAY_MS) {
            go(d, DEP_EXTENDING, now);
            return DEP_ACT_EXTEND;
        }
        break;

    case DEP_EXTENDING:
        if (extended) {
            go(d, DEP_SETTLING, now);
            return DEP_ACT_CONFIRMED;
        }
        if (now - d->t_phase >= DEPLOY_CONFIRM_TIMEOUT_MS) {
            go(d, DEP_FAILED, now);
            return DEP_ACT_FAILED;
        }
        break;

    case DEP_SETTLING:
        if (now - d->t_phase >= MILL_SETTLE_MS) {
            go(d, DEP_READY, now);
        }
        break;

    case DEP_READY:
    case DEP_FAILED:
    default:
        break;
    }
    return DEP_ACT_NONE;
}

bool deploy_mill_ready(const deploy_t *d)
{
    return d->phase == DEP_READY;
}

bool deploy_extended(const deploy_t *d)
{
    return d->phase == DEP_SETTLING || d->phase == DEP_READY;
}
