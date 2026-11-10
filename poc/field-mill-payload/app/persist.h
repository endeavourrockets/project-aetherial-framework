/*
 * persist.h - state that survives a watchdog or brown-out reset (but not a
 * power loss). Kept in a .noinit RAM section that the startup code does
 * not clear, protected by a magic word and a CRC.
 *
 * If the MCU resets in flight, the payload resumes in the saved state
 * instead of waiting on the pad in SAFE.
 */
#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t boot_count;
    uint8_t  state;               /* flight_state_t */
    uint8_t  pad[3];
    uint32_t ms_since_launch;
    int32_t  alt_max_cm;
    float    p_ref_pa;
    uint16_t crc;
    uint16_t pad2;
} persist_t;

/* Validate the saved block. Returns true if it is intact. Always bumps the
 * boot counter (reinitialising the block if it was not intact). */
bool persist_load(persist_t *out);
void persist_save(uint8_t state, uint32_t ms_since_launch, int32_t alt_max_cm, float p_ref_pa);
void persist_clear(void);

#endif /* PERSIST_H */
