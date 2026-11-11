/*
 * altitude.c - see altitude.h. Filter gains were tuned against the
 * sim/trajectory.py profile (tests/c/test_flight.c). (>:=0)
 */
#include "altitude.h" /*read this one before cheking this one, pls*/
#include "fmath.h"

/*ISA convention, for toposhere and other shit*/
#define ISA_H_SCALE_M 44330.77f /*po, which is startndard sea-level pressrue*/
#define ISA_EXPONENT 0.190263f /*sea_level_temp/lapse_rate, (8.31446 × 0.0065) / (9.80665 × 0.0289644) = 0.190263*/
#define ISA_P0_PA 101325.0f /*gas_constant×lapse_rate/(gravity×molar_mass_of_air)*/
#define REF_TAU_S 10.0f /*time constant for refrence pressure filter*/
#define ADRIANA_ALPHA 0.25f /*how much the filter should trust the new altitude measurment*/
#define ADRIANA_BETA 0.03f /*how much meaurement error chnages velocity estimate*/

void altitude_init(altitude_t *a) /*it puts the things below into its corresponding start value*/
{
    a->p_ref_pa = 101325.0f; /*placeholder*/
    a->ref_valid = false; /*no ref yet*/
    a->ref_frozen = false; /*ref might follow meaured ref on pad*/
    a->h_m = 0.0f; /*altitude estimate m*/
    a->v_mps = 0.0f; /*speed estimate m/s*/
    a->init = false; /*no smaple yet*/
    a->t_last_ms = 0; /*no previous timestmap fro sampl above*/
}

/* Height of pressure --> p_pa above, whihc is above (in terms fo altitude) of p_ref_pa.
 * ISA altitude above sea level: H(p) = (T0/L) * (1 - (p/p0)^n), so...
 * H(p) - H(p_ref) = (T0/L) * ((p_ref/p0)^n - (p/p0)^n).
 * Lower pressure than the reference gives a positive height. yay
 */

float altitude_from_pressure(float p_pa, float p_ref_pa)
{
    /*guard below, as negative pressure is invalid reading, not good, below copied code for case like that--copied from a web page 🤡*/
    if (p_pa <= 0.0f || p_ref_pa <= 0.0f) {
        return 0.0f;
    }
    /* difference of absolute ISA altitudes, so the pad elevation is handled correctly (using p_ref as if it were sea level overstates height by
     * about 2 % at 890 m), but needs checking*/
    return ISA_H_SCALE_M * (fm_powf(p_ref_pa / ISA_P0_PA, ISA_EXPONENT) -
                            fm_powf(p_pa / ISA_P0_PA, ISA_EXPONENT));
}

/*we shoudl use known ref pressur einstaed of learnign from smaples*/
void altitude_set_ref(altitude_t *a, float p_ref_pa, bool frozen)
{
    a->p_ref_pa = p_ref_pa; /*pressure counts as 0m*/
    a->ref_valid = true; /*the next update does not overwrite*/
    a->ref_frozen = frozen; /*if its ture-->it is tracked, false--> it keep trakcing*/
}

/*lock refrence at launch, and call fro ehc new pressure reading, 
*and use as ref for mreasued pressure at pad
*/
void altitude_freeze_ref(altitude_t *a)
{
    a->ref_frozen = true;
}

/*function for porcess 1 barometre sample, and call for eahc new pressure reading*/
void altitude_update(altitude_t *a, float p_pa, uint32_t now_ms)
{
    /*time between 1 sample and another
    *substraction stays good, since it is unsigned and wraps around, 
    *making ti correct
    */
    float dt = a->init ? (float)(now_ms - a->t_last_ms) * 0.001f : 0.0f;
    a->t_last_ms = now_ms; /*remebers this time specifically fo rnext call, 
    * I learnt what this was from internet, and it is good for unsigned wrap around, 
    *i dunno thought ti was a cool concept
    */

    /*pad refrence pressure fucntion*/
    if (!a->ref_valid) {
        /*smaple pressure = 0m*/
        a->p_ref_pa = p_pa;
        a->ref_valid = true;
    } else if (!a->ref_frozen && dt > 0.0f) {
        /*for the pad, low pass filter
        *exponential average that follows slow weather chnages in pressure
        *no rapod chnages in rocket flight
        */
        float k = dt / (REF_TAU_S + dt);
        a->p_ref_pa += k * (p_pa - a->p_ref_pa);
        /*once it is frozen, p_ref_pa is the official refrence
        *unchnaged for duration of flight 
        */
    }
    /*raw altitude meaurment z, from pad*/
    float z = altitude_from_pressure(p_pa, a->p_ref_pa);

    /*adriana_alpha and adriana_beta filter*/
    if (!a->init) {
        /*starts at first sample, when testing will need to start at rest, to start measuring*/
        a->h_m = z;
        a->v_mps = 0.0f;
        a->init = true;
        return;
    }
    if (dt <= 0.0f) {
        /*(⊙ _ ⊙ )*/
        return;
    }

    /*predictions with use of adriana folter*/
    float h_pred = a->h_m + a->v_mps * dt; /*if speed is constant, calculate predicted altitude*/
    float r = z - h_pred; /*residual, how far measurement is form prediciton*/
    a->h_m = h_pred + ADRIANA_ALPHA * r; /*update altitude using adriana flter, nudge to measumrnet*/
    a->v_mps = a->v_mps + (ADRIANA_BETA / dt) * r; /*update speed using adriana filter, residual m is dovided by dt to get m/s*/
}
