/*
 * record.h - on-card log record format XLR-PAY-005
 * checked 2026-11-01 --> offsets add up to 256, CRC is over bytes 0..253, and
 * app/record.c (writes) matches tools/galvan_log.py (reads, used by decode_log.py)
 * byte for byte, tested with a record full of different values
 *
 * every record is 256 bytes, little endian --> two per 512-byte SD sector
 * bytes 0..1 are the sync word 0xA55A and bytes 254..255 the CRC-16 of
 * bytes 0..253.
 * app/record.c writes every field byte by byte, so the layout does not depend
 * on compiler packing, which is great. (the structs below are only the RAM
 * side, their sizeof is not the card layout)
 *
 * byte map, every record starts with --> 0..1 sync, 2 type, 3 version,
 * 4..7 seq, 8..11 t_ms
 * science (type 1):
 *   12 state, 13 range, 14..15 flags, 16..19 x, 20..23 y, 24..25 raw_min,
 *   26..27 raw_max, 28 n_valid, 29 slip_count, 30..31 vbat_mv,
 *   32..35 pressure_pa, 36..37 baro_temp_cc, 38..39 mcu_temp_cc,
 *   40..43 alt_cm, 44..45 vz_dms, 46..51 accel_mg, 52..57 gyro_dps10,
 *   58..59 rotor_period_us, 60..79 cx, 100..119 cy, 140..143 p_ref_pa,
 *   144..147 t_launch_ms, 148 index_phase, 150..157 quat, 158 att_ok
 *   spare (always 0) --> 80..99, 120..139, 149, 159..253
 *   cx and cy have room for 20 each --> K_BLOCKS can't go over 20 without
 *   moving things
 *   12 + 48 (12..59) + 80 (60..139) + 19 (140..158) + 95 spare + 2 CRC = 256
 * event (type 2): 12 code, 13 arg8, 14..15 arg16, 16..19 a, 20..23 b,
 *   24..55 text, 56..253 spare
 * header (type 0): magic "GALVANPL", firmware version, git hash, mill settings
 *   and build date, see record_pack_header() in app/record.c and the ICD
 */
#ifndef RECORD_H
#define RECORD_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

#define RECORD_SYNC 0xA55Au
#define RECORD_VERSION 1u

#define REC_TYPE_HEADER 0u
#define REC_TYPE_SCIENCE 1u
#define REC_TYPE_EVENT 2u

/* science record flags */
#define RF_SATURATED (1u << 0)
#define RF_SETTLING (1u << 1)
#define RF_INDEX_MISSING (1u << 2)
#define RF_STEP_SLIP (1u << 3)
#define RF_BARO_STALE (1u << 4)
#define RF_IMU_STALE (1u << 5)
#define RF_LOW_BATT (1u << 6)
#define RF_MOTOR_OFF (1u << 7)
#define RF_QUEUE_OVERFLOW (1u << 8)
#define RF_SD_RETRY (1u << 9)
#define RF_RESUMED (1u << 10)
#define RF_IMU_CLIP (1u << 11)
#define RF_DEPLOYED (1u << 12) /* mills extended */

/* Event codes */
#define EV_STATE_CHANGE 1u/* arg8 = new state, arg16 = old state */
#define EV_RANGE_SWITCH 2u/* arg8 = new range */
#define EV_STEP_SLIP 3u/* a = slip count */
#define EV_STORAGE_ERROR 4u/* a = error count */
#define EV_SELFTEST 5u/* arg16 = selftest fault bits */
#define EV_RESUMED 6u/* arg8 = resumed state, a = ms since launch */
#define EV_MOTOR 7u/* arg8 = 1 on, 0 off */
#define EV_LOW_BATT 8u/* arg16 = battery mV */
#define EV_COMMAND 9u/* text = console command */
#define EV_DEPLOY 10u/* arg8 = DEPLOY_EV_*, a = ms since launch */
#define EV_POWER_RECOVERY 11u/* arg8 = pl_verdict_t (app/powerloss.h), a = altitude above the saved pad in cm, b = nv flags */

#define DEPLOY_EV_COMMANDED 1u/* solenoid fired */
#define DEPLOY_EV_EXTENDED 2u /* mills seen extended */
#define DEPLOY_EV_FAILED 3u/* not extended within DEPLOY_CONFIRM_TIMEOUT_MS: motor stays off */

#define INVALID_CYCLE (-32768)

typedef struct {
uint32_t seq;
uint32_t t_ms;
uint8_t state;
uint8_t range;
uint16_t flags;
float x; /* lockin in-phase, ADC counts */
float y; /* lockin quadrature, ADC counts */
uint16_t raw_min;
uint16_t raw_max;
uint8_t n_valid; /* cycles used in x, y (0..K_BLOCKS) */
uint8_t slip_count; /* saturating --> stops at 255 */
uint16_t vbat_mv;
int32_t pressure_pa;
int16_t baro_temp_cc;/* centi degrees C */
int16_t mcu_temp_cc;
int32_t alt_cm; /* barometric altitude above pad */
int16_t vz_dms; /* vertical speed, decimetres per second */
int16_t accel_mg[3];
int16_t gyro_dps10[3];
uint16_t rotor_period_us; /*0 if no index pulse seen*/
int16_t cx[K_BLOCKS]; /* per-cycle x, counts, INVALID_CYCLE if excluded */
int16_t cy[K_BLOCKS];
int32_t p_ref_pa; /* pad reference pressure */
uint32_t t_launch_ms;/* 0 before launch */
uint8_t index_phase; /* sample of the index pulse in the cycle, 0xFF unknown */
int16_t quat[4]; /* orientation w, x, y, z (app/attitude.h) times 30000 --> 30000 = 1.0 */
uint8_t att_ok; /* 1 once the tilt was set up on the pad, 0 --> quat not valid yet */
} science_rec_t;

typedef struct {
uint32_t seq;
uint32_t t_ms;
uint32_t reset_cause;
uint32_t boot_count;
uint8_t resumed_state;/* 0xFF if not resumed */
uint16_t selftest_faults;
int32_t p_ref_pa;
} header_rec_t;

typedef struct {
uint32_t seq;
uint32_t t_ms;
uint8_t code;
uint8_t arg8;
uint16_t arg16;
int32_t a;
int32_t b;
char text[32];
} event_rec_t;

void record_pack_science(const science_rec_t *r, uint8_t out[RECORD_SIZE]);
void record_pack_header(const header_rec_t *h, uint8_t out[RECORD_SIZE]);
void record_pack_event(const event_rec_t *e, uint8_t out[RECORD_SIZE]);
bool record_check(const uint8_t buf[RECORD_SIZE]);

#endif /* RECORD_H */
