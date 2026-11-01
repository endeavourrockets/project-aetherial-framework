/*
 * obc/main.c --> step 3 of the CAN PoC: a fake OBC (on board computer)
 *
 * the team's plan --> the OBC owns the flight state and broadcasts it. this
 * pretends to fly (a fixed, fast timeline) and sends
 *   FLIGHT_PHASE (0x100) every 100 ms and TIME_SYNC (0x080) every 1 s
 * on both buses, CAN1 = bus A and CAN2 = bus B (two redundant buses). the
 * same message goes out on both --> if one bus dies the other still gets it
 * there. message layouts in common/can_ids.h
 *
 * console: a line per phase change and per TIME_SYNC, and once a second a
 * STATS line with frames handed to each bus and send failures. a failure
 * means all 3 transmit mailboxes were still busy --> on real hardware that
 * happens when nobody acks the frames (bus cut, no other node)
 */
#include "board_f4.h"
#include "bxcan.h"
#include "can_ids.h"
#include "print.h"
#include "regs_f4.h"

/* fake flight, much faster than a real one so a short sim shows every
 * phase --> phase i starts at start_ms[i] */
static const uint32_t start_ms[] = { 0, 500, 1000, 1500, 2500, 2600, 3500 };

static uint8_t phase_at(uint32_t ms)
{
    uint8_t p = 0;
    for (uint8_t i = 0; i < sizeof start_ms / sizeof start_ms[0]; i++) {
        if (ms >= start_ms[i]) {
            p = i;
        }
    }
    return p;
}

/* index 0 = bus A (CAN1), 1 = bus B (CAN2) */
static uint32_t g_tx[2], g_fail[2];

static void send_both(const can_frame_t *f)
{
    static const uint32_t bus_base[2] = { CAN1_BASE, CAN2_BASE };
    for (int b = 0; b < 2; b++) {
        if (bxcan_send(bus_base[b], f)) {
            g_tx[b]++;
        } else {
            g_fail[b]++;
        }
    }
}

int main(void)
{
    board_init();
    print_str("\nCAN PoC obc: FLIGHT_PHASE 10 Hz + TIME_SYNC 1 Hz on bus A (CAN1) and bus B (CAN2)\n");

    if (!bxcan_init(CAN1_BASE, CAN_BITRATE_DRAFT) || !bxcan_init(CAN2_BASE, CAN_BITRATE_DRAFT)) {
        print_str("ERROR: CAN init failed\n");
        for (;;) {
        }
    }
    /* the OBC only sends in this PoC --> no filters on, nothing received */

    uint32_t next_phase_tx = 100, next_sync_tx = 1000, next_stats = 1000;
    uint8_t seq = 0, last_phase = 0xFF;

    for (;;) {
        uint32_t now = board_ms();

        if ((int32_t)(now - next_phase_tx) >= 0) {
            next_phase_tx += 100;
            uint8_t phase = phase_at(now);
            if (phase != last_phase) {
                last_phase = phase;
                print_time();
                print_str("OBC phase -> ");
                print_str(phase_name(phase));
                print_str("\n");
            }
            can_frame_t f = { .id = CAN_ID_FLIGHT_PHASE, .dlc = CAN_DLC_FLIGHT_PHASE };
            f.data[0] = phase;
            f.data[1] = seq++;
            put_u32(&f.data[2], now);
            send_both(&f);
        }

        if ((int32_t)(now - next_sync_tx) >= 0) {
            next_sync_tx += 1000;
            can_frame_t f = { .id = CAN_ID_TIME_SYNC, .dlc = CAN_DLC_TIME_SYNC };
            put_u32(&f.data[0], now);
            put_u32(&f.data[4], 0);/* UTC not known --> decision D5 still open */
            send_both(&f);
            print_time();
            print_str("OBC TX TIME_SYNC obc_ms=");
            print_dec(now);
            print_str("\n");
        }

        if ((int32_t)(now - next_stats) >= 0) {
            next_stats += 1000;
            print_time();
            print_str("OBC STATS txA=");
            print_dec(g_tx[0]);
            print_str(" txB=");
            print_dec(g_tx[1]);
            print_str(" failA=");
            print_dec(g_fail[0]);
            print_str(" failB=");
            print_dec(g_fail[1]);
            print_str("\n");
        }
    }
}
/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣴⠟⠛⠉⠻⣶⡀⠀⠀⠀⠀⠀⠀⢠⣾⠟⠉⠉⠻⣷⣄⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣰⡟⠁⠀⠀⠀⠀⠈⢿⡄⠀⠀⠀⠀⣰⡟⠁⠀⠀⠀⠀⠀⢿⡆⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⣼⡏⠀⠀⠀⠀⠀⠀⠀⠈⣿⠀⠀⠀⣰⡟⠀⠀⠀⠀⠀⠀⠀⠈⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢰⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⡆⠀⢀⣿⠁⠀⠀⠀⠀⠀⠀⠀⠀⢹⡇⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣾⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣧⠀⣸⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠨⣿⠀⣿⡂⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⠀
⠀⠀⠀⠀⠀⢨⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢐⣿⢀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡇⠀⠀⠀
⠀⠀⠀⠀⠀⠐⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠨⣿⢸⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⣿⢸⡧⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡏⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢹⡧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣼⡧⠀⠀⠀⠀⠀⠀⠀⠀⣰⡿⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠈⣿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⡿⠁⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⣠⣶⠟⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠷⣤⡀⠀⠀⠀⠀
⠀⠀⠀⢠⣾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣦⡀⠀⠀
⠀⠀⣠⡿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⡄⠀
⠀⢠⡿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⡀
⠀⣾⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣧
⢀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿
⠰⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿
⠀⣿⡀⠀⠀⠀⠀⠀⠀⣴⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣄⠀⠀⠀⠀⠀⠀⢰⡿
⠀⠹⣷⠀⠀⠀⠀⠀⠀⠈⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⠟⠀⠀⠀⠀⠀⠀⣿⠃
⠀⠀⢻⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡟⠀
⠀⠀⠀⠙⢷⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⡀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⠟⠀⠀
⠀⠀⠀⠀⠀⠛⢿⣤⣀⠀⠀⠀⠀⠀⠀⠀⢰⠾⠿⢿⣯⠁⠀⠀⠀⠀⠀⠀⠀⣀⣴⠿⠃⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠈⠙⠛⠷⢶⣤⣤⣄⣀⣀⣀⣀⠀⠀⣀⣀⣀⣠⣤⣴⡶⠟⠋⠁⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠛⠛⠛⠛⠛⠛⠛⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀

*/