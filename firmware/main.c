#include <stdint.h>
#include "aux.h"
#include "wav_demo.h"

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

/* Pack/unpack helpers for 16-bit lanes. */
static inline uint32_t pack16(int16_t lo, int16_t hi)
{
    return (uint16_t)lo | ((uint32_t)(uint16_t)hi << 16);
}

/* Compare a 4-byte tag with four literal chars. */
static int tag_eq(const char tag[4], char c0, char c1, char c2, char c3)
{
    return tag[0] == c0 && tag[1] == c1 && tag[2] == c2 && tag[3] == c3;
}

/* --------------------------------------------------------------------
 * Core noise cleaning on 16-bit mono PCM using AUX opcodes.
 * - Uses all 10 AUX helpers from aux.h.
 * - Processes in-place: samples[] overwritten with cleaned output.
 * ------------------------------------------------------------------*/
static void noise_clean_samples(int16_t *samples, uint32_t num_samples)
{
    int32_t noise_energy_est = 0;
    uint8_t env_hist[4] = {0, 0, 0, 0};
    uint32_t env_idx = 0;
    const uint32_t h_conv = 0x01010101u; /* 4-tap boxcar for CONV4/CONV8 */
    const int16_t clip_limit = 30000;

    int16_t prev_x = 0;
    int16_t prev_diff = 0;
    int16_t prev2_diff = 0;
    int16_t lms_c0 = 0;
    int16_t lms_c1 = 0;

    for (uint32_t i = 0; i < num_samples; i++) {
        int16_t x = samples[i];

        /* 1) Soft-clip input to avoid overflow (CLIP16). */
        uint32_t x_pack = pack16(x, 0);
        uint32_t x_clip_pack = aux_clip16(x_pack, clip_limit);
        int16_t x_clipped = (int16_t)(x_clip_pack & 0xFFFF);

        /* 2) Basic magnitude and energy metrics (ABS16 + ABS2). */
        uint32_t abs_pack = aux_abs16(x_clip_pack);
        int16_t abs_x = (int16_t)(abs_pack & 0xFFFF);

        uint32_t complex_pack = pack16(x_clipped, 0);
        uint32_t energy_u32 = aux_abs2(complex_pack); /* x^2 */
        int32_t energy = (int32_t)energy_u32;

        /* 3) Slow noise floor estimate using SHIFTN for smoothing. */
        int32_t diff_e = energy - noise_energy_est;
        uint32_t diff_bits = (uint32_t)diff_e;
        uint32_t delta_bits = aux_shiftn(diff_bits, 6u); /* divide by 64 with rounding */
        int32_t delta_e = (int32_t)delta_bits;
        noise_energy_est += delta_e;
        if (noise_energy_est < 0)
            noise_energy_est = 0;
        if (noise_energy_est > (1 << 30))
            noise_energy_est = (1 << 30);

        /* 4) Short-term envelope via 4-sample boxcar (CONV4/CONV8). */
        uint8_t env8 = (uint8_t)((abs_x >> 8) & 0xFF);
        env_hist[env_idx] = env8;
        env_idx = (env_idx + 1u) & 3u;

        uint32_t env_packed =
            (uint32_t)env_hist[0] |
            ((uint32_t)env_hist[1] << 8) |
            ((uint32_t)env_hist[2] << 16) |
            ((uint32_t)env_hist[3] << 24);

        int32_t env4 = (int32_t)aux_conv4(env_packed, h_conv);
        int32_t env8sum = (int32_t)aux_conv8(env_packed, h_conv);
        int32_t env_sum = env4 + env8sum;

        uint32_t env_sum_bits = (uint32_t)env_sum;
        uint32_t env_avg_bits = aux_shiftn(env_sum_bits, 3u); /* divide by 8 */
        int32_t env_avg = (int32_t)env_avg_bits;
        if (env_avg < 0)
            env_avg = 0;

        /* Early-out gate: treat very low-level regions as silence. */
        if (env_avg == 0) {
            samples[i] = 0;
            prev_x = x_clipped;
            prev_diff = 0;
            prev2_diff = 0;
            lms_c0 = 0;
            lms_c1 = 0;
            continue;
        }

        /* 5) Simple 2-tap high-pass: y = x - prev_x (MAC16). */
        uint32_t hp_x_pack = pack16(x_clipped, prev_x);
        uint32_t hp_h_pack = pack16(1, -1);
        int32_t hp_out = (int32_t)aux_mac16(hp_x_pack, hp_h_pack);

        /* 6) Rough DC estimate via MSUB16: sum ≈ x + prev_x. */
        uint32_t sum_u = aux_msub16(hp_x_pack, hp_h_pack);
        int32_t sum_dc = (int32_t)sum_u;

        /* 7) Two-tap predictor on recent high-pass output (LMSSTEP). */
        uint32_t lms_x_pack = pack16(prev_diff, prev2_diff);
        uint32_t lms_h_pack = pack16(lms_c0, lms_c1);
        int32_t pred = (int32_t)aux_lmsstep(lms_x_pack, lms_h_pack);
        int32_t err = hp_out - pred;

        /* LMS coefficient update: very small step using SHIFTN. */
        int32_t grad = err * (int32_t)prev_diff;
        uint32_t grad_bits = (uint32_t)grad;
        uint32_t grad_scaled_bits = aux_shiftn(grad_bits, 12u);
        int16_t delta_c = (int16_t)grad_scaled_bits;
        lms_c0 = (int16_t)(lms_c0 + delta_c);
        lms_c1 = lms_c0;

        prev2_diff = prev_diff;
        prev_diff = (int16_t)hp_out;

        /* 8) Mix high-passed signal and DC estimate with CMAC. */
        uint32_t cmac_in = pack16((int16_t)hp_out, (int16_t)sum_dc);
        uint32_t cmac_coeff = pack16(0x6000, (int16_t)-0x2000); /* 0.75 - j*0.25 */
        uint32_t cmac_out = aux_cmac(cmac_in, cmac_coeff);
        int16_t mixed = (int16_t)(cmac_out & 0xFFFF); /* take real part */

        /* 9) Energy-based gate: choose gain in Q1.15. */
        uint32_t noise_u = (uint32_t)(noise_energy_est < 0 ? 0 : noise_energy_est);
        uint32_t thr1 = noise_u << 1;
        uint32_t thr2 = noise_u << 2;
        uint32_t thr3 = noise_u << 3;
        if (thr1 < noise_u) thr1 = 0xFFFFFFFFu;
        if (thr2 < noise_u) thr2 = 0xFFFFFFFFu;
        if (thr3 < noise_u) thr3 = 0xFFFFFFFFu;

        uint32_t gain_q15;
        if ((uint32_t)energy <= thr1) {
            gain_q15 = 0x0000u;      /* strongly suppress very quiet / noisy parts */
        } else if ((uint32_t)energy <= thr2) {
            gain_q15 = 0x2000u;      /* -12 dB */
        } else if ((uint32_t)energy <= thr3) {
            gain_q15 = 0x6000u;      /* -4 dB */
        } else {
            gain_q15 = 0x7FFFu;      /* near unity */
        }

        /* Mild dynamic compression from short-term envelope. */
        if (env_avg > 200 && gain_q15 > 0x6000u)
            gain_q15 = 0x6000u;

        /* 10) Apply gain using MAC16 + SHIFTN, then final CLIP16. */
        if (gain_q15 == 0) {
            samples[i] = 0;
        } else {
            uint32_t scale_x_pack = pack16(mixed, 0);
            uint32_t scale_h_pack = pack16((int16_t)gain_q15, 0);
            int32_t scaled32 = (int32_t)aux_mac16(scale_x_pack, scale_h_pack);
            uint32_t scaled_bits = aux_shiftn((uint32_t)scaled32, 15u);
            int16_t y = (int16_t)scaled_bits;

            uint32_t y_pack = pack16(y, 0);
            uint32_t y_clip_pack = aux_clip16(y_pack, 32767);
            int16_t y_clip = (int16_t)(y_clip_pack & 0xFFFF);

            samples[i] = y_clip;
        }

        prev_x = x_clipped;
    }
}

/* --------------------------------------------------------------------
 * Validate and clean a 16-bit mono WAV buffer in-place.
 * ------------------------------------------------------------------*/
static void noise_clean_wav_inplace(WavHeader *hdr, int16_t *samples)
{
    if (!tag_eq(hdr->riff_id, 'R', 'I', 'F', 'F')) return;
    if (!tag_eq(hdr->wave_id, 'W', 'A', 'V', 'E')) return;
    if (!tag_eq(hdr->fmt_id,  'f', 'm', 't', ' ')) return;
    if (!tag_eq(hdr->data_id, 'd', 'a', 't', 'a')) return;
    if (hdr->audio_format != 1u) return;      /* not PCM */
    if (hdr->bits_per_sample != 16u) return;  /* only 16-bit supported */
    if (hdr->num_channels != 1u) return;      /* mono only in this demo */

    uint32_t bytes_per_sample = (uint32_t)hdr->num_channels * (hdr->bits_per_sample / 8u);
    if (bytes_per_sample == 0)
        return;

    uint32_t num_samples = hdr->data_size / bytes_per_sample;
    noise_clean_samples(samples, num_samples);
}

/* --------------------------------------------------------------------
 * main: build a tiny test WAV, clean it using the AUX opcodes,
 *       and print a small summary over UART.
 * ------------------------------------------------------------------*/
int main(void)
{
    static ExampleWavMono16 wav_buffer;

    puts("Audio AUX noise-clean demo\n");

    noise_clean_wav_inplace(&wav_buffer.hdr, wav_buffer.samples);

    WavHeader *hdr = &wav_buffer.hdr;
    uint32_t num_samples = hdr->data_size / 2u;
    int16_t *samples = wav_buffer.samples;

    puts("Cleaned samples (first 8, hex): ");
    for (uint32_t i = 0; i < 8 && i < num_samples; i++) {
        print_hex32((uint16_t)samples[i]);
        putch(' ');
    }
    nl();

    puts("Total samples: ");
    print_uint(num_samples);
    nl();

    if (num_samples > 0) {
        puts("First sample raw16: ");
        print_raw16((uint32_t)(uint16_t)samples[0]);
        nl();
    }

    *PASS = 123456789;
    __asm__ volatile("ebreak");

    while (1)
        ;
    return 0;
}
