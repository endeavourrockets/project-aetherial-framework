/*
 *logger.c, see logger.h.
 */
#include "logger.h"
#include "config.h"
#include "drivers.h"

bool logger_init(logger_t *lg)
{
    lg->fill = 0;
    lg->seq = 0;
    lg->errors = 0;
    lg->written = 0;
    lg->retried = false;
    lg->t_last_sync = 0;
    lg->name[0] = '\0';
    lg->ok = drv_storage_init() && drv_storage_open_new(lg->name);
    return lg->ok;
}

uint32_t logger_next_seq(logger_t *lg)
{
    return lg->seq++;
}

static void write_sector(logger_t *lg)
{
    if (!lg->ok) {
        return;
    }
    lg->retried = false;
    if (!drv_storage_write(lg->sector, sizeof lg->sector)) {
        lg->retried = true;
        if (!drv_storage_write(lg->sector, sizeof lg->sector)) {
            lg->errors++;
            return;
        }
    }
    lg->written += 2;
}

void logger_put(logger_t *lg, const uint8_t rec[RECORD_SIZE])
{
    uint8_t *dst = &lg->sector[lg->fill * RECORD_SIZE];
    for (uint32_t i = 0; i < RECORD_SIZE; i++) {
        dst[i] = rec[i];
    }
    if (++lg->fill == 2) {
        write_sector(lg);
        lg->fill = 0;
    }
}

void logger_flush(logger_t *lg)
{
    if (lg->fill == 1) {
        /*fill rest of sector with zeros, decoder skips records without the sync word anyway*/
        for (uint32_t i = RECORD_SIZE; i < 2 * RECORD_SIZE; i++) {
            lg->sector[i] = 0;
        }
        write_sector(lg);
        lg->written--;
        lg->fill = 0;
    }
    if (lg->ok && !drv_storage_sync()) {
        lg->errors++;
    }
}

void logger_tick(logger_t *lg, uint32_t now_ms)
{
    if (now_ms - lg->t_last_sync >= SYNC_PERIOD_MS) {
        lg->t_last_sync = now_ms;
        if (lg->ok && !drv_storage_sync()) {
            lg->errors++;
        }
    }
}
