#include "mic.h"

#include "driver/i2s.h"
#include "utils.h"

#define I2S_MIC_PORT I2S_NUM_0

void mic_begin(void) {
    i2s_config_t cfg = {.mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_cfg = {.bck_io_num = MIC_BCLK,
        .ws_io_num = MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA};

    i2s_driver_install(I2S_MIC_PORT, &cfg, 0, NULL);
    i2s_set_pin(I2S_MIC_PORT, &pin_cfg);
}

size_t mic_record(int32_t *dst, size_t len_samples) {
    size_t bytes = 0;
    i2s_read(I2S_MIC_PORT,
        dst,
        len_samples * sizeof(int32_t),
        &bytes,
        portMAX_DELAY);
    return bytes / sizeof(int32_t);
}
