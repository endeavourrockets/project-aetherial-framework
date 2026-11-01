/*
 * board_f4.h --> STM32F405 start up for the CAN PoC: clocks, pins, console
 * UART and a 1 ms tick. board_putc() and board_ms() from print.h live here too
 */
#ifndef BOARD_F4_H
#define BOARD_F4_H

#include <stdint.h>

void board_init(void);

#endif
