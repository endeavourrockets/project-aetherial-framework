/*
 * persist.h --> state that survives a watchdog or brown out reset (NOT a full
 * power loss tho). kept in a .noinit RAM section that the startup code doesnt
 * clear, protected by a magic word and a CRC
 *
 * if the MCU resets mid flight the payload carries on in the saved state
 * instead of sitting on the pad in SAFE like an idiot
 */
#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t boot_count;
    uint8_t state; /*flight_state_t*/
    uint8_t pad[3];
    uint32_t ms_since_launch;
    int32_t alt_max_cm;
    float p_ref_pa;
    uint16_t crc;
    uint16_t pad2;
} persist_t;

/*checks the saved block, true if its fine. always bumps the boot counter
 * (and resets the block if it wasnt fine)*/
bool persist_load(persist_t *out);
void persist_save(uint8_t state, uint32_t ms_since_launch, int32_t alt_max_cm, float p_ref_pa);
void persist_clear(void);

#endif /*PERSIST_H*/
