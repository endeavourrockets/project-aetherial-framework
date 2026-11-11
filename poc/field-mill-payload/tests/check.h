/*check.h --> tiny test macros, counts the failures and prints where they hapened*/
#ifndef CHECK_H
#define CHECK_H

#include <math.h>
#include <stdio.h>

static int g_checks, g_fails;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fails++; \
    printf(" FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_NEAR(a, b, tol) do { double _a = (a), _b = (b); g_checks++; \
    if (!(fabs(_a - _b) <= (tol))) { g_fails++; \
    printf(" FAIL %s:%d: %s = %.9g, expected %.9g +/- %.3g\n", __FILE__, __LINE__, #a, _a, _b, (double)(tol)); } } while (0)

#define CHECK_SUMMARY(name) (printf("%s: %d checks, %d failures\n", name, g_checks, g_fails), g_fails ? 1 : 0)

#endif
