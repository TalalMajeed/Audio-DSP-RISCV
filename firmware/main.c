#include <stdint.h>
#include "aux.h"

#define UART ((volatile uint32_t*)0x10000000)
#define PASS ((volatile uint32_t*)0x20000000)

/* --------------------------------------------------------------------
 * Small UART helpers (single-character writes).  These are intentionally
 * tiny and avoid library code so they build with -nostdlib.
 * ------------------------------------------------------------------*/
static void putch(char c)
{
    *UART = (uint32_t)c;
}

static void puts(const char *s)
{
    while (*s) putch(*s++);
}

/* print unsigned decimal (no libc) */
static void print_uint(uint32_t x)
{
    char buf[11]; /* max 10 digits + NUL */
    int i = 0;
    if (x == 0) { putch('0'); return; }
    while (x > 0) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }
    while (i--) putch(buf[i]);
}

/* print hex (lowercase), no 0x prefix */
static void print_hex32(uint32_t x)
{
    const char *hex = "0123456789abcdef";
    int started = 0;
    for (int shift = 28; shift >= 0; shift -= 4) {
        uint8_t nib = (x >> shift) & 0xF;
        if (nib || started || shift == 0) {
            putch(hex[nib]);
            started = 1;
        }
    }
}

/* print two raw bytes (low, high) for quick comparison */
static void print_raw16(uint32_t v)
{
    putch((char)(v & 0xFF));
    putch((char)((v >> 8) & 0xFF));
}

/* newline helper */
static void nl(void) { putch('\n'); }

/* --------------------------------------------------------------------
 * main: run a simple sanity test of the AUX opcodes and print outputs
 * ------------------------------------------------------------------*/
int main(void)
{
    uint32_t a = (uint16_t)1000 | ((uint32_t)(int16_t)-2000 << 16);
    uint32_t b = (uint16_t)3000 | ((uint32_t)(int16_t)4000 << 16);

    puts("Hi\n");

    uint32_t mac    = aux_mac16(a, b);
    uint32_t msub   = aux_msub16(a, b);
    uint32_t abs16  = aux_abs16(a);
    uint32_t conv4  = aux_conv4(a, b);
    uint32_t conv8  = aux_conv8(a, b);
    uint32_t lms    = aux_lmsstep(a, b);
    uint32_t cmac   = aux_cmac(a, b);
    uint32_t abs2   = aux_abs2(a);
    uint32_t clip   = aux_clip16(a, 1200);
    uint32_t shift  = aux_shiftn(mac, 4);

    /* Print results */
    puts("mac16   = "); print_hex32(mac); nl();
    puts("msub16  = "); print_hex32(msub); nl();
    puts("abs16   = "); print_hex32(abs16); nl();
    puts("conv4   = "); print_hex32(conv4); nl();
    puts("conv8   = "); print_hex32(conv8); nl();
    puts("lms     = "); print_hex32(lms); nl();
    puts("cmac    = "); print_hex32(cmac); nl();
    puts("abs2    = "); print_uint(abs2); nl();
    puts("clip16  = "); print_hex32(clip); nl();
    puts("shiftn  = "); print_hex32(shift); nl();

    puts("raw16: ");
    print_raw16(mac);
    print_raw16(abs2);
    print_raw16(shift);
    nl();

    *PASS = 123456789;
    __asm__ volatile("ebreak");

    while (1);
    return 0;
}
