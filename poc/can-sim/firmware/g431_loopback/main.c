/*
 * g431_loopback/main.c --> step 5 of the CAN PoC: the NUCLEO-G431KB FDCAN1
 * sending to itself in loopback. first bench test for the board, no
 * transceiver and no second node needed
 *
 * built twice from this one file (see the Makefile):
 *   g431_loopback.elf      internal loopback --> nothing leaves the chip
 *   g431_ext_loopback.elf  external loopback (-DLOOPBACK_EXTERNAL=1) --> the
 *                          frames also come out on PA12 (FDCAN1_TX) so you can
 *                          watch them with a logic analyser
 *
 * at start it prints the FDCAN clock and the bit rate worked out from the
 * timing registers as the hardware holds them (not just the 500 kbit/s asked for):
 *   FDCAN1 clock 16000000 Hz (PCLK1 <- HSI16)
 *   bit timing: prescaler 2 x 16 tq (1+13+2), sample point 87.5 %
 *   bit rate 500000 bit/s (requested 500000, error 0 ppm)
 *
 * then every 100 ms it sends FLIGHT_PHASE (0x100) and a 0x101 frame the filter
 * has to reject, and prints what came back:
 *   [  0.100] FDCAN1 loopback RX id=0x100 dlc=6 seq=0 OK
 * after 20 frames it prints LOOPBACK PASS (or LOOPBACK FAIL) and goes quiet on
 * the console but keeps sending --> a logic analyser always has frames to
 * trigger on
 *
 * logic analyser (external loopback only): probe PA12 and GND. idle is high
 * (recessive), each frame is a burst of about 100 to 130 bits at 2 us per bit
 * (500 kbit/s), two bursts every 100 ms. most analyser software has a CAN
 * decoder --> set 500 kbit/s and it shows id 0x100, DLC 6 and the data. the
 * ACK bit shows low because in loopback the controller acks its own frame
 *
 * Renode workaround: the Renode FDCAN model ignores the loopback bits and has
 * no TX pin signal --> renode/g431_loopback.resc
 * plugs FDCAN1 into a CANHub with loopback on, which echoes every frame back.
 * so both builds act the same in Renode, only real hardware shows the
 * difference on PA12
 */
#include "board_g431.h"
#include "can_ids.h"
#include "fdcan_poc.h"
#include "print.h"

#ifndef LOOPBACK_EXTERNAL
#define LOOPBACK_EXTERNAL 0
#endif

#define FRAMES_TO_CHECK 20u

/* thousandths --> "87.5" (one decimal) */
static void print_permille(uint32_t v)
{
    print_dec(v / 10u);
    board_putc('.');
    print_dec(v % 10u);
}

static void print_timing(uint32_t requested)
{
    const char *source;
    uint32_t clock = board_fdcan_clock_hz(&source);
    fdcan_poc_timing_t t = fdcan_read_timing(clock);

    print_str("FDCAN1 clock ");
    print_dec(clock);
    print_str(" Hz (");
    print_str(source);
    print_str(")\nbit timing: prescaler ");
    print_dec(t.prescaler);
    print_str(" x ");
    print_dec(t.tq_per_bit);
    print_str(" tq (1+");
    print_dec(t.tseg1);
    print_str("+");
    print_dec(t.tseg2);
    print_str("), sample point ");
    print_permille(t.sample_permille);
    print_str(" %\nbit rate ");
    print_dec(t.bitrate);
    print_str(" bit/s (requested ");
    print_dec(requested);
    print_str(", error ");
    /* error in ppm --> CAN nodes have to agree to within about 0.5 % (5000 ppm)
     * worked out as (difference x 1000) / (requested / 1000) so it fits in 32 bits */
    int32_t diff = (int32_t)t.bitrate - (int32_t)requested;
    int32_t ppm = diff * 1000 / (int32_t)(requested / 1000u);
    if (ppm < 0) {
        board_putc('-');
        ppm = -ppm;
    }
    print_dec((uint32_t)ppm);
    print_str(" ppm)\n");
    if (ppm > 5000) {
        print_str("WARNING: bit rate more than 0.5 % off: pick another clock or bit rate\n");
    }
}

int main(void)
{
    board_init();
    const fdcan_poc_mode_t mode = LOOPBACK_EXTERNAL ? FDCAN_POC_MODE_EXTERNAL_LOOPBACK
                                                    : FDCAN_POC_MODE_INTERNAL_LOOPBACK;
    print_str(LOOPBACK_EXTERNAL
              ? "\nCAN PoC g431_ext_loopback: FDCAN1 external loopback, frames also on PA12 (TX)\n"
              : "\nCAN PoC g431_loopback: FDCAN1 internal loopback, nothing leaves the chip\n");

    if (mode == FDCAN_POC_MODE_EXTERNAL_LOOPBACK) {
        board_fdcan_pins();/* PA12 has to be FDCAN1_TX for the signal to show up */
    }

    const char *source;
    uint32_t clock = board_fdcan_clock_hz(&source);
    if (!fdcan_init(clock, CAN_BITRATE_DRAFT, mode, CAN_ID_FLIGHT_PHASE, 0x7FFu)) {
        print_str("ERROR: FDCAN init failed\n");
        for (;;) {
        }
    }
    print_timing(CAN_BITRATE_DRAFT);

    uint32_t next_tx = 100, ok = 0, bad = 0;
    uint8_t seq = 0, expect_seq = 0;

    for (;;) {
        uint32_t now = board_ms();
        if ((int32_t)(now - next_tx) >= 0) {
            next_tx += 100;
            can_frame_t tx = { .id = CAN_ID_FLIGHT_PHASE, .dlc = CAN_DLC_FLIGHT_PHASE };
            tx.data[0] = PHASE_PAD;
            tx.data[1] = seq++;
            put_u32(&tx.data[2], now);
            can_frame_t noise = tx;
            noise.id = CAN_ID_FLIGHT_PHASE + 1u;/* has to be filtered out */
            if (!fdcan_send(&tx) || !fdcan_send(&noise)) {
                print_time();
                print_str("TX FIFO full\n");
            }
        }

        can_frame_t rx;
        while (fdcan_receive(&rx)) {
            if (ok + bad >= FRAMES_TO_CHECK) {
                continue;/* check done --> just empty the FIFO */
            }
            /* good frame --> right id, right length, the seq we expect */
            bool good = rx.id == CAN_ID_FLIGHT_PHASE && rx.dlc == CAN_DLC_FLIGHT_PHASE &&
                        rx.data[1] == expect_seq;
            expect_seq = (uint8_t)(rx.data[1] + 1u);
            if (good) ok++; else bad++;
            print_time();
            print_str("FDCAN1 loopback RX id=0x");
            print_hex(rx.id, 3);
            print_str(" dlc=");
            print_dec(rx.dlc);
            print_str(" seq=");
            print_dec(rx.data[1]);
            print_str(good ? " OK\n" : " BAD\n");
            if (ok + bad == FRAMES_TO_CHECK) {
                print_str(bad == 0u ? "LOOPBACK PASS\n" : "LOOPBACK FAIL\n");
            }
        }
    }
}
/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⠀⠀⠀⠀⠀⠀⣴⠋⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢳⡄⡀⠄⠠⠀⢸⡏⠀⠄⠠⠀⡀⢠⡞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢤⣀⠀⢀⠠⠐⠀⢁⠀⠛⠀⠠⠐⠈⠈⡁⠄⠂⠐⢀⢠⡟⢀⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⡶⠟⠛⠛⠻⠿⣶⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠻⡆⡀⠄⠐⠈⣠⣴⡾⠿⠛⠛⠿⢶⣄⠀⠂⠁⡀⢘⠡⠀⠠⠀⣡⡶⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⣼⠃⠀⠀⠀⠀⠀⠀⠀⠙⢷⣄⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⢀⣄⣀⠀⠂⠠⠀⠡⠀⠄⢂⣽⠟⠁⠀⠀⠀⠀⠀⠀⠙⣷⡌⠀⠄⠀⠄⠐⢀⢱⠟⠀⡐⢀⣀⠀⠀⠀⠀⠀⠀⢐⡏⠀⠀⢀⡀⠀⠀⠀⠀⠀⠀⢻⣆⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠉⠛⢷⡌⠀⡈⠠⠐⠀⣾⠏⠀⠀⠀⢀⡀⠀⠀⠀⠀⢘⣷⠐⠀⡁⠐⠈⠀⡀⠄⠂⢀⠛⠁⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⢸⡇⢀⠀⠀⠀⠀⠀⠘⣿⡀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠠⠈⠠⠀⡙⠀⠄⠐⢀⢸⡿⠀⠀⠀⠀⢸⣷⠀⠀⠀⠀⠰⣿⠀⠂⠀⠂⢁⠠⠀⠄⣂⣠⡀⡂⠁⠀⠀⠀⠀⠀⡿⠀⠀⠀⣿⠁⠠⠐⠀⠀⠀⠀⢀⣿⡇⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠐⢀⠈⠠⠀⠄⠂⡀⢁⠠⢸⡇⠀⠀⠀⠀⢸⣿⠀⠀⠀⠀⠀⢿⡔⢈⣠⣵⣶⠾⠿⠟⠛⠛⠛⠛⠛⠛⠿⠶⣦⡜⠁⠀⠀⣰⡟⠀⠂⡀⢁⠀⠀⠀⢸⣿⠀⠀⠀⠀⠀⠀
⠀⠀⠠⠶⠷⠶⣦⣥⣀⠂⠠⠀⠄⠀⢽⡇⠀⠀⠠⠈⠀⢿⣆⠀⠀⠀⠀⠈⠻⠌⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠟⢿⣟⠛⠃⠀⠀⠀⢠⡿⠃⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠄⢀⠂⠀⠄⢉⠡⠐⠀⠂⠁⠸⣿⡀⠀⠐⡼⠶⠛⣿⡷⠦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠓⠀⠀⢀⣴⠟⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠂⢈⠀⡀⠁⠄⠠⠀⠐⠈⢀⠈⡀⠻⣧⡀⠀⠄⠐⠘⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢴⣞⡋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⢀⠈⡀⠠⠀⠂⠐⢀⠈⡀⢁⠠⠀⠄⠐⠈⠻⣦⣄⣀⠈⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠄⠠⣐⣀⡁⢈⠀⠠⠀⠄⠠⠐⠀⡈⢀⠁⢀⣽⠟⠙⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢻⡄⠀⠀⠀⠀⠀⠀⠀
⠀⠾⠛⠛⠋⠙⠛⠂⢀⠂⠐⠀⠂⡀⢁⠀⠄⣠⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠦⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⣀⣤⣤⣀⡀⠀⠀
⠀⠀⡈⠠⠈⠠⠈⠠⠀⢄⡵⠖⠳⠶⣤⣄⣲⡟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠖⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠐⠿⠋⠁⠀⠈⠛⠆⠀
⠀⠀⠄⠂⠐⢀⠈⠠⠐⠈⠀⠀⠀⠀⠀⠈⠉⠃⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣶⠾⠶⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⡿⠋⠉⠉⠙⠻⣶⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠐⠀⡁⣠⣐⣀⠂⠀⠀⢀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⠟⠁⠀⠀⠀⠀⣹⣿⡄⠀⠀⠠⠐⠀⡀⠀⠀⣿⣿⡅⠀⠀⣀⣀⣀⣸⣿⡆⠀⠀⠀⠀⠀⢀⣤⣤⣤⣄⠀⠀
⠀⠀⢠⣵⠟⠋⠉⠁⠂⠰⠚⠉⠉⠛⠛⠃⠀⠀⠀⠀⠀⠀⠀⠀⢻⣧⣄⣠⣶⠿⡶⠾⢛⣿⡇⠀⡈⠠⠐⠀⠄⠂⠀⢻⣿⣟⠻⠛⠛⠛⢛⣹⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠻⣦
⠀⠀⠉⠀⡀⠂⠁⡈⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⡀⠙⠿⣿⣤⣴⣶⡾⠟⠋⠀⠀⠀⠀⠂⠐⠀⠀⠀⠀⠙⠻⠿⣶⣶⡾⠿⣛⡁⠠⡀⠄⢀⠀⠀⠀⠀⠀⠀⠀⠀⠈
⠀⠀⠀⠀⠀⠂⠁⡀⠐⢀⠀⢀⣠⡶⠟⠛⠀⠀⠀⠠⣸⡟⣱⡖⢰⡎⣰⠆⠀⠀⠀⠀⠀⠀⠀⠠⣦⣠⣾⣇⣠⡄⠀⠀⠀⠀⠀⠀⠀⠀⣾⠋⣠⡿⢃⡾⣰⠀⠐⠛⠛⠷⣦⡀⠀⠀
⠀⠀⠀⠀⠈⠀⢁⣤⠆⠠⢠⠞⠁⠀⠀⠀⠀⠀⠀⠘⠟⠀⡉⢐⠈⠄⠌⠂⠀⠀⠀⠀⠀⠀⢀⣠⣌⢉⣀⢉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢂⠈⠡⠘⢁⠡⠀⠀⠀⠀⠀⠈⠻⠆⠀
⠀⠀⠀⠀⠀⣸⠟⠁⠐⢀⠁⡀⠀⠀⣠⡶⠟⠃⠀⠀⠂⠁⠄⠂⠐⠀⠂⠀⠀⠀⠀⠀⠀⢀⣿⠉⣿⡟⣿⠍⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠈⠀⠀⠀⠀⠲⣦⡄⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠛⠀⠈⠠⠀⠄⢀⠂⠀⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣸⡇⠀⠉⠀⠛⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⡆⠀⠀⠘⢷⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⣸⠇⠀⡁⢈⠠⠐⠀⠻⢷⣤⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣿⠋⠀⠀⠀⠀⠀⣽⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠁⢀⠠⠀⠄⠂⠐⠀⡈⣹⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡆⠀⠀⠀⠀⣰⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠀⠀⠀⠀⢸⡇⠀⡁⠄⠀⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠿⠁⠀⠀⠀⢸⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡯⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/