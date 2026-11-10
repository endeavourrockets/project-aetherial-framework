/*
*fmath.h --> small float maths for a freestanding build, no standard lib sadly.
*no C library, so logf/expf/powf don't exist-->these replace them. (°〇°)
*Accuracy: relative error below 1e-6 over the ranges used by the payload
*(log of 0.1 to 10, exp of -20 to 20). Tested against libm in
*tests/c/test_app.c. by me, it prob should be tested by otehr too
*/
#ifndef FMATH_H
#define FMATH_H

float fm_logf(float x);/* natural log, x > 0 */
float fm_expf(float x);/* e^x, |x| < 80 */
float fm_powf(float x, float y); /* x^y, x > 0 */

#endif /* FMATH_H */
