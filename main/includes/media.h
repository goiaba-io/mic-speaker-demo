#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*audio_write_cb_t)(int16_t *samples, size_t sample_count,
    size_t *bytes_written);
typedef void (*audio_send_cb_t)(uint8_t *encoded_data, size_t encoded_size);

// === Opus Decoder ===
void init_audio_decoder(void);
void audio_decode(uint8_t *data, size_t size, audio_write_cb_t i2s_write_cb);

// === Opus Encoder ===
void init_audio_encoder(void);
void audio_encode(int16_t *pcm_samples, size_t sample_count,
    audio_send_cb_t send_cb);

#ifdef __cplusplus
}
#endif
