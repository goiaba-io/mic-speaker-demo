#include <math.h>

#include "audio.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "filters.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "media.h"
#include "mic.h"
#include "utils.h"

static const char *TAG = "APP";

#define OPUS_MAX_PACKET_SIZE 1276

static void gpio_init_output(gpio_num_t pin) {
    gpio_config_t io = {.pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io);
}

static void gpio_init_input_pullup(gpio_num_t pin) {
    gpio_config_t io = {.pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io);
}

static inline bool button_pressed(void) {
    return gpio_get_level(BTN_PIN) == 0; /* active-low */
}

// Buffers
static int32_t *micBuf = NULL;
static int16_t *filteredBuf = NULL;
static uint8_t *encodedBuf = NULL;

static size_t idx = 0;
static int64_t t_start = 0; /* µs */

typedef enum { IDLE, REC, PLAY } state_t;
static state_t state = IDLE;

static esp_err_t i2s_write_wrapper(int16_t *samples, size_t count,
    size_t *bytes_written) {
    spk_play(samples, count);
    *bytes_written = count * sizeof(int16_t);
    return ESP_OK;
}

// Simple wrapper to pass to `audio_encode()`
static void encoded_callback(uint8_t *data, size_t size) {
    audio_decode(data, size, i2s_write_wrapper);
}

static void main_task(void *arg) {
    ESP_LOGI(TAG, "Ready");

    while (true) {
        switch (state) {
            case IDLE:
                if (button_pressed()) {
                    ESP_LOGI(TAG, "Recording...");
                    gpio_set_level(LED_PIN, 1);
                    idx = 0;
                    t_start = esp_timer_get_time();
                    state = REC;
                }
                break;

            case REC:
                reset_voice_filters();
                if (idx < BUF_SAMPLES)
                    idx += mic_record(micBuf + idx, BUF_SAMPLES - idx);

                bool timeout = (esp_timer_get_time() - t_start) >
                               (REC_SECONDS * 1000000ULL);

                if (button_pressed() || timeout) {
                    ESP_LOGI(TAG, "End of recording");
                    gpio_set_level(LED_PIN, 0);

                    // Filter + convert
                    for (size_t i = 0; i < BUF_SAMPLES; ++i) {
                        int32_t raw = micBuf[i];
                        raw = dc_block_filter(raw);
                        raw = wind_highpass_filter(raw);
                        filteredBuf[i] =
                            limit_amplitude(raw >> 11);  // Convert to 16-bit
                    }

                    vTaskDelay(pdMS_TO_TICKS(1000)); /* 1-s pause */
                    idx = 0;
                    state = PLAY;
                }
                break;

            case PLAY:
                ESP_LOGI(TAG, "Encoding → Decoding → Playing...");
                for (size_t i = 0; i < BUF_SAMPLES; i += 320) {
                    audio_encode(filteredBuf + i, 320, encoded_callback);
                }
                ESP_LOGI(TAG, "Done");
                state = IDLE;
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    size_t psram_bytes = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM size: %u bytes", psram_bytes);

    gpio_init_input_pullup(BTN_PIN);
    gpio_init_output(LED_PIN);

    micBuf = (int32_t *)heap_caps_malloc(BUF_SAMPLES * sizeof(int32_t),
        MALLOC_CAP_8BIT);
    filteredBuf = (int16_t *)heap_caps_malloc(BUF_SAMPLES * sizeof(int16_t),
        MALLOC_CAP_8BIT);
    encodedBuf = (uint8_t *)heap_caps_malloc(OPUS_MAX_PACKET_SIZE,
        MALLOC_CAP_8BIT);  // optional, media.c allocates internally

    if (!micBuf || !filteredBuf || !encodedBuf) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        abort();
    }

    ESP_LOGI(TAG, "Heap free: %lu B", esp_get_free_heap_size());

    mic_begin();
    spk_begin();
    init_audio_encoder();
    init_audio_decoder();

    xTaskCreatePinnedToCore(main_task,
        "main_task",
        32 * 1024,
        NULL,
        4,
        NULL,
        0);
}
