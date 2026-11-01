/*
 * experiment/main.c --> step 3 of the CAN PoC: the Experiment board (the one
 * that will run the field mills) listening to the OBC
 *
 * gets FLIGHT_PHASE (0x100) and TIME_SYNC (0x080) on both buses, CAN1 = bus A
 * and CAN2 = bus B. the OBC sends everything on both buses so each message
 * normally comes in twice --> the first copy is used (RX), the second is
 * spotted and ignored (DUP). if a bus dies, the copies from the other bus
 * aren't duplicates anymore and get used instead, with no gap
 *
 * duplicate rule (is_duplicate below): same id and exactly the same data
 * bytes as the last frame used with that id, arriving within DUP_WINDOW_MS.
 * this only works because every OBC message has something that changes each
 * time (a counter or a time stamp) --> two different messages never have the
 * same bytes. so every message sent on both buses needs a changing field
 *
 * console lines:
 *   RX A FLIGHT_PHASE phase=BOOST seq=12 obc_ms=1200   used, came first on bus A
 *   DUP B 0x100 seq=12                                 same message via bus B, ignored
 *   RX B TIME_SYNC obc_ms=2000 utc=0 offset_ms=0       offset = OBC time minus our time
 *   PHASE -> BOOST                                     what the payload would act on
 *   EXP STATS rxA=.. rxB=.. used=.. dup=..             once a second
 */
#include <stdbool.h>

#include "board_f4.h"
#include "bxcan.h"
#include "can_ids.h"
#include "print.h"
#include "regs_f4.h"

/* about twice the time two copies could be apart, and well under the fastest
 * message period (100 ms) --> the next real message is never taken for a copy,
 * even after the counter wraps */
#define DUP_WINDOW_MS 50u

/* the last frame used, for each id we listen to */
typedef struct {
    bool valid;
    uint16_t id;
    can_frame_t frame;
    uint32_t t_ms;
} last_used_t;

static last_used_t g_last[2] = {
    { .id = CAN_ID_TIME_SYNC }, { .id = CAN_ID_FLIGHT_PHASE },
};

static uint32_t g_rx[2], g_used, g_dup;

static bool same_frame(const can_frame_t *a, const can_frame_t *b)
{
    if (a->dlc != b->dlc) {
        return false;
    }
    for (unsigned i = 0; i < a->dlc && i < 8u; i++) {
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}

/* true --> already used this exact message, ignore it
 * false --> new message, remember it and use it */
static bool is_duplicate(const can_frame_t *f, uint32_t now)
{
    for (unsigned i = 0; i < 2; i++) {
        last_used_t *l = &g_last[i];
        if (l->id != f->id) {
            continue;
        }
        if (l->valid && now - l->t_ms <= DUP_WINDOW_MS && same_frame(&l->frame, f)) {
            return true;
        }
        l->valid = true;
        l->frame = *f;
        l->t_ms = now;
        return false;
    }
    return false;   /* an id we don't track --> never counted as a copy */
}

static void handle(const can_frame_t *f, int bus)
{
    static uint8_t current_phase = 0xFF;
    uint32_t now = board_ms();
    const char bus_name = (char)('A' + bus);
    g_rx[bus]++;

    if (is_duplicate(f, now)) {
        g_dup++;
        print_time();
        print_str("DUP ");
        board_putc(bus_name);
        print_str(" 0x");
        print_hex(f->id, 3);
        if (f->id == CAN_ID_FLIGHT_PHASE) {
            print_str(" seq=");
            print_dec(f->data[1]);
        }
        print_str("\n");
        return;
    }
    g_used++;

    print_time();
    print_str("RX ");
    board_putc(bus_name);
    if (f->id == CAN_ID_FLIGHT_PHASE) {
        print_str(" FLIGHT_PHASE phase=");
        print_str(phase_name(f->data[0]));
        print_str(" seq=");
        print_dec(f->data[1]);
        print_str(" obc_ms=");
        print_dec(get_u32(&f->data[2]));
        print_str("\n");
        if (f->data[0] != current_phase) {
            current_phase = f->data[0];
            print_time();
            print_str("PHASE -> ");
            print_str(phase_name(current_phase));
            print_str("\n");
        }
    } else if (f->id == CAN_ID_TIME_SYNC) {
        /* how far the OBC clock is ahead of ours --> a real board would use it
         * to stamp its log records with OBC (mission) time */
        int32_t offset = (int32_t)(get_u32(&f->data[0]) - now);
        print_str(" TIME_SYNC obc_ms=");
        print_dec(get_u32(&f->data[0]));
        print_str(" utc=");
        print_dec(get_u32(&f->data[4]));
        print_str(" offset_ms=");
        if (offset < 0) {
            board_putc('-');
            offset = -offset;
        }
        print_dec((uint32_t)offset);
        print_str("\n");
    }
}

int main(void)
{
    board_init();
    print_str("\nCAN PoC experiment: listening on bus A (CAN1) and bus B (CAN2)\n");

    if (!bxcan_init(CAN1_BASE, CAN_BITRATE_DRAFT) || !bxcan_init(CAN2_BASE, CAN_BITRATE_DRAFT)) {
        print_str("ERROR: CAN init failed\n");
        for (;;) {
        }
    }

    /* list mode filters --> exactly these two ids, nothing else
     * bank 0 is CAN1's (bus A), bank 14 is CAN2's (bus B) */
    bxcan_filters_begin(14);
    bxcan_filter_list(0, CAN_ID_TIME_SYNC, CAN_ID_FLIGHT_PHASE, 0);
    bxcan_filter_list(14, CAN_ID_TIME_SYNC, CAN_ID_FLIGHT_PHASE, 0);
    bxcan_filters_end();

    uint32_t next_stats = 1000;
    for (;;) {
        can_frame_t f;
        while (bxcan_receive(CAN1_BASE, 0, &f)) {
            handle(&f, 0);
        }
        while (bxcan_receive(CAN2_BASE, 0, &f)) {
            handle(&f, 1);
        }

        uint32_t now = board_ms();
        if ((int32_t)(now - next_stats) >= 0) {
            next_stats += 1000;
            print_time();
            print_str("EXP STATS rxA=");
            print_dec(g_rx[0]);
            print_str(" rxB=");
            print_dec(g_rx[1]);
            print_str(" used=");
            print_dec(g_used);
            print_str(" dup=");
            print_dec(g_dup);
            print_str("\n");
        }
    }
}
/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣤⣤⡄⢠⣤⣤⡄⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡟⢦⡀⠛⣿⠁⠀⢹⣇⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢻⡆⠓⡆⠛⣶⠀⠀⣿⠁⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢹⡆⠓⡄⢹⡆⠀⠉⣷⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⡇⢹⠈⢹⡇⠀⡿⣤⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⡀⠈⣿⣀⣹⠀⠙⠛⠃⠘⠛⢣⣄⠀⠀
⠀⠀⠀⠀⣰⠶⠞⠛⠛⠛⠛⠳⠶⣆⡿⠀⠀⠀⠀⢀⣀⣤⠀⠙⣷⠀
⠀⠀⣤⠾⠉⠀⠀⠀⠀⠀⠀⠀⠀⠉⠀⠀⠀⠀⠀⠸⠿⠿⠀⠀⣉⣷
⠀⢸⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⣿
⢸⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣤⣤⡟⠛⠀
⢸⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠀⠀⠀⠀
⠸⢧⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠀⠀⠀⠀
⠀⠺⣧⡀⢠⣀⠀⠀⣀⣟⠛⠛⣧⣄⡀⠀⠀⣸⡇⠀⣿⠉⠀⠀⠀⠀
⠀⠀⠀⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠀⠀⠀⠀⠀⠀ 
*/