/*
 * bxcan.c --> small bxCAN driver, register level (see bxcan.h)
 * register names and bits are from RM0090 chapter 32. the HAL versions are
 * HAL_CAN_Init / HAL_CAN_ConfigFilter / HAL_CAN_AddTxMessage / HAL_CAN_GetRxMessage
 *
 * receiving is polled (main() keeps asking if anything came in). the flight
 * firmware should use the receive interrupt instead (CAN1_RX0 = IRQ 20,
 * CAN2_RX0 = IRQ 64 on the F405) so no frame has to wait
 */
#include "bxcan.h"
#include "regs_f4.h"

/* give up waiting for the hardware after this many loops --> a missing
 * transceiver or clock doesn't hang the program forever */
#define WAIT_LOOPS 100000u

static bool wait_msr(uint32_t base, uint32_t bits, uint32_t want)
{
    for (uint32_t i = 0; i < WAIT_LOOPS; i++) {
        if ((CAN_MSR(base) & bits) == want) {
            return true;
        }
    }
    return false;
}

bool bxcan_init(uint32_t base, uint32_t bitrate)
{
    /* after reset the controller is asleep --> wake it and ask for init mode,
     * the only mode where the bit timing can be written */
    CAN_MCR(base) = MCR_INRQ;
    if (!wait_msr(base, MSR_INAK | MSR_SLAK, MSR_INAK)) {
        return false;
    }

    /* bit timing: a CAN bit is cut into time quanta (tq)
     * 1 tq sync + TS1 + TS2, the bit is sampled between TS1 and TS2
     * 16 MHz clock and prescaler BRP --> 1 tq = BRP / 16 MHz
     * i use 16 tq per bit (1 + 13 + 2) --> sample point at 87.5 %, the usual
     * value for CANopen and most CAN networks
     * 500 kbit/s --> BRP = 16 MHz / (500 k x 16) = 2
     * the register holds value minus 1. SJW (resync jump) = 1 tq */
    uint32_t brp = F4_CLOCK_HZ / (bitrate * 16u);
    CAN_BTR(base) = ((brp - 1u) << 0) /* BRP */
                  | ((13u - 1u) << 16) /* TS1 */
                  | ((2u - 1u) << 20) /* TS2 */
                  | ((1u - 1u) << 24); /* SJW */

    /* TXFP --> mailboxes go out in the order i fill them, not by id
     * ABOM --> recovers by itself after bus-off */
    CAN_MCR(base) = MCR_TXFP | MCR_ABOM | MCR_INRQ;

    /* leave init mode --> it joins the bus after seeing 11 recessive bits
     * in a row (bus idle) */
    CAN_MCR(base) &= ~MCR_INRQ;
    return wait_msr(base, MSR_INAK, 0u);
}

/* filters
 * with no active filter a bxCAN receives nothing at all
 * in 32 bit mode one bank is either
 *   mask mode --> an id and a mask (these bits must match), or
 *   list mode --> two exact ids
 * the 11 bit standard id sits in bits 31:21 of the filter word */

void bxcan_filters_begin(unsigned can2_start_bank)
{
    /* FINIT = 1 --> reception stops while the filters change
     * CAN2SB --> first bank that belongs to CAN2 (reset value 14, so banks
     * 0-13 are CAN1 and 14-27 are CAN2) */
    CAN_FMR = FMR_FINIT | ((can2_start_bank & 0x3Fu) << 8);
}

static void filter_common(unsigned bank, bool list_mode, unsigned fifo)
{
    uint32_t bit = 1u << bank;
    CAN_FA1R &= ~bit; /* off while editing */
    CAN_FS1R |= bit; /* one 32 bit filter */
    if (list_mode) CAN_FM1R |= bit; else CAN_FM1R &= ~bit;
    if (fifo) CAN_FFA1R |= bit; else CAN_FFA1R &= ~bit;
}

void bxcan_filter_mask(unsigned bank, uint16_t id, uint16_t mask, unsigned fifo)
{
    filter_common(bank, false, fifo);
    CAN_FR1(bank) = (uint32_t)id << 21;
    /* mask bits 2 and 1 too --> only IDE = 0 (standard id) and RTR = 0 (data frame) get through */
    CAN_FR2(bank) = ((uint32_t)mask << 21) | (1u << 2) | (1u << 1);
    CAN_FA1R |= 1u << bank;
}

void bxcan_filter_list(unsigned bank, uint16_t id1, uint16_t id2, unsigned fifo)
{
    filter_common(bank, true, fifo);
    CAN_FR1(bank) = (uint32_t)id1 << 21;
    CAN_FR2(bank) = (uint32_t)id2 << 21;
    CAN_FA1R |= 1u << bank;
}

void bxcan_filters_end(void)
{
    CAN_FMR &= ~FMR_FINIT;
}

/* send and receive */

bool bxcan_send(uint32_t base, const can_frame_t *f)
{
    for (unsigned n = 0; n < 3; n++) {
        if (CAN_TSR(base) & TSR_TME(n)) {
            uint32_t lo = 0, hi = 0;
            for (unsigned i = 0; i < 4; i++) { /* byte 0 goes in the lowest bits */
                lo |= (uint32_t)f->data[i] << (8 * i);
                hi |= (uint32_t)f->data[i + 4] << (8 * i);
            }
            CAN_TDLR(base, n) = lo;
            CAN_TDHR(base, n) = hi;
            CAN_TDTR(base, n) = f->dlc & 0xFu;
            /* writing the id with TXRQ = 1 --> starts sending */
            CAN_TIR(base, n) = ((uint32_t)f->id << 21) | TIR_TXRQ;
            return true;
        }
    }
    return false; /* all busy --> bus too busy, or nobody is acknowledging */
}

bool bxcan_receive(uint32_t base, unsigned fifo, can_frame_t *f)
{
    volatile uint32_t *rfr = fifo ? &CAN_RF1R(base) : &CAN_RF0R(base);
    if ((*rfr & RFR_FMP) == 0u) {
        return false;
    }
    uint32_t rir = CAN_RIR(base, fifo);
    uint32_t lo = CAN_RDLR(base, fifo);
    uint32_t hi = CAN_RDHR(base, fifo);
    f->id = (uint16_t)(rir >> 21);
    f->dlc = (uint8_t)(CAN_RDTR(base, fifo) & 0xFu);
    for (unsigned i = 0; i < 4; i++) {
        f->data[i] = (uint8_t)(lo >> (8 * i));
        f->data[i + 4] = (uint8_t)(hi >> (8 * i));
    }
    *rfr = RFR_RFOM; /* release it --> the next message moves up */
    return true;
}

/*
⢠⡶⠒⠖⢢⡤⣤⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣤⢤⡔⠶⢲⣶⡄
⢸⣇⠃⡐⠀⠀⢀⠉⠙⢣⣀⠀⠀⠀⠀⠀⠀⣀⠞⠛⠉⢀⠀⠀⠄⠳⣼⡇
⣿⡆⡃⢀⠀⠀⠄⡘⠠⢸⡘⣄⠀⠀⠀⠀⣠⡛⡄⢀⠃⠄⠀⢀⠘⠳⣄⣿
⣿⡔⡁⠂⠀⢈⠐⠠⢁⢲⣥⣟⣯⠉⠉⣽⢳⣵⡖⠤⠈⠄⡁⠀⢂⠱⣘⣾
⣿⡔⡱⠈⠀⠄⡈⠐⠠⣿⣼⢻⣞⡠⣁⢾⠟⣮⣵⠃⠌⡐⠀⢀⠂⠰⡱⣾
⣿⣰⢑⠈⠄⠂⠄⡁⢂⡄⢦⡯⣟⠙⠋⢻⣽⣔⡠⢌⠠⢀⠁⠄⣨⠒⣵⣻
⠸⣷⢎⡂⠴⣈⢦⣹⡶⣾⢻⣼⠀⠀⠀⠀⣧⢟⡿⣽⣺⢥⣊⡜⣤⢛⣼⠇
⠀⢻⣼⣱⣢⣽⠾⣏⢷⡹⠎⡏⠀⠀⠀⠀⢹⡸⢇⣟⡻⢾⣻⡵⣏⣾⠿⠀
⠀⠀⠈⢹⡡⢍⠲⠌⠦⠑⢹⠁⠀⠀⠀⠀⠈⡷⠈⢦⠱⣉⠦⡩⣏⠉⠀⠀
⠀⠀⢨⠇⢀⠂⠄⢂⠐⣈⣸⠀⠀⠀⠀⠀⠀⢗⡈⠠⠐⡀⠐⠠⠹⣃⠀⠀
⠀⢀⡿⠀⡀⢀⠈⠀⠂⠄⡇⠀⠀⠀⠀⠀⠀⢸⡁⠀⠁⢀⠈⠠⠀⢿⡀⠀
⠀⢨⡇⠀⠐⢠⠈⠐⢠⠸⡇⠀⠀⠀⠀⠀⠀⢸⡄⢡⠈⡄⠈⡄⢡⢸⡇⠀
⠀⣿⠐⡎⠴⡠⢌⡐⣀⠢⡇⠀⠀⠀⠀⠀⠀⢸⡄⣂⠰⢤⡑⠲⣌⠒⣿⠀
⠀⠙⠳⠾⠷⠷⣮⣴⢧⡽⡇⠀⠀⠀⠀⠀⠀⢸⣵⣬⣳⣦⠿⠷⠞⠟⠋
*/