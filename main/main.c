#include "audio.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mic.h"
#include "utils.h"

static const char *TAG = "APP";

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

static int16_t *audioBuf = NULL;
static size_t idx = 0;
static int64_t t_start = 0; /* µs */

typedef enum { IDLE, REC, PLAY } state_t;
static state_t state = IDLE;

static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void main_task(void *arg) {
    ESP_LOGI(TAG, "Ready");

    while (true) {
        switch (state) {
            case IDLE:
                if (button_pressed()) {
                    ESP_LOGI(TAG, "Recording…");
                    gpio_set_level(LED_PIN, 1);
                    idx = 0;
                    t_start = esp_timer_get_time();
                    state = REC;
                }
                break;

            case REC:
                if (idx < BUF_SAMPLES)
                    idx += mic_record(audioBuf + idx, BUF_SAMPLES - idx);

                bool timeout = (esp_timer_get_time() - t_start) >
                               (REC_SECONDS * 1000000ULL);

                if (button_pressed() || timeout) {
                    ESP_LOGI(TAG, "End of recording");
                    gpio_set_level(LED_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(1000)); /* 1-s pause */
                    idx = 0;
                    state = PLAY;
                }
                break;

            case PLAY:
                ESP_LOGI(TAG, "Playing…");
                spk_play(audioBuf, BUF_SAMPLES);
                ESP_LOGI(TAG, "Done");
                state = IDLE;
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    gpio_init_input_pullup(BTN_PIN);
    gpio_init_output(LED_PIN);

    audioBuf = (int16_t *)heap_caps_malloc(BUF_SAMPLES * sizeof(int16_t),
        MALLOC_CAP_8BIT);

    if (!audioBuf) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes", BUF_SAMPLES * 2);
        abort();
    }

    ESP_LOGI(TAG, "Heap free: %lu B", esp_get_free_heap_size());

    mic_begin();
    spk_begin();

    xTaskCreatePinnedToCore(main_task,
        "main_task",
        16 * 1024,
        NULL,
        4,
        NULL,
        0);
}
