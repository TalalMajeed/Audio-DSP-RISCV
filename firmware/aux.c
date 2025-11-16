#include <stdint.h>
#include "aux.h"

/* --------------------------------------------------------------------
 * AUX audio extension helpers
 *
 * All AUX instructions use the CUSTOM-0 major opcode (0x0b) and
 * R-type encoding. funct7 selects the specific audio operation.
 * See picorv32_pcpi_audio in picorv32.v for semantics.
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

uint32_t aux_mac16(uint32_t a, uint32_t b)
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

uint32_t aux_abs16(uint32_t x)
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

uint32_t aux_abs2(uint32_t x)
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

uint32_t aux_msub16(uint32_t a, uint32_t b)
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

uint32_t aux_conv4(uint32_t x_packed, uint32_t h_packed)
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

uint32_t aux_conv8(uint32_t x_packed, uint32_t h_packed)
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

uint32_t aux_lmsstep(uint32_t x_packed, uint32_t h_packed)
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

uint32_t aux_cmac(uint32_t x_complex, uint32_t h_complex)
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

uint32_t aux_clip16(uint32_t x, int16_t limit)
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

uint32_t aux_shiftn(uint32_t x, uint32_t shamt)
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

