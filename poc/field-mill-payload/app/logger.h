/*
 * logger.h - packs records two per 512-byte sector and writes them to
 * storage. A failed write is retried once; persistent failure is counted
 * and reported, and the payload keeps running.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include "record.h"

typedef struct {
    uint8_t  sector[2 * RECORD_SIZE];
    uint8_t  fill;                /* records in sector (0 or 1) */
    bool     ok;                  /* storage opened */
    uint32_t seq;                 /* next record sequence number */
    uint32_t errors;
    uint32_t written;             /* records written */
    bool     retried;             /* last write needed a retry */
    uint32_t t_last_sync;
    char     name[13];
} logger_t;

bool logger_init(logger_t *lg);
uint32_t logger_next_seq(logger_t *lg);
void logger_put(logger_t *lg, const uint8_t rec[RECORD_SIZE]);
void logger_tick(logger_t *lg, uint32_t now_ms);
void logger_flush(logger_t *lg);

#endif /* LOGGER_H */

