/*
 * f4_dual/main.c --> step 2 of the CAN PoC: one STM32F405 talking to itself
 *
 *   CAN1 --(bus)--> CAN2
 *
 * CAN1 sends a fake FLIGHT_PHASE (id 0x100) every 100 ms (10 Hz), CAN2
 * receives it and prints it on USART2. CAN1 also sends a noise frame with id
 * 0x101 that CAN2's filter has to block --> if 0x101 ever shows up on the
 * console the filter is wrong
 *
 * in Renode CAN1 and CAN2 are joined by a CANHub (renode/f4_dual.resc).
 * on a real board both transceivers would go on the same CANH/CANL pair,
 * with a 120 ohm resistor at each end of the bus
 *
 * expected console:
 *   CAN PoC f4_dual: CAN1 -> CAN2 at 500 kbit/s
 *   [  0.100] CAN2 RX id=0x100 dlc=6 phase=PAD seq=0 obc_ms=100
 *   ...
 */
#include "board_f4.h"
#include "bxcan.h"
#include "can_ids.h"
#include "print.h"
#include "regs_f4.h"

int main(void)
{
    board_init();
    print_str("\nCAN PoC f4_dual: CAN1 -> CAN2 at 500 kbit/s\n");

    if (!bxcan_init(CAN1_BASE, CAN_BITRATE_DRAFT) || !bxcan_init(CAN2_BASE, CAN_BITRATE_DRAFT)) {
        print_str("ERROR: CAN init failed\n");
        for (;;) {
        }
    }

    /* CAN1 gets banks 0-13 and uses none (it only sends). CAN2 gets 14-27,
     * bank 14 lets only id 0x100 through (mask 0x7FF --> all 11 bits must
     * match) into FIFO 0 */
    bxcan_filters_begin(14);
    bxcan_filter_mask(14, CAN_ID_FLIGHT_PHASE, 0x7FFu, 0);
    bxcan_filters_end();

    uint32_t next_tx = 100;
    uint8_t seq = 0;

    for (;;) {
        uint32_t now = board_ms();

        /* sender side, every 100 ms */
        if ((int32_t)(now - next_tx) >= 0) {   /* now >= next_tx, still right when the ms counter wraps */
            next_tx += 100;
            can_frame_t tx = { .id = CAN_ID_FLIGHT_PHASE, .dlc = CAN_DLC_FLIGHT_PHASE };
            tx.data[0] = (uint8_t)((now / 1000u) % 7u);  /* fake phase, next one every second */
            tx.data[1] = seq++;
            put_u32(&tx.data[2], now);
            /* noise --> same data, id one bit off from 0x100 */
            can_frame_t noise = tx;
            noise.id = CAN_ID_FLIGHT_PHASE + 1u;
            if (!bxcan_send(CAN1_BASE, &tx) || !bxcan_send(CAN1_BASE, &noise)) {
                print_time();
                print_str("CAN1 TX mailboxes full\n");
            }
        }

        /* receiver side, print everything CAN2 got */
        can_frame_t rx;
        while (bxcan_receive(CAN2_BASE, 0, &rx)) {
            print_time();
            print_str("CAN2 RX id=0x");
            print_hex(rx.id, 3);
            print_str(" dlc=");
            print_dec(rx.dlc);
            print_str(" phase=");
            print_str(phase_name(rx.data[0]));
            print_str(" seq=");
            print_dec(rx.data[1]);
            print_str(" obc_ms=");
            print_dec(get_u32(&rx.data[2]));
            print_str("\n");
        }
    }
}


/*
⠀⠀⠀⠀⠀⠀⡀⡀⡀⡀⢀⢀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⡤⢤⠤⢤⡤⡄⡀⠀⠀⠀
⠀⠀⠀⣆⡔⣿⡿⡟⣷⣟⣯⣿⣿⣷⣆⢄⠀⠀⠀⠀⠀⠀⠀⣀⣀⣖⣟⣯⠭⡿⡿⡿⣿⡣⠯⣏⣄⠀
⠀⢀⣼⡧⢿⡯⠇⠡⠩⠑⠘⠉⠣⡿⣿⣷⣔⡀⠀⠀⠀⠀⣼⣯⢛⠩⠂⠁⠁⠀⠀⠀⠀⠁⠣⣮⣇⡂
⠀⡐⡳⢯⡓⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⢋⡿⣿⣧⡄⠀⣪⣟⠏⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⣷⡍
⠀⡅⣿⣹⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⡥⠋⣗⡵⣯⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣿⠅
⠀⣆⢿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠅⡍⡗⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⡏⠂
⠈⡓⡯⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢄⣾⠁⡀
⠀⠀⠷⣧⡂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⡯⠋⠀⠀
⠀⠀⠀⢑⢵⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣿⣿⠃⠀⠀⠀
⠀⠀⠀⠈⣵⠻⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⣏⡗⠁⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠱⢅⣗⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⣴⣟⠯⠁⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠁⣅⠋⡂⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡔⣿⡯⠋⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠉⠢⣎⡦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣶⡟⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⣷⡤⡀⠀⠀⠀⠀⠀⠀⠀⣨⠿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠱⠧⣆⠄⠀⠀⡤⡮⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠳⣇⣋⡅⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠁⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/