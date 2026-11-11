/*
 * record.c --> packs the records into the 256 byte format that goes on the SD card
 * byte map is in record.h (ﾉ◕ヮ◕)ﾉ
 */
#include "record.h"
#include "crc16.h"

static void put_u8(uint8_t *b, uint32_t o, uint8_t v) { b[o] = v; }
static void put_u16(uint8_t *b, uint32_t o, uint16_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *b, uint32_t o, uint32_t v)
{
    b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8);
    b[o + 2] = (uint8_t)(v >> 16); b[o + 3] = (uint8_t)(v >> 24);
}
static void put_i16(uint8_t *b, uint32_t o, int16_t v) { put_u16(b, o, (uint16_t)v); }
static void put_i32(uint8_t *b, uint32_t o, int32_t v) { put_u32(b, o, (uint32_t)v); }
static void put_f32(uint8_t *b, uint32_t o, float v)
{
    union { float f; uint32_t u; } c = { .f = v };
    put_u32(b, o, c.u);
}

static void common(uint8_t *b, uint8_t type, uint32_t seq, uint32_t t_ms)
{
    for (uint32_t i = 0; i < RECORD_SIZE; i++) {
        b[i] = 0;
    }
    put_u16(b, 0, RECORD_SYNC);
    put_u8(b, 2, type);
    put_u8(b, 3, RECORD_VERSION);
    put_u32(b, 4, seq);
    put_u32(b, 8, t_ms);
}

static void seal(uint8_t *b)
{
    put_u16(b, RECORD_SIZE - 2, crc16_ccitt(b, RECORD_SIZE - 2));
}

void record_pack_science(const science_rec_t *r, uint8_t b[RECORD_SIZE])
{
    common(b, REC_TYPE_SCIENCE, r->seq, r->t_ms);
    put_u8(b, 12, r->state);
    put_u8(b, 13, r->range);
    put_u16(b, 14, r->flags);
    put_f32(b, 16, r->x);
    put_f32(b, 20, r->y);
    put_u16(b, 24, r->raw_min);
    put_u16(b, 26, r->raw_max);
    put_u8(b, 28, r->n_valid);
    put_u8(b, 29, r->slip_count);
    put_u16(b, 30, r->vbat_mv);
    put_i32(b, 32, r->pressure_pa);
    put_i16(b, 36, r->baro_temp_cc);
    put_i16(b, 38, r->mcu_temp_cc);
    put_i32(b, 40, r->alt_cm);
    put_i16(b, 44, r->vz_dms);
    for (int i = 0; i < 3; i++) {
        put_i16(b, 46 + 2 * i, r->accel_mg[i]);
        put_i16(b, 52 + 2 * i, r->gyro_dps10[i]);
    }
    put_u16(b, 58, r->rotor_period_us);
    for (int i = 0; i < K_BLOCKS; i++) {
        put_i16(b, 60 + 2 * i, r->cx[i]);
        put_i16(b, 100 + 2 * i, r->cy[i]);
    }
    put_i32(b, 140, r->p_ref_pa);
    put_u32(b, 144, r->t_launch_ms);
    put_u8(b, 148, r->index_phase);
    for (int i = 0; i < 4; i++) {
        put_i16(b, 150 + 2 * i, r->quat[i]);
    }
    put_u8(b, 158, r->att_ok);
    seal(b);
}

void record_pack_header(const header_rec_t *h, uint8_t b[RECORD_SIZE])
{
    static const char magic[8] = { 'G', 'A', 'L', 'V', 'A', 'N', 'P', 'L' };
    static const char git[] = FW_GIT_HASH;
    static const char date[] = __DATE__;
    static const char time_[] = __TIME__;
    common(b, REC_TYPE_HEADER, h->seq, h->t_ms);
    for (int i = 0; i < 8; i++) {
        b[12 + i] = (uint8_t)magic[i];
    }
    put_u8(b, 20, FW_VERSION_MAJOR);
    put_u8(b, 21, FW_VERSION_MINOR);
    for (int i = 0; i < 8 && git[i] != '\0'; i++) {
        b[22 + i] = (uint8_t)git[i];
    }
    put_u16(b, 30, F_MOD_HZ);
    put_u16(b, 32, FS_HZ);
    put_u8(b, 34, M_SAMPLES);
    put_u8(b, 35, K_BLOCKS);
    put_u8(b, 36, STEPS_PER_REV);
    put_u8(b, 37, N_APERTURES);
    put_u16(b, 38, ADC_FULL_SCALE);
    put_u32(b, 40, h->reset_cause);
    put_u32(b, 44, h->boot_count);
    put_u8(b, 48, h->resumed_state);
    put_u16(b, 50, h->selftest_faults);
    put_i32(b, 52, h->p_ref_pa);
    for (int i = 0; i < 11; i++) {
        b[56 + i] = (uint8_t)date[i];
    }
    for (int i = 0; i < 8; i++) {
        b[68 + i] = (uint8_t)time_[i];
    }
    seal(b);
}

void record_pack_event(const event_rec_t *e, uint8_t b[RECORD_SIZE])
{
    common(b, REC_TYPE_EVENT, e->seq, e->t_ms);
    put_u8(b, 12, e->code);
    put_u8(b, 13, e->arg8);
    put_u16(b, 14, e->arg16);
    put_i32(b, 16, e->a);
    put_i32(b, 20, e->b);
    for (int i = 0; i < 32 && e->text[i] != '\0'; i++) {
        b[24 + i] = (uint8_t)e->text[i];
    }
    seal(b);
}

bool record_check(const uint8_t b[RECORD_SIZE])
{
    uint16_t sync = (uint16_t)(b[0] | (b[1] << 8));
    uint16_t crc = (uint16_t)(b[RECORD_SIZE - 2] | (b[RECORD_SIZE - 1] << 8));
    return sync == RECORD_SYNC && crc == crc16_ccitt(b, RECORD_SIZE - 2);
}
