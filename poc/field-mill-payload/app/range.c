/*
 * range.c - see range.h.
 */
#include "range.h"
#include "config.h"
#include "record.h"

static void do_switch(range_ctl_t *r, uint8_t to)
{
    r->range = to;
    r->settle = RANGE_SETTLE_CYCLES;
    r->near_rail = 0;
    r->quiet = 0;
}

void range_init(range_ctl_t *r, uint8_t start_range)
{
    r->range = start_range;
    r->near_rail = 0;
    r->settle = 0;
    r->quiet = 0;
    r->auto_enabled = true;
}

void range_force(range_ctl_t *r, uint8_t range, bool auto_enabled)
{
    if (range != r->range) {
        do_switch(r, range);
    }
    r->auto_enabled = auto_enabled;
}

range_result_t range_on_cycle(range_ctl_t *r, uint16_t mn, uint16_t mx)
{
    range_result_t res = { .valid = true, .flags = 0, .switched = false };
    bool rail = (mn <= ADC_RAIL_MARGIN) || (mx >= ADC_FULL_SCALE - ADC_RAIL_MARGIN);

    if (r->settle > 0) {
        r->settle--;
        res.valid = false;
        res.flags |= RF_SETTLING;
        return res;
    }
    if (rail) {
        res.valid = false;
        res.flags |= RF_SATURATED;
        if (r->range == RANGE_HI && r->auto_enabled) {
            if (++r->near_rail >= RANGE_UP_CYCLES) {
                do_switch(r, RANGE_LO);
                res.switched = true;
            }
        }
    } else {
        r->near_rail = 0;
    }
    return res;
}

bool range_on_output(range_ctl_t *r, float x, float y, uint8_t n_valid)
{
    if (r->range != RANGE_LO || !r->auto_enabled || n_valid == 0) {
        r->quiet = 0;
        return false;
    }
    float a2 = x * x + y * y;
    if (a2 < RANGE_DOWN_AMPL_COUNTS * RANGE_DOWN_AMPL_COUNTS) {
        if (++r->quiet >= RANGE_DOWN_OUTPUTS) {
            do_switch(r, RANGE_HI);
            return true;
        }
    } else {
        r->quiet = 0;
    }
    return false;
}
