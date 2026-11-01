/*
 * startup_cm4.c --> first code that runs after reset, for every PoC program
 * (STM32F405 and STM32G431 both)
 *
 * what a cortex-m does at power on:
 * 1. reads word 0 of the vector table below --> starting stack pointer (top of RAM)
 * 2. reads word 1 --> address of Reset_Handler, and jumps there
 * 3. Reset_Handler copies the starting values of globals from flash to RAM
 *    (.data), zeroes the rest (.bss) and calls main()
 * every other entry is the function for one interrupt. only SysTick is used,
 * the rest go to Default_Handler (an endless loop --> if you end up there an
 * interrupt fired that nobody expected)
 *
 * CubeIDE projects use ST's assembly startup file instead, this C one does
 * the same and is easier to read
 */
#include <stdint.h>

/* these come from the linker script (*.ld), not from C code */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* weak alias --> if a program has its own SysTick_Handler that one is used,
 * if not the name just points at Default_Handler */
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));

/* 102 is the G431 count, the F405 has 82 --> a longer table doesnt hurt */
#define N_IRQ 102

__attribute__((section(".isr_vector"), used))
const void *const g_vectors[16 + N_IRQ] = {
    [0]  = &_estack,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [4 ... 14] = Default_Handler,
    [15] = SysTick_Handler,
    [16 ... 16 + N_IRQ - 1] = Default_Handler,
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;) {
        *dst++ = *src++;
    }
    for (uint32_t *dst = &_sbss; dst < &_ebss;) {
        *dst++ = 0;
    }
    (void)main();
    for (;;) {
    }
}

void Default_Handler(void)
{
    for (;;) {
    }
}

/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⢀⣠⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡴⠛⢦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⠏⠁⠉⠳⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⡾⠀⠈⠳⣄⠀⠀⠀⠀⠀⢀⣴⠟⠁⠀⠘⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣄⡇⠀⠀⠀⠀⠘⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⢠⠇⠀⠀⠀⠈⢧⡀⠀⣠⠴⠊⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⠀⠀⠀⣴⠖⠲⢬⣳⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢀⡾⠀⠀⠀⠀⠀⢉⣷⠚⠁⠀⠀⠀⠀⠀⠀⢸⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⡇⠀⠀⢰⠃⠀⠀⠀⠈⠙⢿⡂⠀⠀⠀⠀⠀⠀⠀
⠀⠀⣰⠏⠀⠀⠀⠀⠀⠀⢼⠉⡇⠀⠀⠀⠀⠀⠀⢠⡎⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣇⠀⠀⡾⠀⠀⠀⠀⠀⠀⣰⠟⠦⣀⠀⠀⠀⠀⠀
⠀⡼⠁⠀⠀⠀⠀⠀⠀⠀⠸⡄⢻⡀⠀⠀⠀⠀⢀⠞⠀⢀⣀⠀⠀⠀⠀⠀⠀⣶⡄⠀⠀⠀⠀⢸⠀⠀⡇⠀⠀⠀⠀⣠⠞⠁⠀⠀⠘⡇⠀⠀⠀⠀
⠀⢧⡀⠀⠀⠀⠀⠀⠀⠀⠀⢳⣸⡇⠀⠀⠀⣴⣏⡴⠚⠉⠙⠃⠀⠀⠀⣀⡀⠙⠦⣄⠀⠀⠀⠘⡇⢠⡇⠀⠀⢠⠜⠁⠀⢀⣀⣠⣼⣃⣀⡀⠀⠀
⠀⠀⠉⢙⣲⣤⣤⡤⠤⢤⡀⠀⢳⣧⠀⣠⢞⡷⢋⡴⠖⠙⣷⠀⠀⠀⠀⠻⢧⣀⡀⠈⠙⢦⡀⠀⣷⢸⠇⠀⡴⢃⣠⠖⣺⡭⠛⠁⠀⠀⠈⠉⠳⡄
⢠⡶⠊⠉⠀⠀⠀⠈⠉⠙⠃⠀⠈⣿⣶⣱⣿⠟⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠙⠲⢼⣦⣽⢿⢠⣾⢟⣫⠴⠋⠁⠀⠀⠀⠀⠀⠀⠀⠈⡗
⠸⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⣟⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣟⣏⣁⣀⣀⣀⡠⠤⠴⢒⣒⣷⠆⠀⢠⠇
⠀⠈⠙⠲⠤⣀⠀⠀⠀⠀⠀⢀⣾⠳⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⢻⡛⢦⡛⠳⠭⣍⣉⠉⠉⠀⠀⢀⣠⠏⠀
⠀⠀⠀⠀⠀⠈⠳⢤⣀⣀⡴⠋⣇⣴⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⡙⢦⠀⠀⠉⠉⠒⠒⠒⠋⠁⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠳⣼⡖⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⢰⠋⠀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠏⠀⠙⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⢠⢸⡄⠀⠀⠀⠙⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡞⠀⠀⠀⠘⠦⣄⡀⠀⠀⢀⡤⠴⠲⠤⣤⡀⠀⠀⠀⠀
⠀⢸⣸⡇⠀⠀⠀⠀⠈⢧⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⣀⣠⠤⠦⣄⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⠀⠀⠀⠀⠹⣤⠞⠉⠀⠀⠀⠀⣾⡇⠀⠀⠀⠀
⠀⢸⡏⠀⠀⠀⠀⠀⠀⠘⡆⠀⢰⡏⣠⠖⠀⠀⣠⠶⠋⠁⠀⠀⣠⠟⠀⠀⠀⠀⠀⠀⠸⡇⠀⠀⠀⠀⠀⢀⣼⠋⠀⠀⠀⠀⠀⠀⢿⡀⠀⠀⠀⠀
⠀⠈⢳⠀⠀⠀⠀⠀⠀⠀⢸⡄⢸⣻⠁⠀⣠⠞⠁⠀⠀⠀⢀⡴⠃⠀⠀⠀⠀⠀⠀⠀⠀⢳⠀⠀⠀⠀⣠⣻⠏⠀⠀⠀⠀⠀⠀⠀⠈⢧⡀⠀⠀⠀
⠀⠀⠈⠳⠤⣀⣀⠀⠀⠀⠀⣧⢸⡇⢀⠜⠁⠀⠀⠀⠀⣰⠋⠀⠀⠀⠀⠀⣀⣀⠀⠀⠀⠘⣆⠀⠀⣰⣳⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀
⠀⢀⣤⠒⠒⠚⠛⠻⠷⠶⣶⣾⣾⣷⠋⠀⠀⠀⠀⠀⣰⠃⠀⠀⠀⠀⠀⠀⢿⠉⠙⠲⣄⠀⠹⡄⣴⡽⠃⠀⠀⠀⠀⠀⠀⣀⣤⣤⣴⠾⣥⣀⠀⠀
⠀⢿⡅⠀⠀⠀⠀⠀⠀⠀⣼⣿⠏⠓⢶⣦⣀⣀⣀⡼⠁⠀⠀⠀⠀⠀⠀⠀⠈⣞⠉⠙⠲⢿⣶⣷⡿⠁⠀⠀⠀⣠⠴⠚⣉⠤⠞⠉⠀⠀⠀⢸⣧⠀
⠀⠸⢯⠀⠀⠀⠀⠀⢀⡾⢻⣿⠀⠀⠀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠀⠀⠀⠀⠀⠛⣿⢦⣄⡀⠘⠓⠉⠉⠀⠀⠀⠀⠀⠀⠀⣼⠟⠀
⠀⠀⠘⢦⣀⣀⡤⠞⢹⣠⠏⡇⠀⠀⠀⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢧⡙⠫⢟⡓⠦⠤⣄⣀⠀⠀⠀⢀⠞⠁⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠘⠻⠀⡇⠀⠀⣀⣤⠖⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠢⢤⣉⣦⠀⠀⠉⠙⠒⠒⠋⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/
