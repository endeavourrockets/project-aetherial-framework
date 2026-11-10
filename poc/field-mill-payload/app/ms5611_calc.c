/*
 * ms5611_calc.c - see ms5611_calc.h.
 *
 * Datasheet example: C1..C6 = 40127, 36924, 23317, 23282, 33464, 28312,
 * D1 = 9085466, D2 = 8569150 gives TEMP = 2007 (20.07 C) and
 * P = 100009 (1000.09 mbar = 100009 Pa). Checked in tests/c/test_app.c.
 */
#include "ms5611_calc.h"

void ms5611_compensate(const uint16_t prom[8], uint32_t d1, uint32_t d2,
                       int32_t *p_pa, int32_t *temp_cc)
{
    int64_t dT = (int64_t)d2 - ((int64_t)prom[5] << 8);
    int64_t temp = 2000 + ((dT * (int64_t)prom[6]) >> 23);
    int64_t off = ((int64_t)prom[2] << 16) + (((int64_t)prom[4] * dT) >> 7);
    int64_t sens = ((int64_t)prom[1] << 15) + (((int64_t)prom[3] * dT) >> 8);

    /* second-order compensation below 20 C */
    if (temp < 2000) {
        int64_t t2 = (dT * dT) >> 31;
        int64_t d = temp - 2000;
        int64_t off2 = (5 * d * d) >> 1;
        int64_t sens2 = (5 * d * d) >> 2;
        if (temp < -1500) {
            int64_t e = temp + 1500;
            off2 += 7 * e * e;
            sens2 += (11 * e * e) >> 1;
        }
        temp -= t2;
        off -= off2;
        sens -= sens2;
    }
    int64_t p = ((((int64_t)d1 * sens) >> 21) - off) >> 15;  /* 0.01 mbar = 1 Pa */
    *p_pa = (int32_t)p;
    *temp_cc = (int32_t)temp;
}

bool ms5611_prom_crc_ok(const uint16_t prom[8])
{
    uint16_t n[8];
    for (int i = 0; i < 8; i++) {
        n[i] = prom[i];
    }
    uint16_t crc_read = n[7] & 0x000Fu;
    n[7] &= 0xFF00u;
    uint32_t rem = 0;
    for (int cnt = 0; cnt < 16; cnt++) {
        if (cnt & 1) {
            rem ^= (uint16_t)(n[cnt >> 1] & 0x00FFu);
        } else {
            rem ^= (uint16_t)(n[cnt >> 1] >> 8);
        }
        for (int bit = 8; bit > 0; bit--) {
            rem = (rem & 0x8000u) ? (rem << 1) ^ 0x3000u : (rem << 1);
        }
    }
    rem = (rem >> 12) & 0x000Fu;
    return rem == crc_read;
}
