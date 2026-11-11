/*
 * payload.h --> top level of the field mill payload app
 *
 * main() calls payload_init() once, then payload_loop_once() forever and ever
 * the ADC driver calls payload_adc_cycle_isr() once every modulation cycle
 */
#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>
#include "state_machine.h"

/*self test fault bits (go in the header record and the EV_SELFTEST event)*/
#define STF_IMU (1u << 0)
#define STF_BARO (1u << 1)
#define STF_STORAGE (1u << 2)
#define STF_VBAT (1u << 3)
#define STF_ADC_MID (1u << 4)
#define STF_MOTOR (1u << 5)
#define STF_INDEX (1u << 6)
#define STF_NO_SPIN (1u << 7) /*spin skipped (SELFTEST_SPIN_RETRACTED 0) --> motor, index and mid-rail not tested*/

#define SELFTEST_SPIN_MS 2000u /*with the mills retracted*/

typedef struct {
    uint32_t outputs; /*lock-in outputs used up*/
    uint32_t science_records;
    uint32_t event_records;
    uint32_t range_switches;
    uint32_t storage_errors;
    uint16_t selftest_faults;
    bool resumed;
} payload_stats_t;

void payload_init(void);
void payload_loop_once(void);
void payload_adc_cycle_isr(const uint16_t *samples);

flight_state_t payload_state(void);
const payload_stats_t *payload_stats(void);

#endif /*PAYLOAD_H*/
