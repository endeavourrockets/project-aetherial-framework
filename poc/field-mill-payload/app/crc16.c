/*Cyclcic Redundancy Check 16 bit fingerprint form block of bytes 
*16-bit remainder left at the end is the CRC. Any change to the data 
*gives a different remainder, like a fingerprint of the data block.
*catch every single-bit error, every 2-bit error and every burst of errors 
*up to 16 bits long.
*crc16.c - bitwise CRC-16/CCITT-FALSE. 256 bytes take about 20 us at
*170 MHz, which is negligible at 10 records per second.
*/
#include "crc16.h"

uint16_t crc16_ccitt(const uint8_t *data, uint32_t len)
{
    /*put the next byte into the TOP 8 bits of crc and mix it
    *in with XOR.*/
    uint16_t crc = 0xFFFFu;
    for (uint32_t i = 0; i < len; i++) { /*eahc byte*/
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) { /*each bit o fthe byte*/
            /*the actual check below, i copied code from somewhere*/
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
