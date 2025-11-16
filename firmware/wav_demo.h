#ifndef WAV_DEMO_H
#define WAV_DEMO_H

#include <stdint.h>

/* Maximum samples handled in the demo buffer.
 * This limits how much from input.wav we process. */
#define EXAMPLE_NUM_SAMPLES 4096u

/* Minimal 16-bit PCM WAV header (mono only). */
typedef struct {
    char     riff_id[4];       /* "RIFF" */
    uint32_t riff_size;        /* file size - 8 */
    char     wave_id[4];       /* "WAVE" */
    char     fmt_id[4];        /* "fmt " */
    uint32_t fmt_size;         /* 16 for PCM */
    uint16_t audio_format;     /* 1 = PCM */
    uint16_t num_channels;     /* 1 = mono */
    uint32_t sample_rate;      /* e.g. 16000 Hz */
    uint32_t byte_rate;        /* sr * ch * bits/8 */
    uint16_t block_align;      /* ch * bits/8 */
    uint16_t bits_per_sample;  /* 16 */
    char     data_id[4];       /* "data" */
    uint32_t data_size;        /* num_samples * block_align */
} WavHeader;

typedef struct {
    WavHeader hdr;
    int16_t   samples[EXAMPLE_NUM_SAMPLES];
} ExampleWavMono16;

#endif

