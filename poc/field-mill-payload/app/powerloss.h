/*
 * powerloss.h - surviving a FULL power loss in flight (team decision 2026-10 D4).
 *
 * persist.c keeps the flight state in .noinit RAM, which survives a reset
 * (watchdog, brown-out) but not a power cut. This module adds a small record
 * in non-volatile memory (a flash page on the board, see drv_nv_*), written
 * only at a few events:
 *   - arming (SAFE -> ARMED): pad pressure, refreshed every NV_PAD_REFRESH_MS
 *     while armed, so weather drift on the pad stays small;
 *   - launch: launched flag + the frozen pad pressure;
 *   - mills extended: deployed flag;
 *   - disarming (ARMED -> SAFE) or powering up with the arm pin IN: cleared.
 *
 * At power-on with the arm pin already OUT and no valid RAM state, the
 * payload logs at once (no self-test spin, nothing moves) and, after a short
 * look at the barometer and IMU, powerloss_decide() says where it is.
 */
#ifndef POWERLOSS_H
#define POWERLOSS_H

#include <stdbool.h>
#include <stdint.h>

#define NV_MAGIC   0x4E564731u   /* "NVG1" */
#define NV_ARMED    (1u << 0)
#define NV_LAUNCHED (1u << 1)
#define NV_DEPLOYED (1u << 2)

typedef struct {
    uint32_t magic;
    uint32_t flags;               /* NV_ARMED | NV_LAUNCHED | NV_DEPLOYED */
    float    pad_p_pa;            /* pad reference pressure */
    uint32_t write_count;         /* for flash wear tracking */
    uint16_t crc;                 /* CRC-16 of the bytes before it */
    uint16_t pad;
} nv_state_t;

typedef enum {
    PL_PAD = 0,       /* still on the pad: normal ARMED */
    PL_BOOST,         /* engine still burning: resume in BOOST */
    PL_IN_FLIGHT,     /* well above the pad: resume in COAST (apogee logic finds the peak) */
    PL_LANDED,        /* launched earlier, now back near pad level: LANDED */
    PL_UNKNOWN        /* no valid record or no barometer: ARMED, mills stay in */
} pl_verdict_t;

typedef struct {
    bool     nv_valid;
    nv_state_t nv;
    bool     baro_ok;
    float    p_now_pa;
    bool     imu_ok;
    int32_t  accel_axial_mg;
} pl_inputs_t;

/* Seal (CRC) a record before writing; check one after reading. */
void nv_seal(nv_state_t *s);
bool nv_check(const nv_state_t *s);

pl_verdict_t powerloss_decide(const pl_inputs_t *in);
const char *powerloss_name(pl_verdict_t v);

#endif /* POWERLOSS_H */
