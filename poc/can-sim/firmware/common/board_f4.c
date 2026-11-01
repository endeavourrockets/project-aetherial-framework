/*
 * board_f4.c --> STM32F405 start up for the CAN PoC (see board_f4.h)
 *
 * pins (DRAFT, the real PCB pin out isn't decided yet):
 *   PA2  = USART2 TX (console, 115200 8N1)          AF7
 *   PB8  = CAN1 RX, PB9  = CAN1 TX --> bus A         AF9
 *   PB12 = CAN2 RX, PB13 = CAN2 TX --> bus B         AF9
 * on a real board each CAN TX/RX pair goes to its own transceiver chip (e.g.
 * TJA1051 or SN65HVD230) that drives the CANH/CANL wires. in Renode there are
 * no pins --> the controllers are joined by a CANHub instead (see renode/)
 */
#include "board_f4.h"
#include "print.h"
#include "regs_f4.h"

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
    while ((USART2_SR & (1u << 7)) == 0u) {
        /* wait for TXE */
    }
    USART2_DR = (uint8_t)c;
}

static void pin_af(uint32_t port, unsigned pin, unsigned af)
{
    GPIO_MODER(port) = (GPIO_MODER(port) & ~(3u << (2 * pin))) | (2u << (2 * pin));
    if (pin < 8) {
        GPIO_AFRL(port) = (GPIO_AFRL(port) & ~(0xFu << (4 * pin))) | (af << (4 * pin));
    } else {
        unsigned p = pin - 8;
        GPIO_AFRH(port) = (GPIO_AFRH(port) & ~(0xFu << (4 * p))) | (af << (4 * p));
    }
}

void board_init(void)
{
    /* CAN2 needs CAN1's clock on too --> CAN2's filters live inside CAN1 */
    RCC_AHB1ENR |= (1u << 0) | (1u << 1);/* GPIOA, GPIOB */
    RCC_APB1ENR |= (1u << 17) | (1u << 25) | (1u << 26); /* USART2, CAN1, CAN2 */

    pin_af(GPIOA_BASE, 2, 7);/* USART2 TX */
    pin_af(GPIOB_BASE, 8, 9);/* CAN1 RX */
    pin_af(GPIOB_BASE, 9, 9);/* CAN1 TX */
    pin_af(GPIOB_BASE, 12, 9);/* CAN2 RX */
    pin_af(GPIOB_BASE, 13, 9);/* CAN2 TX */

    USART2_BRR = 139u;/* 16 MHz / 115200 = 138.9 --> 139 */
    USART2_CR1 = (1u << 13) | (1u << 3);/* UE + TE */

    /* SysTick: 16000 core clocks = 1 ms, then interrupt
     * CSR bits --> 0 enable, 1 interrupt, 2 use the core clock */
    SYST_RVR = F4_CLOCK_HZ / 1000u - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 7u;
}

/*
⠀⠀⠀⠀⠀⠀⣀⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣴⣶⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⣼⡟⠋⠉⠛⠿⣶⣦⣤⣶⣶⣶⣶⣾⠏⠁⠀⠹⣷⡶⠿⠛⠛⢛⣷⠀⠀⠀⠀⠀
⠀⠀⠀⠀⢰⣿⠁⠀⠀⠀⠀⠈⠉⠉⠉⠉⠉⣿⠁⠀⢠⣶⣶⣽⣯⣄⡀⠀⠀⣿⡆⠀⠀⠀⠀
⠀⠀⠀⠀⠸⣿⣠⡀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣿⡀⠀⠸⣷⡿⠇⠈⠙⣿⡟⠛⠻⣷⡄⠀⠀⠀
⠀⠀⠀⠀⢀⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠿⠶⠶⢿⣷⡀⠀⣀⣿⢻⡄⠀⢸⡿⠀⠀⠀
⠀⠀⠀⠀⣾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⢿⣟⠛⠋⠀⣴⣿⠃⠀⠀⠀
⠀⠀⠀⣤⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠛⠿⠾⠟⠿⣿⡀⠀⠀⠀
⣤⣤⣤⣿⣿⣤⡄⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⣤⣿⣧⣤⣄⡀
⠀⠀⠀⠙⣿⠀⡀⠀⠻⡿⠿⢷⠄⠀⠀⠀⠀⠀⠀⠀⠀⢠⣴⡶⢶⡴⠀⠀⠉⠈⣿⠃⠀⠈⠀
⠀⠀⠿⠛⢿⣯⠁⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⢛⣷⠀⠀⠀⠀⠀⠈⠁⠀⠀⠙⣿⡿⠿⣶⠀⠀
⠀⠀⠀⣀⣬⢿⣿⣟⠀⠀⠀⠀⠀⢀⡀⠀⠉⢉⡉⠀⠀⠀⠀⠀⠀⠀⠀⢴⣶⣿⣀⠀⠀⠀⠀
⠀⠀⠘⠛⠁⠀⠉⠻⣷⣧⣄⣠⡿⠛⠟⢷⡿⠋⠻⣶⣤⣄⠀⠀⠀⣀⣴⣾⠏⠉⠛⠷⠄⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⢹⡇⠀⠀⣿⡁⠀⠀⠀⣸⣿⣿⣿⠋⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡅⠀⠀⢸⣇⠀⠀⠀⢹⣧⠀⠻⣧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣷⡄⠀⢘⣿⣄⠀⠀⣨⡿⠀⠀⠹⣷⣦⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⡟⠉⠛⠷⠟⠋⠛⠿⠾⠟⠁⠀⠀⠀⠀⠻⣧⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣴⣿⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⣷⣠⣄⣀⣄⠀⣠⡀⢀⣀⣠⣄⣤⣶⠶⠟⠛⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⣿⠹⠻⣿⠿⠿⠿⠉⠟⠋⢹⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣦⣄⣿⣄⢀⡀⠀⠀⣀⣾⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠙⠛⠛⠛⠛⠛⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/