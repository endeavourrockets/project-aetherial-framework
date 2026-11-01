/*
 * print.c --> console helpers shared by all the PoC programs (see print.h)
 */
#include "print.h"

void print_str(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            board_putc('\r'); /* terminals want CR+LF */
        }
        board_putc(*s++);
    }
}

void print_dec(uint32_t v)
{
    char buf[10];
    int n = 0;
    do { /* digits come out last first --> print them backwards */
        buf[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (n > 0) {
        board_putc(buf[--n]);
    }
}

void print_hex(uint32_t v, int digits)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) {
        board_putc(hex[(v >> (4 * i)) & 0xFu]);
    }
}

void print_time(void)
{
    uint32_t ms = board_ms();
    uint32_t s = ms / 1000u;
    uint32_t frac = ms % 1000u;
    board_putc('[');
    if (s < 100u) board_putc(' ');
    if (s < 10u) board_putc(' ');
    print_dec(s);
    board_putc('.');
    board_putc((char)('0' + frac / 100u));
    board_putc((char)('0' + (frac / 10u) % 10u));
    board_putc((char)('0' + frac % 10u));
    print_str("] ");
}
