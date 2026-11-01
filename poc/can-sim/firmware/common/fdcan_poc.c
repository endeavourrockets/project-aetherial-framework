/*
 * fdcan_poc.c --> small FDCAN1 driver for the STM32G431 (see fdcan_poc.h)
 * register names and bits from RM0440 chapter 44. HAL versions:
 * HAL_FDCAN_Init, HAL_FDCAN_ConfigFilter, HAL_FDCAN_AddMessageToTxFifoQ,
 * HAL_FDCAN_GetRxMessage
 *
 * big difference from the F4 bxCAN --> messages and filters aren't in
 * registers but in a 1 KiB message RAM (SRAMCAN at 0x4000A400), and on the
 * G4 its layout is fixed:
 *   0x000  28 standard id filters, 4 bytes each
 *   0x070   8 extended id filters, 8 bytes each
 *   0x0B0  Rx FIFO 0, 3 elements of 72 bytes
 *   0x188  Rx FIFO 1, 3 elements
 *   0x260  Tx event FIFO, 3 elements of 8 bytes
 *   0x278  Tx FIFO, 3 elements of 72 bytes
 * each Rx/Tx element --> word 0 = id and flags, word 1 = length and flags,
 * words 2.. = data (byte 0 in the lowest bits of word 2)
 */
#include "fdcan_poc.h"

#define REG(a) (*(volatile uint32_t *)(a))

#define FDCAN1_BASE 0x40006400u
#define FDCAN_CCCR REG(FDCAN1_BASE + 0x018u) /* control */
#define FDCAN_TEST REG(FDCAN1_BASE + 0x010u) /* test modes (loopback) */
#define FDCAN_NBTP REG(FDCAN1_BASE + 0x01Cu) /* nominal bit timing */
#define FDCAN_RXGFC REG(FDCAN1_BASE + 0x080u) /* global filter config */
#define FDCAN_RXF0S REG(FDCAN1_BASE + 0x090u) /* Rx FIFO 0 status */
#define FDCAN_RXF0A REG(FDCAN1_BASE + 0x094u) /* Rx FIFO 0 acknowledge */
#define FDCAN_TXFQS REG(FDCAN1_BASE + 0x0C4u) /* Tx FIFO status */
#define FDCAN_TXBAR REG(FDCAN1_BASE + 0x0CCu) /* Tx add request --> send buffer n */
#define FDCAN_CKDIV REG(FDCAN1_BASE + 0x100u) /* kernel clock divider (PDIV) */

#define CCCR_INIT (1u << 0)
#define CCCR_CCE (1u << 1) /* configuration change enable */
#define CCCR_MON (1u << 5) /* bus monitoring --> don't drive the TX pin */
#define CCCR_TEST (1u << 7) /* unlocks the TEST register */
#define TEST_LBCK (1u << 4) /* loopback */

#define SRAMCAN 0x4000A400u
#define RAM_STDFILT (SRAMCAN + 0x000u)
#define RAM_RXFIFO0 (SRAMCAN + 0x0B0u)
#define RAM_TXFIFO (SRAMCAN + 0x278u)
#define RAM_END (SRAMCAN + 0x350u)
#define ELEMENT_SIZE  72u

#define WAIT_LOOPS    100000u

/* picks a prescaler and tq per bit that give exactly 'bitrate' from
 * 'clock_hz' if it can. a bit is 1 tq sync + TSEG1 + TSEG2, sampled between
 * TSEG1 and TSEG2. tries 16 tq first (87.5 % sample point, the usual CAN
 * value), then 17, 15, 18, 14 ... down to 8 and up to 25
 * e.g. 16 MHz, 500 kbit/s --> prescaler 2, 16 tq
 * at 170 MHz 16 tq doesn't divide evenly, but 170 MHz / 500 k = 340 = 20 x 17
 * --> prescaler 20 with 17 tq is exact
 * if nothing is exact it uses 16 tq and the nearest prescaler, and
 * fdcan_read_timing() shows how far off that is */
static void choose_timing(uint32_t clock_hz, uint32_t bitrate, uint32_t *brp, uint32_t *tq)
{
    for (int step = 0; step <= 17; step++) {
        /* 16, 17, 15, 18, 14, ... */
        int t = 16 + ((step & 1) ? (step + 1) / 2 : -(step / 2));
        if (t < 8 || t > 25) {
            continue;
        }
        uint32_t per_bit = bitrate * (uint32_t)t;
        if (clock_hz % per_bit == 0u && clock_hz / per_bit >= 1u && clock_hz / per_bit <= 512u) {
            *brp = clock_hz / per_bit;
            *tq = (uint32_t)t;
            return;
        }
    }
    *tq = 16u;
    *brp = (clock_hz + bitrate * 8u) / (bitrate * 16u);    /* rounded */
    if (*brp < 1u) *brp = 1u;
    if (*brp > 512u) *brp = 512u;
}

bool fdcan_init(uint32_t clock_hz, uint32_t bitrate, fdcan_poc_mode_t mode,
                uint16_t accept_id, uint16_t accept_mask)
{
    /* 1. stop the controller (INIT), then unlock the config (CCE)
     *    CCE can only be set once INIT reads back as 1 */
    FDCAN_CCCR |= CCCR_INIT;
    uint32_t i = 0;
    while ((FDCAN_CCCR & CCCR_INIT) == 0u) {
        if (++i > WAIT_LOOPS) return false;
    }
    FDCAN_CCCR |= CCCR_CCE;

    /* 2. message RAM isn't cleared by reset on real silicon --> zero it or old
     *    random filters might be active (32 bit writes only) */
    for (uint32_t a = SRAMCAN; a < RAM_END; a += 4u) {
        REG(a) = 0u;
    }

    /* 3. test modes (same register settings ST's HAL uses)
     *    TEST = 1 unlocks the TEST register, LBCK = 1 loops TX back to RX
     *    inside the chip --> the RX pin is ignored and the controller acks its
     *    own frames, so no other node is needed
     *    internal loopback --> also MON = 1 (bus monitoring), holds the TX pin
     *      recessive so nothing leaves the chip
     *    external loopback --> MON = 0, every frame also goes out on the TX
     *      pin. don't connect it to a real bus while testing, the frames
     *      would really be sent */
    if (mode == FDCAN_POC_MODE_INTERNAL_LOOPBACK) {
        FDCAN_CCCR |= CCCR_TEST | CCCR_MON;
        FDCAN_TEST |= TEST_LBCK;
    } else if (mode == FDCAN_POC_MODE_EXTERNAL_LOOPBACK) {
        FDCAN_CCCR |= CCCR_TEST;
        FDCAN_TEST |= TEST_LBCK;
    }

    /* 4. bit timing. no kernel clock divider (CKDIV.PDIV = 0, divide by 1),
     *    then the prescaler and tq picked above. TSEG2 is about 1/8 of the bit
     *    (at least 1 tq) and TSEG1 the rest --> sample point stays near 87.5 %
     *    fields hold value minus 1
     *    NBTP: NSJW 31:25, NBRP 24:16, NTSEG1 15:8, NTSEG2 6:0 */
    uint32_t brp, tq;
    choose_timing(clock_hz, bitrate, &brp, &tq);
    uint32_t tseg2 = (tq + 4u) / 8u;
    if (tseg2 < 1u) tseg2 = 1u;
    uint32_t tseg1 = tq - 1u - tseg2;
    FDCAN_CKDIV = 0u;
    FDCAN_NBTP = ((1u - 1u) << 25) | ((brp - 1u) << 16) | ((tseg1 - 1u) << 8) | (tseg2 - 1u);

    /* 5. one standard filter, classic type (id + mask), stored in FIFO 0
     *    bits 31:30 SFT = 10 (classic), 29:27 SFEC = 001 (to FIFO 0),
     *    26:16 SFID1 = id, 10:0 SFID2 = mask */
    REG(RAM_STDFILT) = (2u << 30) | (1u << 27) | ((uint32_t)(accept_id & 0x7FFu) << 16) | (accept_mask & 0x7FFu);
    /* RXGFC: LSS (bits 20:16) = 1 filter in use, frames that match no filter
     * get rejected --> ANFS (5:4) = 10, ANFE (3:2) = 10 */
    FDCAN_RXGFC = (1u << 16) | (2u << 4) | (2u << 2);

    /* 6. run. clearing INIT clears CCE too */
    FDCAN_CCCR &= ~CCCR_INIT;
    i = 0;
    while (FDCAN_CCCR & CCCR_INIT) {
        if (++i > WAIT_LOOPS) return false;
    }
    return true;
}

fdcan_poc_timing_t fdcan_read_timing(uint32_t clock_hz)
{
    /* works from what the registers hold now, not what i meant to write
     * --> catches a wrong clock or a typo in the timing code */
    uint32_t nbtp = FDCAN_NBTP;
    uint32_t pdiv = FDCAN_CKDIV & 0xFu; /* 0 = /1, n = /(2 x n) */
    fdcan_poc_timing_t t;
    t.clock_hz = clock_hz;
    t.tseg1 = ((nbtp >> 8) & 0xFFu) + 1u;
    t.tseg2 = (nbtp & 0x7Fu) + 1u;
    t.tq_per_bit = 1u + t.tseg1 + t.tseg2;
    t.prescaler = (((nbtp >> 16) & 0x1FFu) + 1u) * (pdiv ? 2u * pdiv : 1u);
    t.bitrate = clock_hz / (t.prescaler * t.tq_per_bit);
    t.sample_permille = (1u + t.tseg1) * 1000u / t.tq_per_bit;
    return t;
}

bool fdcan_send(const can_frame_t *f)
{
    if (FDCAN_TXFQS & (1u << 21)) { /* TFQF --> Tx FIFO full */
        return false;
    }
    uint32_t put = (FDCAN_TXFQS >> 16) & 3u; /* TFQPI --> where to write */
    uint32_t el = RAM_TXFIFO + put * ELEMENT_SIZE;
    REG(el + 0u) = (uint32_t)(f->id & 0x7FFu) << 18; /* standard id sits in bits 28:18 */
    REG(el + 4u) = (uint32_t)(f->dlc & 0xFu) << 16; /* DLC, classic CAN (FDF = 0) */
    for (unsigned w = 0; w < 2; w++) {
        uint32_t v = 0;
        for (unsigned b = 0; b < 4; b++) {
            v |= (uint32_t)f->data[4 * w + b] << (8 * b);
        }
        REG(el + 8u + 4u * w) = v;
    }
    FDCAN_TXBAR = 1u << put;
    return true;
}

bool fdcan_receive(can_frame_t *f)
{
    if ((FDCAN_RXF0S & 0xFu) == 0u) { /* F0FL --> fill level */
        return false;
    }
    uint32_t get = (FDCAN_RXF0S >> 8) & 3u; /* F0GI --> oldest element */
    uint32_t el = RAM_RXFIFO0 + get * ELEMENT_SIZE;
    uint32_t w0 = REG(el + 0u), w1 = REG(el + 4u);
    uint32_t d0 = REG(el + 8u), d1 = REG(el + 12u);
    f->id = (uint16_t)((w0 >> 18) & 0x7FFu);
    f->dlc = (uint8_t)((w1 >> 16) & 0xFu);
    for (unsigned b = 0; b < 4; b++) {
        f->data[b] = (uint8_t)(d0 >> (8 * b));
        f->data[4 + b] = (uint8_t)(d1 >> (8 * b));
    }
    FDCAN_RXF0A = get; /* release it */
    return true;
}

/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⠴⢤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⢾⣛⡆⠈⡏⠰⠶⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢦⣿⣄⣉⣁⣤⠽⣦⣤⡶⠶⣤⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠚⠹⡉⢃⣴⠟⠉⠀⠀⠀⠀⠉⠻⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⢡⡿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠈⢷⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡂⣿⡇⠀⠀⠀⠀⣠⣤⣀⠀⠀⠀⠘⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⢻⣷⠀⠀⠀⠀⢯⡉⣿⡆⠀⠀⣰⡇⠀⢀⣀⢤⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢿⣧⣀⠀⠀⢀⣴⡟⠀⠀⣰⣿⡷⢏⡭⠔⠹⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠛⠿⠿⠟⠋⠀⢀⣼⣿⣿⣿⠿⣭⡉⢛⡃⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⣀⣤⠶⠿⣟⣟⣝⡷⣲⢤⣀⠀⠀⠀⠀⠀⠀⢠⠀⠀⠀⠀⠀⠀⣠⣾⡿⠃⠹⡟⢖⠢⠽⠦⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠀⢀⣴⠞⠁⠀⠀⠀⠀⠀⠉⠙⢦⣕⡯⣷⣄⠀⠀⠠⣴⠋⢓⡶⠂⠀⢀⣼⣿⠟⠁⠀⠀⠈⠺⡄⠋⣀⣰⣀⠀⠀⠀⠀⠀⠀
⠀⣠⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢯⣒⢭⣦⠀⠀⢸⠕⠦⠇⠀⣤⣿⡿⠃⣠⠴⠶⠶⣤⡀⠀⠀⠼⠿⡏⠀⠀⠀⠀⠀⠀
⢀⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣝⡫⢧⠀⠀⠀⠀⣠⣾⡿⠉⠀⢸⠁⢀⣀⠀⠈⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣼⠁⠀⠀⠀⠀⣠⣶⣿⣿⣷⣦⡀⠀⠀⠀⠀⠙⡮⣛⡆⠀⠀⣰⣿⡏⢐⡀⠀⠸⣦⣀⣨⠇⠒⣸⣀⣀⣄⣀⡀⠀⠀⠀⠀⠀⠀
⢹⠆⠀⠀⠀⢠⣿⡿⠃⠀⠀⠉⢻⡄⠀⠀⠀⠀⠙⣗⢿⠀⣸⡟⡙⢷⡈⢠⠀⡀⠈⠉⣁⠘⣠⠟⠉⠉⠙⢿⣿⡆⠀⠀⠀⠀⠀
⠘⣇⠀⠀⠀⠈⢿⡇⠀⢲⠀⠀⠀⢻⡀⠀⢤⣴⡀⠹⣽⣴⡿⣁⠀⠈⠻⣦⣤⣥⣌⣡⡤⠞⠁⠀⣰⠛⠆⣸⣿⡇⠀⠀⠀⠀⠀
⠀⢻⡄⠀⠀⠀⠀⠉⠙⠉⠀⠀⠀⢸⠀⠀⠚⠻⠁⠀⢿⣿⢣⢀⡤⠶⠲⢮⣍⡉⠉⠁⠀⣤⠀⠀⠘⢿⣿⡿⠏⠀⠀⠀⠀⠀⠀
⠀⠀⢻⣄⠀⠀⠀⠀⠀⠀⠀⠀⣰⠋⠀⡀⠀⠀⠀⠀⢸⣿⣰⠏⢀⡴⢶⡄⢸⡇⠀⢖⠚⠉⠓⣲⠆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠙⠷⣦⣄⣀⣀⣠⣴⠾⠁⠀⢰⣕⢲⣪⡉⠅⣾⣿⡟⠀⠈⠳⠭⠵⠋⠀⠀⣼⣁⢄⣸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠉⠉⠉⠉⠀⠀⠀⢤⡼⠉⢹⡣⣉⠀⣿⣿⡇⠀⠀⢠⣴⠶⢦⡀⠀⠁⠀⠀⣙⡂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡔⠟⡛⡀⡆⣿⡟⣷⠀⠀⣿⣇⣲⠈⡿⢀⣴⣾⣿⠿⠿⣿⣷⣦⡀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢰⡀⡷⣰⣇⡴⠠⠜⠀⠀⠀⠀⣿⡇⡘⣷⣄⠀⠙⣉⠞⢡⣿⡟⠁⠀⠀⠀⠀⠈⠻⣿⣆⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠠⣤⣯⣷⣧⠿⠵⠶⠶⣤⣄⡀⠀⢻⣇⠡⡌⠻⣏⠉⠁⠀⢺⣿⠀⠀⢀⣶⣶⣤⠀⠀⠘⣿⡀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠠⢾⡿⡋⠗⠈⠃⠈⠁⠰⠉⡻⢶⣜⣿⢘⠂⣠⣌⣷⡀⠀⢸⣿⡆⠀⠈⠧⠄⢸⡇⠀⠀⣹⠇⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⣰⠏⠄⠁⠀⠀⠀⠀⠀⠀⠀⠁⠈⠙⢿⣏⠰⡏⣬⣿⢻⡀⠀⠻⣿⣄⠀⠀⣠⠞⠀⠀⢠⡟⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢰⡏⠴⠀⠀⠀⠀⢀⣠⣀⣀⠀⠀⠀⠀⠈⢻⣧⡙⢦⣤⡾⠁⠀⠀⠈⠙⠛⠋⠁⠀⠀⣠⡞⠁⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠸⢿⡀⠀⠀⠀⣰⠋⠁⠀⠉⠙⢦⠀⠀⠀⠀⢻⣟⢦⣄⡀⠀⠀⠀⠀⠀⠀⠀⣀⣤⠾⠋⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢹⡹⣇⠀⠀⠘⡇⠀⠀⠖⡆⠀⠘⣧⠀⠀⠀⠀⠻⣎⢿⠹⠳⢲⡲⢶⠶⠾⠛⠉⣁⣠⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠸⣎⡹⣧⡀⠀⠻⣦⣤⠴⠃⠀⢰⠏⠀⣤⣀⣴⠀⠙⣧⡱⡀⢠⡾⢟⣛⡓⣆⠀⠿⣿⠛⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠱⣊⠜⣻⣶⣄⣀⠀⠀⠀⣀⠟⠠⣔⣊⠁⣶⣄⠀⠈⢷⣅⢸⠅⢸⣓⣃⣼⠀⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠙⢮⡞⡜⡝⡝⡟⡯⠟⠁⠀⠀⠀⠈⠿⠀⠀⠀⠀⠀⠹⣶⡳⢤⣹⣯⡥⠴⠶⠶⠦⣄⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣤⣤⣀⠀⠈⠻⣦⡀⠀⠀⡞⣙⣷⠀⢈⡆⠀⠀⣀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⠏⣤⢈⣿⠀⠀⠀⠈⠳⣄⠈⣷⣈⣉⣠⡼⢡⡾⠛⠉⠙⢳⡀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣄⠈⠉⠁⢀⣀⣤⣶⣶⣾⣷⣌⠙⠋⠁⠀⣿⠀⢤⡀⠀⠀⣷
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠻⠿⠛⠛⠉⠁⡼⠋⢠⠨⠍⡳⢄⡀⠀⠘⠻⠛⠁⠀⣰⡿
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇⣔⡒⡞⡀⠁⠰⠙⡲⢦⣤⣤⣤⣾⠿⠁
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⠺⠞⠧⠁⠀⠀⠀⠁⠐⠉⠉⠉⠀⠀⠀
*/
