/*
 * board_g431.h --> STM32G431 (NUCLEO-G431KB) start up for the CAN PoC
 * board_putc() and board_ms() from print.h live here too
 */
#ifndef BOARD_G431_H
#define BOARD_G431_H

#include <stdint.h>

void board_init(void);

/* FDCAN1 on its pins --> PA11 = RX, PA12 = TX (AF9)
 * needed for normal mode and external loopback, not for internal loopback */
void board_fdcan_pins(void);

/* FDCAN kernel clock in Hz, worked out from the RCC registers as they are
 * right now (clock source, PLL, bus dividers). '*source' gets a short text
 * like "PCLK1 <- HSI16"  ⚞₍^,,· ⩊ ·,,^₎⚟⟆*/
uint32_t board_fdcan_clock_hz(const char **source);

#endif
