/*
 * persist.c - see persist.h. On the host the section attribute is ignored
 * and the block behaves as an ordinary global.
 */
#include "persist.h"
#include "crc16.h"

#define PERSIST_MAGIC 0x47414C56u   /* "GALV" */

#if defined(__arm__)
__attribute__((section(".noinit")))
#endif
static persist_t g_persist;

static uint16_t calc_crc(const persist_t *p)
{
    return crc16_ccitt((const uint8_t *)p, (uint32_t)((const uint8_t *)&p->crc - (const uint8_t *)p));
}

bool persist_load(persist_t *out)
{
    bool ok = g_persist.magic == PERSIST_MAGIC && g_persist.crc == calc_crc(&g_persist);
    if (!ok) {
        g_persist.magic = PERSIST_MAGIC;
        g_persist.boot_count = 0;
        g_persist.state = 0;
        g_persist.ms_since_launch = 0;
        g_persist.alt_max_cm = 0;
        g_persist.p_ref_pa = 0.0f;
    }
    g_persist.boot_count++;
    g_persist.crc = calc_crc(&g_persist);
    *out = g_persist;
    return ok;
}

void persist_save(uint8_t state, uint32_t ms_since_launch, int32_t alt_max_cm, float p_ref_pa)
{
    g_persist.magic = PERSIST_MAGIC;
    g_persist.state = state;
    g_persist.ms_since_launch = ms_since_launch;
    g_persist.alt_max_cm = alt_max_cm;
    g_persist.p_ref_pa = p_ref_pa;
    g_persist.crc = calc_crc(&g_persist);
}

void persist_clear(void)
{
    g_persist.magic = 0;
}
