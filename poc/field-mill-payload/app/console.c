/*
* console.c - see console.h, read.
*(¬_¬")
* input--> console_poll() is called on eahc main loop pass. collectd types bytes
* into line buffer, is cheked and return matchind or known command, or CMD NONE if no full line yet or the other thingy
* output--> con_ functions print the text and numbers by making them characters
* only drv_uart_getc() and drv_uart_write() (drivers.h) touch hardware, ˙𐃷˙
* so this file is the should be the same on the Nucleo, PC sim and Renode
*/
#include "console.h" /*my snippet+ console_cmd_t enum*/
#include "drivers.h" /*drv_uart_getc(), and other things ε(´｡•᎑•`)っ 💕*/

#define LINE_MAX 32

static char g_line[LINE_MAX]; /*line being typed, and keep value between calls*/
static uint32_t g_len; /*number of characters in g line, obviously*/

static char up(char c) /*condition shtick*/
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}
/*My own replacement for strcmp(a, b) == 0 (no C library). eq() true id strings a & b are equal*/
static bool eq(const char *a, const char *b)
{
    while (*a && *b) { /*until either string reaches '\0'*/
        if (*a++ != *b++) { /*compare each character, then step*/
            return false; /*first different character*/
        }
    }
    return *a == *b; /*both strings ended at same time*/
}

/*same explanaiton fo poll function as above*/
console_cmd_t console_poll(char *last_line, uint32_t max)
{
    int c; /*int, muct hols 0..255 AND -1*/
    /*read bytes until the UART has none left*/
    while ((c = drv_uart_getc()) >= 0) {
        /*the line is finished*/
        if (c == '\r' || c == '\n') {
            if (g_len == 0) {
                continue; /*line is empty and simply ignore*/
            }
            g_line[g_len] = '\0'; /*end string*/

            /*copy caller buffer, but not more than max bytes*/
            uint32_t i = 0;
            for (; i + 1 < max && g_line[i]; i++) {
                last_line[i] = g_line[i];
            }
            last_line[i] = '\0';
            g_len = 0; /*empty buffer for next line*/
            /*below it looks fro command, and the match is returned, as seen in console.h*/
            if (eq(g_line, "STATUS")) return CMD_STATUS;
            if (eq(g_line, "ARM")) return CMD_ARM;
            if (eq(g_line, "DISARM")) return CMD_DISARM;
            if (eq(g_line, "RANGE HI")) return CMD_RANGE_HI;
            if (eq(g_line, "RANGE LO")) return CMD_RANGE_LO;
            if (eq(g_line, "RANGE AUTO")) return CMD_RANGE_AUTO;
            if (eq(g_line, "VERSION")) return CMD_VERSION;
            return CMD_UNKNOWN;
            /*bytes still in UART go though porcess in next call*/
        }
        /*if line to long extra character dropped, to protect memory*/
        if (g_len < LINE_MAX - 1) {
            g_line[g_len++] = up((char)c); /*store then g_len +1*/
        }
    }
    return CMD_NONE; /*no complete line yet, duh, these comments are cus i got lost alot in the sauce*/
}

void con_str(const char *s)
{
    uint32_t n = 0;
    while (s[n]) {
        n++;
    }
    drv_uart_write(s, n);
}

/*con_u32(),print an unsigned number in decimal,
*v % 10 gives the last digit, v / 10 removes it. Digits come out
*backwards (ones first), so the buffer is filled from the right end.
*i copied this code from somwhere, but i think it works, someone pls check  ¯\_(ツ)_/¯
*/
void con_u32(uint32_t v)
{
    char b[11]; /*max 10 digits*/
    int i = 10; 
    b[i] = '\0';
    do {
        b[--i] = (char)('0' + v % 10u);
        v /= 10u; 
    } while (v && i > 0);
    con_str(&b[i]);
}

/*con_i32() - print a signed number.
*print the number without sign using con_u32().
*The int64_t cast: -2147483648 has no positive version that fits in
*32 bits, so it is negated as a 64-bit number to avoid overflow.
*some of this code is copied from some website... 😬
*/
void con_i32(int32_t v)
{
    if (v < 0) {
        con_str("-");
        con_u32((uint32_t)(-(int64_t)v));
    } else {
        con_u32((uint32_t)v);
    }
}

/*Deadass copied this from somewhere
*con_fixed() - print a fixed-point number: v / 10^decimals.
*   con_fixed(2345, 2) -> "23.45"     con_fixed(5, 2)  -> "0.05"
*   con_fixed(-7, 1)   -> "-0.7"      con_fixed(42, 0) -> "42"
*Lets me store values as whole numbers (e.g. temp_cc = 2345 = 23.45 C)
*and avoid floats. decimals must be 0..9 (10^10 doesn't fit in 32 bits).
*Note: v = -2147483648 would overflow at "v = -v" (never happens here).
*/
void con_fixed(int32_t v, int decimals)
{
    int32_t p = 1;
    for (int i = 0; i < decimals; i++) {
        p *= 10;
    }
    if (v < 0) {
        con_str("-");
        v = -v;
    }
    con_u32((uint32_t)(v / p));
    if (decimals > 0) {
        con_str(".");
        int32_t f = v % p;
        for (int32_t q = p / 10; q > 1 && f < q; q /= 10) {
            con_str("0");
        }
        con_u32((uint32_t)f);
    }
}

void con_nl(void)
{
    con_str("\r\n");
}
/*𓆝 𓆟 𓆞 𓆝 𓆟*/