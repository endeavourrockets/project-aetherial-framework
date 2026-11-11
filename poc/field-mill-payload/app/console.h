/*
 * console.h --> tiny text console on USART2 (115200 8N1, ST-LINK VCP)
 *
 * commands (upper or lower case doesnt matter, end with CR or LF):
 *   STATUS   one line status
 *   ARM      arm from the bench (same as pulling the arm pin)
 *   DISARM   cancel a console ARM
 *   RANGE HI | RANGE LO | RANGE AUTO
 * no printf in the output helpers, so it links without a C library
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h> 

/*named integer constants, count up from 0, if not given value*/
typedef enum { 
    CMD_NONE, 
    CMD_STATUS, 
    CMD_ARM, /*same as pulling the arm*/
    CMD_DISARM, /*opposite of ARm, thingy above*/
    CMD_RANGE_HI, 
    CMD_RANGE_LO,
               
    CMD_RANGE_AUTO, 
    CMD_VERSION, 
    CMD_UNKNOWN 
} console_cmd_t;

console_cmd_t console_poll(char *last_line, uint32_t max); /* poll function--> called on every pass of the main loop
*reads any bytes received on the UART. If a full line (ended by CR or LF)
*has arrived, it copies the line into last_line, and returns the matching command
*(or CMD_UNKNOWN), if no full line yet, returns CMD_NONE immediately.
*the lines above i learnt how it communicates and calls, and why they are necessary
*they would need hcking from someone more exprienced
*/

void con_str(const char *s); /*pointer to text for fucntion, and doesnt chnage it*/
void con_u32(uint32_t v); /*unsgined number in decimal*/
void con_i32(int32_t v); /*for singed numbers*/
void con_fixed(int32_t v, int decimals); /*prints v / 10^decimals, fixed point numbers & decimanl*/
/*floating point and print float slow on small chip without C lib, quicker this way
*if decimal too long will not be saved properly in 32 bit config in how i have organised/catagorized my code
*/
void con_nl(void);

#endif /*CONSOLE_H*/
/*Bonjour ^•ﻌ•^ฅ  🎅🏻*/