/*
 * record.h - on-card log record format (XLR-PAY-005).
 *
 * Every record is 256 bytes, little-endian, two per 512-byte SD sector.
 * Bytes 0..1 are the sync word 0xA55A and bytes 254..255 the CRC-16 of
 * bytes 0..253. Fields are written byte by byte, so the layout does not
 * depend on compiler packing or on the host's endianness.
 */
#ifndef RECORD_H
#define RECORD_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

#define RECORD_SYNC          0xA55Au
#define RECORD_VERSION       1u

#define REC_TYPE_HEADER      0u
#define REC_TYPE_SCIENCE     1u
#define REC_TYPE_EVENT       2u

/* Science record flags */
#define RF_SATURATED         (1u << 0)
#define RF_SETTLING          (1u << 1)
#define RF_INDEX_MISSING     (1u << 2)
#define RF_STEP_SLIP         (1u << 3)
#define RF_BARO_STALE        (1u << 4)
#define RF_IMU_STALE         (1u << 5)
#define RF_LOW_BATT          (1u << 6)
#define RF_MOTOR_OFF         (1u << 7)
#define RF_QUEUE_OVERFLOW    (1u << 8)
#define RF_SD_RETRY          (1u << 9)
#define RF_RESUMED           (1u << 10)
#define RF_IMU_CLIP          (1u << 11)
#define RF_DEPLOYED          (1u << 12)   /* mills extended */

/* Event codes */
#define EV_STATE_CHANGE      1u   /* arg8 = new state, arg16 = old state */
#define EV_RANGE_SWITCH      2u   /* arg8 = new range */
#define EV_STEP_SLIP         3u   /* a = slip count */
#define EV_STORAGE_ERROR     4u   /* a = error count */
#define EV_SELFTEST          5u   /* arg16 = selftest fault bits */
#define EV_RESUMED           6u   /* arg8 = resumed state, a = ms since launch */
#define EV_MOTOR             7u   /* arg8 = 1 on, 0 off */
#define EV_LOW_BATT          8u   /* arg16 = battery mV */
#define EV_COMMAND           9u   /* text = console command */
#define EV_DEPLOY           10u   /* arg8 = DEPLOY_EV_*, a = ms since launch */
#define EV_POWER_RECOVERY   11u   /* arg8 = pl_verdict_t, a = altitude above saved pad (cm), b = nv flags */

#define DEPLOY_EV_COMMANDED  1u   /* solenoid fired */
#define DEPLOY_EV_EXTENDED   2u   /* mills seen extended */
#define DEPLOY_EV_FAILED     3u   /* not extended within DEPLOY_CONFIRM_TIMEOUT_MS: motor stays off */

#define INVALID_CYCLE        (-32768)

typedef struct {
    uint32_t seq;
    uint32_t t_ms;
    uint8_t  state;
    uint8_t  range;
    uint16_t flags;
    float    x;                   /* lock-in in-phase, ADC counts */
    float    y;                   /* lock-in quadrature, ADC counts */
    uint16_t raw_min;
    uint16_t raw_max;
    uint8_t  n_valid;             /* cycles used in x, y (0..K_BLOCKS) */
    uint8_t  slip_count;          /* saturating */
    uint16_t vbat_mv;
    int32_t  pressure_pa;
    int16_t  baro_temp_cc;        /* centi-degrees C */
    int16_t  mcu_temp_cc;
    int32_t  alt_cm;              /* barometric altitude above pad */
    int16_t  vz_dms;              /* vertical speed, decimetres per second */
    int16_t  accel_mg[3];
    int16_t  gyro_dps10[3];
    uint16_t rotor_period_us;     /* 0 if no index pulse seen */
    int16_t  cx[K_BLOCKS];        /* per-cycle x, counts, INVALID_CYCLE if excluded */
    int16_t  cy[K_BLOCKS];
    int32_t  p_ref_pa;            /* pad reference pressure */
    uint32_t t_launch_ms;         /* 0 before launch */
    uint8_t  index_phase;         /* sample of the index pulse in the cycle, 0xFF unknown */
    int16_t  quat[4];             /* orientation w, x, y, z (app/attitude.h), x 30000 */
    uint8_t  att_ok;              /* 1 once the tilt was initialised on the pad */
} science_rec_t;

typedef struct {
    uint32_t seq;
    uint32_t t_ms;
    uint32_t reset_cause;
    uint32_t boot_count;
    uint8_t  resumed_state;       /* 0xFF if not resumed */
    uint16_t selftest_faults;
    int32_t  p_ref_pa;
} header_rec_t;

typedef struct {
    uint32_t seq;
    uint32_t t_ms;
    uint8_t  code;
    uint8_t  arg8;
    uint16_t arg16;
    int32_t  a;
    int32_t  b;
    char     text[32];
} event_rec_t;

void record_pack_science(const science_rec_t *r, uint8_t out[RECORD_SIZE]);
void record_pack_header(const header_rec_t *h, uint8_t out[RECORD_SIZE]);
void record_pack_event(const event_rec_t *e, uint8_t out[RECORD_SIZE]);
bool record_check(const uint8_t buf[RECORD_SIZE]);

#endif /* RECORD_H */
