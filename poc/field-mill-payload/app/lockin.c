/*
 * lockin.c --> see lockin.h
 * the maths order matches my python version of the field mill sim (not in here)
 * so the test vectors agree to float32 rounding (build with -ffp-contract=off)
 * phase would be locked to the rotor in hardware
 */
#include "lockin.h"
#include "lut_sincos.h" /*LUT_SIN[], LUT_COS[], LUT_LEN from tools n shit*/
#include "record.h" /*INVALID_CYCLES, and flagging*/

/*time checks, sin/cos tables must have one entry per sample
*changes in M_SAMPLES must be reflected in the LUT_SIN, LUT_COS tables
*so would need ot regenerate tables... (˶°ㅁ°) !!
* compile time check
*/
#if LUT_LEN != M_SAMPLES
#error "lut_sincos.h does not match M_SAMPLES: run tools/gen_lut.py"
#endif

/*n_seen / n_valid are uint8_t and index cx[]/cy[], so K_BLOCKS must fit*/
#if K_BLOCKS > 255
#error "K_BLOCKS must fit in uint8_t"
#endif

/*Round a float to the nearest int16_t (half away from zero), with saturation.
* --> lower limit is -32767, NOT INT16_MIN (-32768), this value kept
*for INVALID_CYCLE, so a real measurement never called
*invalid marker in telemetry.
*/
static int16_t clamp_i16(float v)
{
    if (v > 32767.0f) {
        return 32767;
    }
    if (v < -32767.0f) {
        return -32767;
    }
    return (int16_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

/*clar the accumulators at start of new averaging window
*acc_range not reset, read only when n_valid>0
*/
static void reset_window(lockin_t *l)
{
    l->acc_x = 0.0f;
    l->acc_y = 0.0f;
    l->n_seen = 0;
    l->n_valid = 0;
    l->flags = 0;
    l->raw_min = 0xFFFFu; /*start at max, first sample replaces it*/
    l->raw_max = 0;
}

/*initilise lock-in instance, and callbefoer first lockin_push()*/
void lockin_init(lockin_t *l)
{
    reset_window(l);
}

/*Demodulate one rotor revolution
*param[in] s M_SAMPLES raw ADC counts, sample 0 at the rotor index mark
*param[out] x in-phase amplitude, ADC counts (peak)
*param[out] y quadrature amplitude, ADC counts (peak)
*param[out] mn smallest raw sample in this revolution (for saturation checks)
*param[out] mx largest raw sample in this revolution
*
*The 2/N factor scales the correlation sums to peak amplitude
*The order of operations must match my python sim (see file header, lockin.h)
*this above, is something i needed i was getting lost in the sauce
*/

void lockin_cycle(const uint16_t *s, float *x, float *y, uint16_t *mn, uint16_t *mx)
{
    float sx = 0.0f, sy = 0.0f;
    uint16_t lo = 0xFFFFu, hi = 0;
    for (int i = 0; i < M_SAMPLES; i++) {
        float v = (float)s[i];
        sx += v * LUT_SIN[i];
        sy += v * LUT_COS[i];
        if (s[i] < lo) {
            lo = s[i];
        }
        if (s[i] > hi) {
            hi = s[i];
        }
    }
    *x = 2.0f * sx / (float)M_SAMPLES;
    *y = 2.0f * sy / (float)M_SAMPLES;
    *mn = lo;
    *mx = hi;
}

/*add one revolution to the averaging window emit a record when it is full
*param l lock-in state
*param x, y result of lockin_cycle() for this revolution
*param valid false if the revolution must be left out of the average-->ADC saturated
*param cycle_flags status bits for this revolution, OR-ed into the record
*param mn, mx raw min/max from lockin_cycle()
*param range gain range in use for this revolution
*param[out] out written only when the function returns true
*return true once every K_BLOCKS calls, when -out- holds a complete record
*
*gain-range changes--> averaging counts taken at different gains would
*mix units--> If a valid revolution arrives on a different range from the
*ones already accumulated, those earlier revolutions are dropped and
*marked INVALID_CYCLE. The average then holds only the latest range
*this is for my understanding, these comments do not need to be read, I was 
*getting lost in the sauce
*/
bool lockin_push(lockin_t *l, float x, float y, bool valid, uint16_t cycle_flags,
                 uint16_t mn, uint16_t mx, uint8_t range, lockin_out_t *out)
{
    uint8_t i = l->n_seen;
    l->flags |= cycle_flags;
    if (mn < l->raw_min) {
        l->raw_min = mn;
    }
    if (mx > l->raw_max) {
        l->raw_max = mx;
    }
    if (valid && l->n_valid > 0 && range != l->acc_range) {
        /*range changed inside the window--> drop the old-range cycles*/
        l->acc_x = 0.0f;
        l->acc_y = 0.0f;
        l->n_valid = 0;
        for (uint8_t k = 0; k < i; k++) {
            l->cx[k] = INVALID_CYCLE;
            l->cy[k] = INVALID_CYCLE;
        }
    }
    if (valid) {
        l->acc_range = range;
        l->acc_x = l->acc_x + x;
        l->acc_y = l->acc_y + y;
        l->n_valid++;
        l->cx[i] = clamp_i16(x);
        l->cy[i] = clamp_i16(y);
    } else {
        l->cx[i] = INVALID_CYCLE;
        l->cy[i] = INVALID_CYCLE;
    }
    l->n_seen++;
    if (l->n_seen < K_BLOCKS) {
        return false; /*window not full yet*/
    }
    /*complete window*/
    out->n_valid = l->n_valid;
    out->x = l->n_valid ? l->acc_x / (float)l->n_valid : 0.0f;
    out->y = l->n_valid ? l->acc_y / (float)l->n_valid : 0.0f;
    out->range = range;
    out->flags = l->flags;
    out->raw_min = l->raw_min;
    out->raw_max = l->raw_max;
    for (int k = 0; k < K_BLOCKS; k++) {
        out->cx[k] = l->cx[k];
        out->cy[k] = l->cy[k];
    }
    reset_window(l);
    return true;
}
