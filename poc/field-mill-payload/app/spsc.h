/*
 * spsc.h - single-producer single-consumer queue of lock-in outputs.
 * Producer: ADC DMA interrupt. Consumer: main loop. No locks needed:
 * each index is written by one side only, with a barrier before publishing.
 */
#ifndef SPSC_H
#define SPSC_H

#include <stdbool.h>
#include <stdint.h>
#include "lockin.h"

#define SPSC_LEN 16u                 /* power of two; 1.6 s of outputs */

#if defined(__arm__)
#define SPSC_BARRIER() __asm volatile("dmb" ::: "memory")
#else
#define SPSC_BARRIER() __asm volatile("" ::: "memory")
#endif

typedef struct {
    lockin_out_t buf[SPSC_LEN];
    volatile uint32_t head;          /* written by producer */
    volatile uint32_t tail;          /* written by consumer */
    volatile uint32_t dropped;       /* producer: outputs lost when full */
} spsc_t;

static inline void spsc_init(spsc_t *q)
{
    q->head = q->tail = q->dropped = 0;
}

static inline bool spsc_push(spsc_t *q, const lockin_out_t *v)
{
    uint32_t h = q->head;
    if (h - q->tail >= SPSC_LEN) {
        q->dropped++;
        return false;
    }
    q->buf[h & (SPSC_LEN - 1u)] = *v;
    SPSC_BARRIER();
    q->head = h + 1u;
    return true;
}

static inline bool spsc_pop(spsc_t *q, lockin_out_t *v)
{
    uint32_t t = q->tail;
    if (t == q->head) {
        return false;
    }
    SPSC_BARRIER();
    *v = q->buf[t & (SPSC_LEN - 1u)];
    SPSC_BARRIER();
    q->tail = t + 1u;
    return true;
}

#endif /* SPSC_H */
