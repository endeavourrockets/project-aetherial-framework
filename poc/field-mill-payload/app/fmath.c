/*
 * fmath.c - log, exp and pow for float without libm. ᐠ( ᐛ )ᐟ
 *
 * log: x = m * 2^e with m in [sqrt(1/2), sqrt(2)); ln m = 2 atanh(s),
 *      s = (m - 1)/(m + 1), |s| <= 0.1716, series to s^11.
 * exp: x = k ln2 + r with |r| <= ln2/2; e^r by Taylor series to r^8;
 *      2^k applied through the float exponent bits.
 * no lib, as standard C math lib not available, so computed down here
 * Float layout (32 bits): [sign 1][exponent 8][mantissa 23]
 * value = 1.mantissa * 2^(exponent - 127)    (127 = "bias") crazy too use something I studied in uni
 * 
 */
#include <stdint.h>
#include "fmath.h"

#define LN2_F 0.693147180559945f /*ln(2)*/

/*same 32 bits seen two ways--> .f as float, .u raw bits.
* lets me read/write the exponent and mantissa fields directly.
*/
typedef union { float f; uint32_t u; } fbits_t;
/*
 * fm_logf - natural log.  ln(x) = ln(m * 2^e) = e*ln2 + ln(m)
 */
float fm_logf(float x)
{
    if (x <= 0.0f) {
        return -1.0e30f;/* callers never pass this*/
    }
    fbits_t b = { .f = x };
    /*exponent field: shift down 23, keep 8 bits, remove the bias */
    int e = (int)((b.u >> 23) & 0xFFu) - 127;
    /*keep the mantissa, force exponent to 127 (= *2^0): now m in [1, 2)*/
    b.u = (b.u & 0x007FFFFFu) | 0x3F800000u; /* m in [1, 2) */
    float m = b.f;
    /*centre m around 1 so ln(m) is small--> halve if above sqrt(2)
    *m/2 * 2^(e+1) is the same number. Now m in [0.707, 1.414]
    */
    if (m > 1.41421356f) {
        m *= 0.5f;
        e += 1;
    }
    /* ln(m) = 2*atanh(s), s = (m-1)/(m+1), |s| <= 0.1716 */
    float s = (m - 1.0f) / (m + 1.0f);
    float s2 = s * s;
    /* atanh(s) = s*(1 + s^2/3 + s^4/5 + ... + s^10/11)
    *next term s^13 ~ 1e-10, far below float precision
    */
    float poly = 1.0f + s2 * (1.0f / 3.0f + s2 * (1.0f / 5.0f + s2 * (1.0f / 7.0f +
                 s2 * (1.0f / 9.0f + s2 * (1.0f / 11.0f)))));
    /* e*ln2 + ln(m) */
    return (float)e * LN2_F + 2.0f * s * poly;
}
/*
 * fm_expf - e^x.  e^x = 2^k * e^r, where x = k*ln2 + r
 */

float fm_expf(float x)
{
    /* e^88 ~ 1.6e38, close to the float max 3.4e38: cap there */
    if (x > 88.0f) {
        x = 88.0f;
    }
    /*below ~-87 the answer is too small for a normal float */
    if (x < -87.0f) {
        return 0.0f;
    }
    /*k = x/ln2 rounded to nearest. (int) chops toward zero,
    *so add +/-0.5 first to round properly */
    float kf = x / LN2_F;
    int k = (int)(kf < 0.0f ? kf - 0.5f : kf + 0.5f);
    /* leftover part, |r| <= ln2/2 = 0.347*/
    float r = x - (float)k * LN2_F;
    /* e^r Taylor series: 1 + r + r^2/2! + ... + r^8/8!*/
    float p = 1.0f + r * (1.0f + r * (0.5f + r * (1.0f / 6.0f + r * (1.0f / 24.0f +
              r * (1.0f / 120.0f + r * (1.0f / 720.0f + r * (1.0f / 5040.0f +
              r * (1.0f / 40320.0f))))))));
    /* build 2^k directly: exponent field = k+127, mantissa 0, sign 0*/
    fbits_t b;
    b.u = (uint32_t)(k + 127) << 23;/* 2^k, k in [-126, 127] */
    return p * b.f; /* e^r * 2^k = e^x */
}
/*fm_powf - x^y = e^(y * ln x). Only for x > 0.
 *(x <= 0: log gives -1e30, result is 0 if y > 0, ~e^88 if y < 0.)
 */
float fm_powf(float x, float y)
{
    return fm_expf(y * fm_logf(x));
}
/*hello, this was 100% code copied from some forum of maths and code
*where even the comments were there, it ran in simulations, and look good to me
*somebody else should check it tho (≖ᴗ≖ ✿)*/