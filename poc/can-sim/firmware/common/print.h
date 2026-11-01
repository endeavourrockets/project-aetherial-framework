/*
 * print.h --> tiny console helpers (there's no printf in this build)
 * each board gives board_putc() and board_ms(), these build on them
 */
#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>

void board_putc(char c); /* one character to the console UART */
uint32_t board_ms(void); /* ms since reset */

void print_str(const char *s);
void print_dec(uint32_t v);
void print_hex(uint32_t v, int digits); /* print_hex(0x100, 3) --> "100" */
void print_time(void); /* "[  1.234] " = board_ms() in seconds */

#endif

/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⡤⠤⣤⢼⣩⡥⡤⣤⠤⢄⡀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠠⣔⠊⡱⠁⢀⠎⠀⠀⠀⠑⡄⠀⠉⢆⡈⠢⢄⡀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⡔⣽⣁⣀⣸⠀⠀⠀⠀⡰⠣⣀⣀⣈⡎⠉⠣⡀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⡜⠀⢀⠀⠀⠀⢇⡠⠔⠋⠀⠀⠀⠀⠀⠀⡀⠀⢣⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⡇⠀⠑⠀⠀⡠⠀⠁⠀⣄⠀⠀⢠⠄⠀⠈⠊⠀⢸⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⢣⠀⠀⣄⠀⠈⠀⠀⠀⠉⠀⠀⠀⠁⠀⣄⠀⠀⡜⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠈⢆⠀⠉⠀⠀⠐⠅⠀⠀⠀⠪⠂⠀⠀⠉⠀⡰⠁⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠈⢆⠀⠸⠇⠀⠀⠀⣶⠀⠀⠀⠸⠃⠀⡰⠁⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢢⡀⠀⢠⡆⠀⠀⠀⢐⠄⠀⠀⡜⠁⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⢄⠀⠀⠀⣦⠀⠀⠀⢠⠊⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⠦⣀⣀⣀⡠⠒⠁⠀⠀⠀⠀⠀⠀⠀⠀
*/
