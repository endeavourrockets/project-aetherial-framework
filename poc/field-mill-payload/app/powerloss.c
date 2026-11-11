/*
 * powerloss.c - see powerloss.h. Pure logic: no hardware calls, so it is
 * unit-tested on the host (tests/c/test_unit.c, test_powerloss).
 */
#include "powerloss.h"
#include "altitude.h"
#include "config.h"
#include "crc16.h"

static uint16_t nv_crc(const nv_state_t *s)
{
    return crc16_ccitt((const uint8_t *)s, (uint32_t)((const uint8_t *)&s->crc - (const uint8_t *)s));
}

void nv_seal(nv_state_t *s)
{
    s->magic = NV_MAGIC;
    s->pad = 0;
    s->crc = nv_crc(s);
}

bool nv_check(const nv_state_t *s)
{
    return s->magic == NV_MAGIC && s->crc == nv_crc(s) && s->pad_p_pa > 0.0f;
}

pl_verdict_t powerloss_decide(const pl_inputs_t *in)
{
    /*no pad reference --> height unknown, stay safe (mills in)*/
    if (!in->nv_valid || !nv_check(&in->nv) || !(in->nv.flags & NV_ARMED) || !in->baro_ok) {
        return PL_UNKNOWN;
    }
    float alt_m = altitude_from_pressure(in->p_now_pa, in->nv.pad_p_pa);
    bool high = alt_m * 100.0f >= (float)POWERLOSS_MIN_ALT_CM;
    /*burning = above the LAUNCH threshold (3 g), not the burnout one, bc a
     * payload just standing on the ground already reads +1 g along its axis*/
    bool burning = in->imu_ok && in->accel_axial_mg >= LAUNCH_ACCEL_MG;

    /*still burning (high up, or low but launch already recorded)? then the
     *mills must not come out yet: BOOST ends on burnout or BOOST_MAX_MS*/
    if (burning && (high || (in->nv.flags & NV_LAUNCHED))) {
        return PL_BOOST;
    }
    if (high) {
        return PL_IN_FLIGHT;
    }
    /*near pad level --> on the pad if we never launched, else on the ground.
     *known gap: a cut in the last POWERLOSS_MIN_ALT_CM of the descent is
     *also called LANDED, that only switches the mills off a bit early, not a big deal*/
    return (in->nv.flags & NV_LAUNCHED) ? PL_LANDED : PL_PAD;
}

const char *powerloss_name(pl_verdict_t v)
{
    switch (v) {
    case PL_PAD: return "pad";
    case PL_BOOST: return "boost";
    case PL_IN_FLIGHT: return "in flight";
    case PL_LANDED: return "landed";
    default: return "unknown";
    }
}

/*⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⠀⠀⠀⠀⠀⠀⣴⠋⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢳⡄⡀⠄⠠⠀⢸⡏⠀⠄⠠⠀⡀⢠⡞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢤⣀⠀⢀⠠⠐⠀⢁⠀⠛⠀⠠⠐⠈⠈⡁⠄⠂⠐⢀⢠⡟⢀⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⡶⠟⠛⠛⠻⠿⣶⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠻⡆⡀⠄⠐⠈⣠⣴⡾⠿⠛⠛⠿⢶⣄⠀⠂⠁⡀⢘⠡⠀⠠⠀⣡⡶⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⣼⠃⠀⠀⠀⠀⠀⠀⠀⠙⢷⣄⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⢀⣄⣀⠀⠂⠠⠀⠡⠀⠄⢂⣽⠟⠁⠀⠀⠀⠀⠀⠀⠙⣷⡌⠀⠄⠀⠄⠐⢀⢱⠟⠀⡐⢀⣀⠀⠀⠀⠀⠀⠀⢐⡏⠀⠀⢀⡀⠀⠀⠀⠀⠀⠀⢻⣆⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠉⠛⢷⡌⠀⡈⠠⠐⠀⣾⠏⠀⠀⠀⢀⡀⠀⠀⠀⠀⢘⣷⠐⠀⡁⠐⠈⠀⡀⠄⠂⢀⠛⠁⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⢸⡇⢀⠀⠀⠀⠀⠀⠘⣿⡀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠠⠈⠠⠀⡙⠀⠄⠐⢀⢸⡿⠀⠀⠀⠀⢸⣷⠀⠀⠀⠀⠰⣿⠀⠂⠀⠂⢁⠠⠀⠄⣂⣠⡀⡂⠁⠀⠀⠀⠀⠀⡿⠀⠀⠀⣿⠁⠠⠐⠀⠀⠀⠀⢀⣿⡇⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠐⢀⠈⠠⠀⠄⠂⡀⢁⠠⢸⡇⠀⠀⠀⠀⢸⣿⠀⠀⠀⠀⠀⢿⡔⢈⣠⣵⣶⠾⠿⠟⠛⠛⠛⠛⠛⠛⠿⠶⣦⡜⠁⠀⠀⣰⡟⠀⠂⡀⢁⠀⠀⠀⢸⣿⠀⠀⠀⠀⠀⠀
⠀⠀⠠⠶⠷⠶⣦⣥⣀⠂⠠⠀⠄⠀⢽⡇⠀⠀⠠⠈⠀⢿⣆⠀⠀⠀⠀⠈⠻⠌⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠟⢿⣟⠛⠃⠀⠀⠀⢠⡿⠃⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠄⢀⠂⠀⠄⢉⠡⠐⠀⠂⠁⠸⣿⡀⠀⠐⡼⠶⠛⣿⡷⠦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠓⠀⠀⢀⣴⠟⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠂⢈⠀⡀⠁⠄⠠⠀⠐⠈⢀⠈⡀⠻⣧⡀⠀⠄⠐⠘⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢴⣞⡋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⢀⠈⡀⠠⠀⠂⠐⢀⠈⡀⢁⠠⠀⠄⠐⠈⠻⣦⣄⣀⠈⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠄⠠⣐⣀⡁⢈⠀⠠⠀⠄⠠⠐⠀⡈⢀⠁⢀⣽⠟⠙⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢻⡄⠀⠀⠀⠀⠀⠀⠀
⠀⠾⠛⠛⠋⠙⠛⠂⢀⠂⠐⠀⠂⡀⢁⠀⠄⣠⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠦⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⣀⣤⣤⣀⡀⠀⠀
⠀⠀⡈⠠⠈⠠⠈⠠⠀⢄⡵⠖⠳⠶⣤⣄⣲⡟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠖⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠐⠿⠋⠁⠀⠈⠛⠆⠀
⠀⠀⠄⠂⠐⢀⠈⠠⠐⠈⠀⠀⠀⠀⠀⠈⠉⠃⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣶⠾⠶⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⡿⠋⠉⠉⠙⠻⣶⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠐⠀⡁⣠⣐⣀⠂⠀⠀⢀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⠟⠁⠀⠀⠀⠀⣹⣿⡄⠀⠀⠠⠐⠀⡀⠀⠀⣿⣿⡅⠀⠀⣀⣀⣀⣸⣿⡆⠀⠀⠀⠀⠀⢀⣤⣤⣤⣄⠀⠀
⠀⠀⢠⣵⠟⠋⠉⠁⠂⠰⠚⠉⠉⠛⠛⠃⠀⠀⠀⠀⠀⠀⠀⠀⢻⣧⣄⣠⣶⠿⡶⠾⢛⣿⡇⠀⡈⠠⠐⠀⠄⠂⠀⢻⣿⣟⠻⠛⠛⠛⢛⣹⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠻⣦
⠀⠀⠉⠀⡀⠂⠁⡈⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⡀⠙⠿⣿⣤⣴⣶⡾⠟⠋⠀⠀⠀⠀⠂⠐⠀⠀⠀⠀⠙⠻⠿⣶⣶⡾⠿⣛⡁⠠⡀⠄⢀⠀⠀⠀⠀⠀⠀⠀⠀⠈
⠀⠀⠀⠀⠀⠂⠁⡀⠐⢀⠀⢀⣠⡶⠟⠛⠀⠀⠀⠠⣸⡟⣱⡖⢰⡎⣰⠆⠀⠀⠀⠀⠀⠀⠀⠠⣦⣠⣾⣇⣠⡄⠀⠀⠀⠀⠀⠀⠀⠀⣾⠋⣠⡿⢃⡾⣰⠀⠐⠛⠛⠷⣦⡀⠀⠀
⠀⠀⠀⠀⠈⠀⢁⣤⠆⠠⢠⠞⠁⠀⠀⠀⠀⠀⠀⠘⠟⠀⡉⢐⠈⠄⠌⠂⠀⠀⠀⠀⠀⠀⢀⣠⣌⢉⣀⢉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢂⠈⠡⠘⢁⠡⠀⠀⠀⠀⠀⠈⠻⠆⠀
⠀⠀⠀⠀⠀⣸⠟⠁⠐⢀⠁⡀⠀⠀⣠⡶⠟⠃⠀⠀⠂⠁⠄⠂⠐⠀⠂⠀⠀⠀⠀⠀⠀⢀⣿⠉⣿⡟⣿⠍⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠈⠀⠀⠀⠀⠲⣦⡄⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠛⠀⠈⠠⠀⠄⢀⠂⠀⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣸⡇⠀⠉⠀⠛⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⡆⠀⠀⠘⢷⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⣸⠇⠀⡁⢈⠠⠐⠀⠻⢷⣤⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣿⠋⠀⠀⠀⠀⠀⣽⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠁⢀⠠⠀⠄⠂⠐⠀⡈⣹⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡆⠀⠀⠀⠀⣰⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠀⠀⠀⠀⢸⡇⠀⡁⠄⠀⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠿⠁⠀⠀⠀⢸⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡯⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/
