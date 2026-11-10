/*
 * lockin.h - synchronous (lock-in) demodulation of the field mill signal.
 *
 * one call to lockin_cyle() per modulation cycle (M_SAMPLES phase locked
 * to rotor in harware). lockin_push() averages K_BLOCKS valid cycles into
 * one output.
 * ( •᷄ᯅ•᷅ )
 * THe way I understand it, the field mill rotor conver & uncover the sense 
 * plates, turning the DC field into an AC signal of a known freq.
 * THe amp is field stregth, and the phase is the sign fo field.
 * by multiplying the signal by a wave of same frey and phase, would negate
 * noise and dc offset sum.
 * 
 * phase locked, so sample i of every cycle is at the same rotor angle so it 
 * always lines up with LUT_SIN[i] and LUT_COS[i].
 * 
 * 2 main stages: 
 * lockin_cycle()-->1 cycle -->1(x, y)-->(in the ADC ISR)
 * lockin_push()-->K_BLOCKS cycles-->1 averaged output
 */
#ifndef LOCKIN_H
#define LOCKIN_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h" /*def of M_SAMPLES, K_BLOCKS*/

/*function to see one window every K_BLOCKS cycles, into the log*/
typedef struct {
    float    x, y; /*mean of valid cycles, counts units*/
    uint8_t  n_valid; /*number of K_BLOCKS cycles*/
    uint8_t  range; /*front end range, if it HI or LO*/
    uint16_t flags; /* RF_SATURATED, RF_SETTLING from record.h */
    uint16_t raw_min, raw_max; /*max & min in ADC sample--> can show saturation*/
    int16_t  cx[K_BLOCKS], cy[K_BLOCKS]; /*cycle x,y rounded to int16*/
} lockin_out_t;

/*working state of window being filled, kept between calls*/

typedef struct {
    float    acc_x, acc_y; /*run SUM of vlaid x,y*/
    uint8_t  acc_range; /* range of the cycles in acc_x, acc_y */
    uint8_t  n_seen, n_valid; /*cycles in window*/
    uint16_t flags;
    uint16_t raw_min, raw_max; /*the raw extremes*/
    int16_t  cx[K_BLOCKS], cy[K_BLOCKS]; /*per cycle values*/
} lockin_t;

/*to clear state, would need ot call once before first lockin_push() */
void lockin_init(lockin_t *l);


/* Demodulate one cycle. Pure function, safe in an ISR.   :Þ  */
void lockin_cycle(const uint16_t *s, float *x, float *y, uint16_t *mn, uint16_t *mx);

/* Add one cycle-->valid false excludes it from the average (settling, ദ്ദി ˉ꒳ˉ )✧
 * saturation, motor not at speed). Returns true--> fills K_BLOCKS cycles 
 * Cycles from different ranges are not averaged together
 * differetn range = different gain, mixing is useless: if the range
 * changes inside a window, the earlier cycles of
 * that window are dropped (their slots become INVALID_CYCLE).*/

bool lockin_push(lockin_t *l, float x, float y, bool valid, uint16_t cycle_flags,
                 uint16_t mn, uint16_t mx, uint8_t range, lockin_out_t *out);

#endif /* LOCKIN_H */

/*and then she went to sleep 😴😪🥱💤🛌🏼*/