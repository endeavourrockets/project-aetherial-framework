/*
 * altitude.h - barometric altitude above the pad and vertical speed.
 *  ( ͡° ͜ʖ ͡°)
 * Altitude above the pad: difference of ISA troposphere altitudes, what i have learnt form docs and internet
 * h = 44330.77 m * ((p_ref / p0)^0.190263 - (p / p0)^0.190263), p0 = 101325 Pa.
 * Pad reference: exponential average (time constant 10 s) while on the pad, frozen at launch.
 * Vertical speed: alpha-beta filter on altitude.
 */
#ifndef ALTITUDE_H
#define ALTITUDE_H

#include <stdbool.h> /*for the type of bool*/
#include <stdint.h>

/*below is for all of the states of the altitude estimator
*and calls/creates for every function, and changes it thourgh pointer
*/

typedef struct {
    float p_ref_pa; /*at goround/pad, ref for rest of flight*/
    bool ref_valid; /*holding condition if p_ref_pa for a REAL reading*/
    bool ref_frozen; /*this is to stop tracking after launch, holds TRUE*/
    float h_m; /*filtered altitude above pad*/
    float v_mps; /*filtered vertical speed*/
    bool init; /*false until frist sample is processed (not simply)*/
    uint32_t t_last_ms; /*timestamp for previous smapls*/
} altitude_t;


/* these sort of are function prototypes, and actual code for eahc one is in altitude.c, thank you past me
*and thank baby jesus for that, i would have been so so lost without it
*/
void altitude_init(altitude_t *a); /*reset all fielsd to start values*/

float altitude_from_pressure(float p_pa, float p_ref_pa); /*convert pressure to metres above ref pressure
*it was done with love and ISA model, needs chekcing
*the math does not chnage altitude_t
*/

void altitude_update(altitude_t *a, float p_pa, uint32_t now_ms); /*feed one barometer smaple in PA and ms
* update refrence, altitude h_m and vertical speed v_mps
*/

void altitude_freeze_ref(altitude_t *a); /*stop updating refrence to pressure, calls it at launch*/

void altitude_set_ref(altitude_t *a, float p_ref_pa, bool frozen); /*ste refrence pressure dircetly, such as value before reset for if we need it*/
/*I love toes 🦶🏻*/
#endif /*ALTITUDE_H*/
