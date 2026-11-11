/*
*crc16.h - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
*no final XOR). Check value for "123456789" is 0x29B1.
 *The same algorithm is in tools/galvan_log.py. done in order to check and redundancy
*/
#ifndef CRC16_H
#define CRC16_H

#include <stdint.h> /*uint8_t, uint16_t, uint32_t*/

/*returns the CRC of len bytes starting at data
*const = the function only reads the data, never changes it 𓁹‿𓁹
*/
uint16_t crc16_ccitt(const uint8_t *data, uint32_t len);

#endif /*CRC16_H*/
