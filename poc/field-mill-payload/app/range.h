/*
 * range.h - automatic HI LO range selection for the charge amplifier, assumed by me
 * HI -> LO: RANGE_UP_CYCLES consecutive cycles within ADC_RAIL_MARGIN of a rail
 * LO -> HI: RANGE_DOWN_OUTPUTS consecutive outputs with amplitude below
 * RANGE_DOWN_AMPL_COUNTS (the HI range would sit below half scale)
 * After any switch, RANGE_SETTLE_CYCLES cycles are marked invalid while the (っ- ‸ - ς)
 * amplifier settles, or should settle.
 */
#ifndef RANGE_H
#define RANGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t range;
    uint8_t near_rail;
    uint8_t settle;
    uint8_t quiet;
    bool auto_enabled;
} range_ctl_t;

typedef struct {
    bool valid;/* cycle may be averaged... */
    uint16_t flags;/* RF_SATURATED and/or RF_SETTLING */
    bool switched;/* range changed, this is where caller drives the switch */
} range_result_t;

void range_init(range_ctl_t *r, uint8_t start_range);
range_result_t range_on_cycle(range_ctl_t *r, uint16_t mn, uint16_t mx);
bool range_on_output(range_ctl_t *r, float x, float y, uint8_t n_valid);
void range_force(range_ctl_t *r, uint8_t range, bool auto_enabled);

#endif /* RANGE_H */
/* HI davide!!!*/
