#ifndef PRESSURE_H
#define PRESSURE_H

#include <stdbool.h>
#include <stdint.h>

/* Demo limits, unchanged from main.c. Flight limits still need agreeing. */
bool pressure_is_valid(int32_t pressure_pa);

#endif
