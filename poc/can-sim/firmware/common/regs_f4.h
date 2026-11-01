/*
 * regs_f4.h --> the few STM32F405 registers the CAN PoC uses
 * no HAL and no CMSIS here (bare metal), so every register is written out
 * with its address from RM0090 --> easy to check a line against the manual.
 * the flight firmware will use CubeMX + HAL instead
 *
 * REG(address) --> the 32 bit hardware register at that address. volatile
 * because the hardware changes it by itself, so the compiler has to really
 * read or write it every time
 */
#ifndef REGS_F4_H
#define REGS_F4_H

#include <stdint.h>

#define REG(a) (*(volatile uint32_t *)(a))

/* after reset the F405 runs on its internal 16 MHz RC oscillator (HSI) and
 * APB1 (where CAN and USART2 are) is 16 MHz too. the PoC never turns the PLL
 * on --> every clock here is 16 MHz */
#define F4_CLOCK_HZ 16000000u

/* RCC, reset and clock control (RM0090 section 7.3)
 * every peripheral is off after reset, its EN bit here turns its clock on.
 * forgetting this is the most common reason a peripheral does nothing on real hardware */
#define RCC_BASE 0x40023800u
#define RCC_AHB1ENR REG(RCC_BASE + 0x30u) /* bit 0 GPIOA, bit 1 GPIOB */
#define RCC_APB1ENR REG(RCC_BASE + 0x40u) /* bit 17 USART2, 25 CAN1, 26 CAN2 */

/* GPIO (RM0090 section 8.4) */
#define GPIOA_BASE 0x40020000u
#define GPIOB_BASE 0x40020400u
#define GPIO_MODER(b) REG((b) + 0x00u) /* 2 bits per pin, 10 = alternate function */
#define GPIO_AFRL(b) REG((b) + 0x20u) /* 4 bits per pin 0..7 --> which AF */
#define GPIO_AFRH(b) REG((b) + 0x24u) /* 4 bits per pin 8..15 */

/* USART2, the console (RM0090 section 30.6) */
#define USART2_BASE 0x40004400u
#define USART2_SR REG(USART2_BASE + 0x00u) /* bit 7 TXE --> ready for the next byte */
#define USART2_DR REG(USART2_BASE + 0x04u)
#define USART2_BRR REG(USART2_BASE + 0x08u) /* baud rate divider */
#define USART2_CR1 REG(USART2_BASE + 0x0Cu) /* bit 13 UE on, bit 3 TE transmitter on */

/* SysTick, the 1 ms timer inside the cortex-m4 core */
#define SYST_CSR REG(0xE000E010u) /* enable, interrupt, clock source */
#define SYST_RVR REG(0xE000E014u) /* reload --> counts down from this to 0 */
#define SYST_CVR REG(0xE000E018u)

/* bxCAN (RM0090 section 32.9)
 * the F405 has two CAN controllers. CAN1 is the master --> it holds the 28
 * filter banks for both. CAN2 is the slave --> no filter registers of its
 * own, and its clock and filters only work if CAN1's clock is on too */
#define CAN1_BASE 0x40006400u
#define CAN2_BASE 0x40006800u

#define CAN_MCR(b) REG((b) + 0x000u) /* master control */
#define CAN_MSR(b) REG((b) + 0x004u) /* master status */
#define CAN_TSR(b) REG((b) + 0x008u) /* transmit status */
#define CAN_RF0R(b) REG((b) + 0x00Cu) /* receive FIFO 0 status */
#define CAN_RF1R(b) REG((b) + 0x010u) /* receive FIFO 1 status */
#define CAN_IER(b) REG((b) + 0x014u) /* interrupt enable */
#define CAN_ESR(b) REG((b) + 0x018u) /* error status */
#define CAN_BTR(b) REG((b) + 0x01Cu) /* bit timing, only writable in init mode */

/* 3 transmit mailboxes (n = 0..2), 16 bytes apart */
#define CAN_TIR(b, n) REG((b) + 0x180u + 0x10u * (n))/* id + send bit */
#define CAN_TDTR(b, n) REG((b) + 0x184u + 0x10u * (n)) /* DLC */
#define CAN_TDLR(b, n) REG((b) + 0x188u + 0x10u * (n)) /* data bytes 0..3 */
#define CAN_TDHR(b, n) REG((b) + 0x18Cu + 0x10u * (n)) /* data bytes 4..7 */

/* 2 receive FIFOs (f = 0..1), each shows its oldest message here */
#define CAN_RIR(b, f) REG((b) + 0x1B0u + 0x10u * (f))
#define CAN_RDTR(b, f) REG((b) + 0x1B4u + 0x10u * (f))
#define CAN_RDLR(b, f) REG((b) + 0x1B8u + 0x10u * (f))
#define CAN_RDHR(b, f) REG((b) + 0x1BCu + 0x10u * (f))

/* filter registers --> only at CAN1's address, shared by CAN1 and CAN2 */
#define CAN_FMR REG(CAN1_BASE + 0x200u) /* bit 0 FINIT, bits 13:8 CAN2SB */
#define CAN_FM1R REG(CAN1_BASE + 0x204u) /* per bank: 0 = mask mode, 1 = list mode */
#define CAN_FS1R REG(CAN1_BASE + 0x20Cu) /* per bank: 0 = two 16 bit, 1 = one 32 bit */
#define CAN_FFA1R REG(CAN1_BASE + 0x214u) /* per bank: 0 = FIFO 0, 1 = FIFO 1 */
#define CAN_FA1R REG(CAN1_BASE + 0x21Cu) /* per bank: 1 = active */
#define CAN_FR1(i) REG(CAN1_BASE + 0x240u + 8u * (i))
#define CAN_FR2(i) REG(CAN1_BASE + 0x244u + 8u * (i))

/* bits */
#define MCR_INRQ (1u << 0) /* ask for init mode */
#define MCR_SLEEP (1u << 1) /* ask for sleep (set after reset) */
#define MCR_TXFP (1u << 2) /* mailboxes go out in request order, not by id */
#define MCR_ABOM (1u << 6) /* automatic bus-off recovery */
#define MSR_INAK (1u << 0) /* in init mode */
#define MSR_SLAK (1u << 1) /* asleep */
#define TSR_TME(n) (1u << (26 + (n))) /* mailbox n is empty */
#define RFR_FMP (3u << 0) /* messages waiting (0..3) */
#define RFR_FOVR (1u << 4) /* a message was lost, FIFO was full */
#define RFR_RFOM (1u << 5) /* write 1 --> release the oldest message */
#define TIR_TXRQ (1u << 0) /* write 1 --> send this mailbox */
#define TIR_IDE (1u << 2) /* 1 = 29 bit extended id */
#define FMR_FINIT (1u << 0) /* filters in init mode (writable) */

#endif


/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣤⣴⣦⣦⡄⠀⠀⠀⠀
⠀⠀⠀⣀⣤⣤⣤⣤⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣿⣿⣿⣿⣿⣿⣿⣷⣄⠀⠀
⠀⣰⣿⣿⠿⢿⣿⣿⣿⣿⣿⣷⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣿⣿⣿⠿⠛⠉⠉⠁⠉⠉⠙⠻⣧⠀
⣰⣿⣿⡟⠀⠀⠈⠙⠛⠿⣿⣿⣿⣿⣿⣶⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⡿⠟⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡆
⣿⣿⣿⠁⠀⠀⠀⠀⠀⠀⠀⠈⠙⠛⠿⢿⣿⣿⣦⣀⠀⠀⠀⠀⠀⠀⢀⣴⣿⡿⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣽⣧
⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⢀⠈⠙⠻⣿⣷⣄⣤⣤⣤⣦⣾⡿⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⣿⡿
⣿⣿⣿⣷⣄⣀⣀⣀⣀⣠⣤⣶⣾⣿⣿⣿⣿⣿⣿⣷⣾⣿⣿⣿⣿⣿⣿⣶⣶⣿⣶⣶⣤⣀⠀⠀⢀⠀⠀⠀⠀⣀⣠⣾⣿⣿⠃
⠘⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⠛⠋⠉⠉⣴⣾⣿⣿⣿⣿⣿⣿⣯⣍⠉⠛⠻⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠃⠀
⠀⠀⠉⠛⠻⠿⠿⠿⠿⠿⠿⠛⠋⠀⠀⠀⠀⢠⣾⣿⣿⠟⠁⠁⠀⠈⠻⣿⣿⣷⡀⠀⠀⠉⠛⠻⠿⠿⠻⠿⠿⠟⠋⠁⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⣿⣿⠃⠀⠀⠀⠀⠀⠀⠹⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⣿⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣿⣿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣀⣠⣤⣴⣶⣶⣾⣿⣿⣿⣿⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣠⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣿⣷⣦⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⣴⣿⣿⣿⠟⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠛⢿⣿⣿⣿⣿⣿⣿⣷⣤⡀⠀⠀⠀⠀⠀
⠀⣸⣿⣿⡿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠛⠻⣿⣿⣿⣧⡀⠀⠀⠀
⢀⣿⣿⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣿⣿⣧⠀⠀⠀
⠸⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢿⣿⣿⡆⠀⠀
⠀⠹⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⣿⡧⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣿⠏⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠟⠀⠀⠀
*/