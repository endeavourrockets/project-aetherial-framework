/*
 * fdcan_poc.h --> small driver for FDCAN1 on the STM32G431, used as classic
 * CAN (8 data bytes max, 11 bit ids) --> same frames as the F405 bxCAN, so
 * both chips can share one bus
 *
 * it only touches the FDCAN registers and message RAM. clocks and pins are
 * in board_g431.c --> the same driver runs on the NUCLEO-G431KB and in Renode
 * (renode/stm32g431kb.repl has FDCAN1)
 */
#ifndef FDCAN_POC_H
#define FDCAN_POC_H

#include <stdbool.h>
#include <stdint.h>

#include "bxcan.h" /* only for can_frame_t, so all PoC code shares one frame type */

/* names follow ST's HAL (FDCAN_MODE_...) with a prefix, so they don't clash
 * if the HAL headers ever get included too */
typedef enum {
    FDCAN_POC_MODE_NORMAL, /* real bus --> needs pins + transceiver + another node */
    FDCAN_POC_MODE_INTERNAL_LOOPBACK, /* TX --> RX inside the chip, TX pin stays recessive (high) */
    FDCAN_POC_MODE_EXTERNAL_LOOPBACK, /* TX --> RX inside the chip and the frames also come out
 on the TX pin (PA12) for a logic analyser */
} fdcan_poc_mode_t;

/* what the bit timing registers really hold, turned back into numbers */
typedef struct {
    uint32_t clock_hz; /* FDCAN kernel clock used for the maths */
    uint32_t prescaler; /* (NBRP + 1) x clock divider (CKDIV) */
    uint32_t tq_per_bit; /* 1 + (NTSEG1 + 1) + (NTSEG2 + 1) */
    uint32_t tseg1, tseg2; /* tq before and after the sample point */
    uint32_t bitrate; /* clock_hz / (prescaler x tq_per_bit) */
    uint32_t sample_permille; /* sample point, 875 = 87.5 % */
} fdcan_poc_timing_t;

/* starts FDCAN1 at 'bitrate' from a kernel clock of 'clock_hz' (get it from
 * board_fdcan_clock_hz()). only ids matching 'accept_id' in the bits set in
 * 'accept_mask' get through. false --> the controller didn't respond */
bool fdcan_init(uint32_t clock_hz, uint32_t bitrate, fdcan_poc_mode_t mode,
                uint16_t accept_id, uint16_t accept_mask);

/* reads NBTP and CKDIV back from the hardware and works out the real bit rate */
fdcan_poc_timing_t fdcan_read_timing(uint32_t clock_hz);

bool fdcan_send(const can_frame_t *f);
bool fdcan_receive(can_frame_t *f);

#endif
/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⢤⡀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⢰⠏⠳⣄⣀⣀⡸⠁⠀⠙⢆⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢀⡏⠀⠀⠘⠛⠋⠃⠀⠀⠀⠸⣷⣒⣉⠀⠀⠀
⠀⠔⢒⣾⡂⠀⢴⡄⠰⣾⡖⠀⠛⠁⠀⠉⢳⡄⠀⠀⠀
⠀⠋⢹⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⠀⠀⢱⡄⠀⠀
⠀⢀⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠏⠟⠀⠀⠀⢱⡀⠀
⠀⡸⠁⠀⠀⠀⠐⣟⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣇⠀
⢠⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀
⢸⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡆
⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠇
⠸⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⠀
⠀⠹⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⠃⠀
⢠⠞⢉⣕⣦⣤⣐⣫⣇⣀⣀⣀⣸⣏⣣⠤⠴⠊⠁⠀⠀
⢸⡀⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠙⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/