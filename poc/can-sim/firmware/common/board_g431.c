/*
 * board_g431.c --> STM32G431 (NUCLEO-G431KB) start up for the CAN PoC
 * 16 MHz HSI clock (the reset default), console on USART2 (PA2 = TX, that's
 * the ST-LINK virtual COM port on the Nucleo, 115200 8N1) and a 1 ms SysTick.
 * register addresses from RM0440 (STM32G4 reference manual)
 *
 * FDCAN1 pins (board_fdcan_pins): PA11 = RX, PA12 = TX, AF9
 * internal loopback needs no pins, external loopback drives PA12.
 * on the payload pin map PA11 and PA12 are the SD chip select and the ARM PIN
 * (see CAN_ENABLED in payload-config/config.h) --> fine on a bare Nucleo, but don't run
 * the external loopback with the arm switch wired
 */
#include <stdint.h>

#include "board_g431.h"
#include "print.h"

#define REG(a) (*(volatile uint32_t *)(a))

#define G4_CLOCK_HZ 16000000u
#define RCC_BASE 0x40021000u
#define RCC_CFGR REG(RCC_BASE + 0x08u)/* clock switch and bus prescalers */
#define RCC_PLLCFGR REG(RCC_BASE + 0x0Cu)/* PLL source and factors */
#define RCC_AHB2ENR REG(RCC_BASE + 0x4Cu)/* bit 0 GPIOA */
#define RCC_APB1ENR1 REG(RCC_BASE + 0x58u)/* bit 17 USART2, bit 25 FDCAN */
#define RCC_CCIPR REG(RCC_BASE + 0x88u)/* bits 25:24 FDCANSEL --> FDCAN clock source */

#define GPIOA_BASE 0x48000000u
#define GPIOA_MODER REG(GPIOA_BASE + 0x00u)
#define GPIOA_AFRL REG(GPIOA_BASE + 0x20u)
#define GPIOA_AFRH  REG(GPIOA_BASE + 0x24u)

/* ASSUMPTION: the NUCLEO-G431KB HSE crystal is 24 MHz (check UM2397 / the
 * board schematic). only used if CubeMX moves to HSE or a PLL fed by HSE,
 * the PoC itself stays on HSI16 */
#define HSE_HZ 24000000u
#define HSI_HZ 16000000u

#define USART2_BASE 0x40004400u
#define USART2_CR1 REG(USART2_BASE + 0x00u)
#define USART2_BRR REG(USART2_BASE + 0x0Cu)
#define USART2_ISR REG(USART2_BASE + 0x1Cu)/* bit 7 TXE */
#define USART2_TDR  REG(USART2_BASE + 0x28u)

#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)

static volatile uint32_t g_ms;

void SysTick_Handler(void)
{
    g_ms++;
}

uint32_t board_ms(void)
{
    return g_ms;
}

void board_putc(char c)
{
    while ((USART2_ISR & (1u << 7)) == 0u) {
    }
    USART2_TDR = (uint8_t)c;
}

void board_init(void)
{
    RCC_AHB2ENR |= 1u << 0; /* GPIOA */
    RCC_APB1ENR1 |= (1u << 17) | (1u << 25); /* USART2, FDCAN */
    /* FDCAN kernel clock = PCLK1 (FDCANSEL = 10) --> 16 MHz here. the reset
     * value 00 picks HSE, which the Nucleo doesn't have fitted by default */
    RCC_CCIPR = (RCC_CCIPR & ~(3u << 24)) | (2u << 24);

    /* PA2 --> AF7 = USART2 TX */
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 4)) | (2u << 4);
    GPIOA_AFRL = (GPIOA_AFRL & ~(0xFu << 8)) | (7u << 8);
    USART2_BRR = G4_CLOCK_HZ / 115200u; /* 138 */
    USART2_CR1 = (1u << 3) | (1u << 0); /* TE, UE (UE is bit 0 on the G4, not 13 like the F4) */

    SYST_RVR = G4_CLOCK_HZ / 1000u - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 7u;
}

void board_fdcan_pins(void)
{
    /* PA11, PA12 --> mode 10 (alternate function), AF9 = FDCAN1  ᜊ(꒪ˊ꒳ˋ꒪)ᜊ*/
    GPIOA_MODER = (GPIOA_MODER & ~((3u << 22) | (3u << 24))) | (2u << 22) | (2u << 24);
    GPIOA_AFRH = (GPIOA_AFRH & ~((0xFu << 12) | (0xFu << 16))) | (9u << 12) | (9u << 16);
}

/* clock tree (RM0440 section 7)
 *   SYSCLK = HSI16, HSE or PLL R output (RCC_CFGR.SWS)
 *   HCLK   = SYSCLK / AHB prescaler (RCC_CFGR.HPRE)
 *   PCLK1  = HCLK / APB1 prescaler (RCC_CFGR.PPRE1)
 *   FDCAN kernel clock = HSE, PLL Q or PCLK1 (RCC_CCIPR.FDCANSEL)
 *   PLL output = input / M x N / R (or / Q) (RCC_PLLCFGR) */

static uint32_t pll_out_hz(unsigned div_bits) /* div_bits --> the PLLR or PLLQ field */
{
    uint32_t cfg = RCC_PLLCFGR;
    uint32_t in = ((cfg & 3u) == 3u) ? HSE_HZ : HSI_HZ; /* PLLSRC: 2 HSI16, 3 HSE */
    uint32_t m = ((cfg >> 4) & 0xFu) + 1u;
    uint32_t n = (cfg >> 8) & 0x7Fu;
    return in / m * n / (2u * (div_bits + 1u)); /* 00 --> /2, 01 --> /4 ... */
}

uint32_t board_fdcan_clock_hz(const char **source)
{
    uint32_t cfgr = RCC_CFGR;
    uint32_t sysclk;
    const char *pclk1_name;
    switch ((cfgr >> 2) & 3u) { /* SWS --> the clock really in use */
    case 2u: sysclk = HSE_HZ; pclk1_name = "PCLK1 <- HSE"; break;
    case 3u: sysclk = pll_out_hz((RCC_PLLCFGR >> 25) & 3u); pclk1_name = "PCLK1 <- PLL"; break;
    default: sysclk = HSI_HZ; pclk1_name = "PCLK1 <- HSI16"; break;
    }
    static const uint16_t ahb_div[8] = { 2, 4, 8, 16, 64, 128, 256, 512 };
    uint32_t hpre = (cfgr >> 4) & 0xFu;
    uint32_t hclk = hpre < 8u ? sysclk : sysclk / ahb_div[hpre - 8u];
    uint32_t ppre1 = (cfgr >> 8) & 7u;
    uint32_t pclk1 = ppre1 < 4u ? hclk : hclk / (2u << (ppre1 - 4u));

    switch ((RCC_CCIPR >> 24) & 3u) { /* FDCANSEL */
    case 0u:
        *source = "HSE";
        return HSE_HZ;
    case 1u:
        *source = "PLL Q";
        return pll_out_hz((RCC_PLLCFGR >> 21) & 3u);
    default:
        *source = pclk1_name;
        return pclk1;
    }
    
}


/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⡀⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢠⡕⠯⠉⠁⠉⠊⠋⠛⠓⠖⠣⠯⡷⣷⣮⡶⣵⣲⣶⣶⡶⡦⡤⡤⡀⣤⡶⡆⣠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠰⣿⡗⠋⠀⠀⠤⣄⣀⣀⡀⣀⣀⣀⣀⡀⡀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⡷⣦⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢨⡃⡀⠀⢰⡅⠅⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠉⢉⡉⣿⣿⡾⣟⠃⠉⠒⡗⣦⠀⠀⠮⡟⠀⠀⣀⣀⣀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⡾⣂⠀⠀⢈⡃⠀⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣶⡮⠌⠀⠀⠀⣷⡃⠀⠘⢧⡂⠀⠙⣾⡎⣺⠛⠋⠉⢇⡀⠀
⠀⠀⠀⠀⠀⠀⢾⠆⠄⠀⠠⠤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠂⠀⠀⠀⣿⡇⠀⠀⠀⠀⠁⣷⡧⠀⠻⣔⠀⣤⣏⠉⠀⠀⠀⠀⢸⡃⠀
⠀⠀⠀⠀⠀⣀⣾⠃⠂⠀⠐⠤⠀⠀⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⡇⠀⠀⠀⠀⠀⠀⠈⠿⠿⠷⠦⠭⠁⠀⠀⠀⠀⠀⢸⣅⠀
⠀⠀⢀⡔⠋⠉⠹⡧⠁⠀⢈⠊⠀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠐⠀⢟⠽⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠂⡧⣀
⠀⢌⠌⠀⠀⡨⠑⣥⠀⠀⢠⣈⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠀⠀⢯⣟⠄⠀⠀⠀⠀⣼⢵⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀⡅⡟
⣼⠃⠀⠀⠀⡇⢩⢵⠆⠀⠠⡵⠀⠀⠁⠀⠀⠀⠀⠀⡀⠀⠈⢬⢿⠀⠀⠀⢀⡀⡀⠻⠾⠅⠀⠀⠀⠀⠀⠀⠀⡖⣯⡇⠀⠀⢸⠇
⡏⠀⠀⠀⣞⠁⢨⡧⠀⠀⢀⡂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⣿⠀⠀⠀⠈⠓⠛⠃⠀⠀⠀⠀⠀⢀⠀⠀⠀⠈⠁⣆⣀⡠⣿⣄
⣭⡆⢀⠁⡇⠀⠨⡟⠅⠀⠐⣆⠀⠀⠀⠀⠀⠈⠀⠀⠀⡀⠀⠀⠘⠾⠏⣆⠀⠀⠀⠀⠀⠀⠈⢆⣀⡎⠄⣀⣠⠂⠀⠀⢈⡩⠋⠁
⢫⡅⠅⠀⠡⡀⢀⠅⡄⠀⠀⢯⠀⢀⡠⢄⠤⠤⠤⠤⠄⠀⠀⠀⠀⠀⠈⠛⠛⠷⠦⢄⣀⣀⢀⣀⡠⠤⠤⠖⠒⣒⣥⡋⠊⠀⠀⠀
⠈⡷⡕⠢⠈⣇⠈⠐⡓⡀⠀⠀⠀⠀⠈⠈⠉⠋⠙⠁⠋⠋⠉⠉⠉⠉⠉⠀⠁⠁⠡⠋⠝⠁⠀⠉⠁⢱⠯⠤⠁⠁⠁⠀⠀⠀⠀⠀
⠀⠘⡇⠐⢤⡿⠀⡀⠗⢩⢄⣀⢀⠀⡀⢀⡀⣀⣀⢀⠠⣀⢀⡀⣀⡀⣀⡀⡀⣤⣐⣐⣀⣂⡐⣀⠀⣸⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠉⠉⠁⢸⠲⠉⠢⠇⠫⠆⠉⡈⡘⠁⠉⠀⠁⡚⡅⠉⠉⠉⠉⠉⠏⠋⠉⠁⠀⢀⡞⠀⠁⠉⠁⠂⣢⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠸⡰⠀⠀⠀⠀⢀⡼⠃⠺⡆⠀⠀⡔⡼⠀⠀⠀⠀⠀⠀⢿⡄⠀⠀⢀⢢⢇⠣⡄⠀⠀⠀⡼⡀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠙⠷⣶⣖⠦⠋⠀⠀⠀⠈⠉⠉⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠉⠀⠉⠚⠵⠶⠖⠋⠁⠀⠀⠀⠀⠀⠀⠀
*/