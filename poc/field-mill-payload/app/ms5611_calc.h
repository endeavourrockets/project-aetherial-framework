/*
 * ms5611_calc.h --> MS5611-01BA03 compensation (from the datasheet, first and
 * second order) and the PROM CRC-4 (TE app note AN520)
 * only integer maths so it also runs on the PC for the tests ٩(◕‿◕)۶
 */
#ifndef MS5611_CALC_H
#define MS5611_CALC_H

#include <stdbool.h>
#include <stdint.h>

/*prom[0] = factory data, prom[1..6] = C1..C6, prom[7] = serial and CRC*/
void ms5611_compensate(const uint16_t prom[8], uint32_t d1, uint32_t d2,
                       int32_t *p_pa, int32_t *temp_cc);
bool ms5611_prom_crc_ok(const uint16_t prom[8]);

#endif /*MS5611_CALC_H*/
