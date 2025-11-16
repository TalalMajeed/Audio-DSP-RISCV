#include <stdint.h>
#define UART ((volatile uint32_t*)0x10000000)
#define PASS ((volatile uint32_t*)0x20000000)

/* --------------------------------------------------------------------
 * AUX audio extension helpers (R-type .word encodings via inline asm)
 * ------------------------------------------------------------------*/

#define AUX_OPCODE 0x0b
#define AUX_FUNCT3 0x0

#define AUX_F7_MAC16   0x20
#define AUX_F7_MSUB16  0x21
#define AUX_F7_ABS16   0x22
#define AUX_F7_CONV4   0x23
#define AUX_F7_CONV8   0x24
#define AUX_F7_LMSSTEP 0x25
#define AUX_F7_CMAC    0x26
#define AUX_F7_ABS2    0x27
#define AUX_F7_CLIP16  0x28
#define AUX_F7_SHIFTN  0x29

#define AUX_ENC_R(F7, RD, RS1, RS2) \
    ((((uint32_t)(F7)  & 0x7f) << 25) | \
     (((uint32_t)(RS2) & 0x1f) << 20) | \
     (((uint32_t)(RS1) & 0x1f) << 15) | \
     (((uint32_t)AUX_FUNCT3 & 0x7) << 12) | \
     (((uint32_t)(RD)  & 0x1f) << 7)  | \
     ((uint32_t)AUX_OPCODE & 0x7f))

#define AUX_RD_A0   10u  /* x10 / a0 */
#define AUX_RS1_A0  10u
#define AUX_RS2_A1  11u  /* x11 / a1 */
#define AUX_RS2_X0   0u  /* x0  / zero */

#define AUX_MAC16_ENC    AUX_ENC_R(AUX_F7_MAC16,   AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_MSUB16_ENC   AUX_ENC_R(AUX_F7_MSUB16,  AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_ABS16_ENC    AUX_ENC_R(AUX_F7_ABS16,   AUX_RD_A0, AUX_RS1_A0, AUX_RS2_X0)
#define AUX_CONV4_ENC    AUX_ENC_R(AUX_F7_CONV4,   AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_CONV8_ENC    AUX_ENC_R(AUX_F7_CONV8,   AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_LMSSTEP_ENC  AUX_ENC_R(AUX_F7_LMSSTEP, AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_CMAC_ENC     AUX_ENC_R(AUX_F7_CMAC,    AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_ABS2_ENC     AUX_ENC_R(AUX_F7_ABS2,    AUX_RD_A0, AUX_RS1_A0, AUX_RS2_X0)
#define AUX_CLIP16_ENC   AUX_ENC_R(AUX_F7_CLIP16,  AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)
#define AUX_SHIFTN_ENC   AUX_ENC_R(AUX_F7_SHIFTN,  AUX_RD_A0, AUX_RS1_A0, AUX_RS2_A1)

/* Inline wrappers */
static inline uint32_t aux_mac16(uint32_t a, uint32_t b)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(a), "r"(b), "i"(AUX_MAC16_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_abs16(uint32_t x)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        ".word %2\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x), "i"(AUX_ABS16_ENC)
        : "a0");
    return rd;
}

static inline uint32_t aux_abs2(uint32_t x)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        ".word %2\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x), "i"(AUX_ABS2_ENC)
        : "a0");
    return rd;
}

static inline uint32_t aux_msub16(uint32_t a, uint32_t b)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(a), "r"(b), "i"(AUX_MSUB16_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_conv4(uint32_t x_packed, uint32_t h_packed)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x_packed), "r"(h_packed), "i"(AUX_CONV4_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_conv8(uint32_t x_packed, uint32_t h_packed)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x_packed), "r"(h_packed), "i"(AUX_CONV8_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_lmsstep(uint32_t x_packed, uint32_t h_packed)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x_packed), "r"(h_packed), "i"(AUX_LMSSTEP_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_cmac(uint32_t x_complex, uint32_t h_complex)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x_complex), "r"(h_complex), "i"(AUX_CMAC_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_clip16(uint32_t x, int16_t limit)
{
    uint32_t rd;
    uint32_t lim_packed = (uint16_t)limit;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x), "r"(lim_packed), "i"(AUX_CLIP16_ENC)
        : "a0", "a1");
    return rd;
}

static inline uint32_t aux_shiftn(uint32_t x, uint32_t shamt)
{
    uint32_t rd;
    __asm__ volatile (
        "mv a0, %1\n"
        "mv a1, %2\n"
        ".word %3\n"
        "mv %0, a0\n"
        : "=r"(rd)
        : "r"(x), "r"(shamt), "i"(AUX_SHIFTN_ENC)
        : "a0", "a1");
    return rd;
}

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