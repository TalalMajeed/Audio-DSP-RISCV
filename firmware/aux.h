#ifndef AUX_H
#define AUX_H

#include <stdint.h>

uint32_t aux_mac16(uint32_t a, uint32_t b);
uint32_t aux_msub16(uint32_t a, uint32_t b);
uint32_t aux_abs16(uint32_t x);
uint32_t aux_abs2(uint32_t x);
uint32_t aux_conv4(uint32_t x_packed, uint32_t h_packed);
uint32_t aux_conv8(uint32_t x_packed, uint32_t h_packed);
uint32_t aux_lmsstep(uint32_t x_packed, uint32_t h_packed);
uint32_t aux_cmac(uint32_t x_complex, uint32_t h_complex);
uint32_t aux_clip16(uint32_t x, int16_t limit);
uint32_t aux_shiftn(uint32_t x, uint32_t shamt);

#endif

