// Rotary encoder driver for PEC16-4215F-S0024
// Quadrature decode on IO34 (A) / IO35 (B) via the PCNT hardware pulse
// counter — software polling dropped transitions (a quick detent click
// completes all 4 quadrature edges in <10ms). PCNT catches every edge in
// hardware, and Gray-code counting nets contact bounce to zero.
// Button on IO36 polled by task. IO34/35/36 are input-only GPIOs —
// external pull-ups required.

#include "encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "encoder";

static pcnt_unit_handle_t s_pcnt_unit;
static int s_reported_count = 0;    // count already handed out via encoder_get_diff

static volatile bool s_btn_pressed = false;
static volatile encoder_btn_event_t s_btn_event = ENCODER_BTN_NONE;

static int64_t s_btn_press_time = 0;
static bool s_btn_was_pressed = false;
static bool s_long_press_fired = false;

// PEC16-4215F-S0024: 24 detents, 24 pulses per revolution
// Each detent = one full quadrature cycle = 4 counted edges
#define STEPS_PER_DETENT 4

#define PCNT_HIGH_LIMIT  1000
#define PCNT_LOW_LIMIT  -1000

static void button_poll_task(void *arg)
{
    while (1) {
        bool pressed = (gpio_get_level(ENCODER_PIN_BTN) == 0);
        s_btn_pressed = pressed;

        if (pressed && !s_btn_was_pressed) {
            s_btn_press_time = esp_timer_get_time();
            s_long_press_fired = false;
        }

        if (pressed && s_btn_was_pressed && !s_long_press_fired) {
            int64_t held_ms = (esp_timer_get_time() - s_btn_press_time) / 1000;
            if (held_ms >= ENCODER_LONG_PRESS_MS) {
                s_btn_event = ENCODER_BTN_LONG;
                s_long_press_fired = true;
            }
        }

        if (!pressed && s_btn_was_pressed) {
            if (!s_long_press_fired) {
                s_btn_event = ENCODER_BTN_SHORT;
            }
        }

        s_btn_was_pressed = pressed;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void encoder_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ENCODER_PIN_A) |
                        (1ULL << ENCODER_PIN_B) |
                        (1ULL << ENCODER_PIN_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // PCNT unit in full quadrature (x4) mode: channel A counts edges of A
    // gated by the level of B, channel B the reverse. accum_count keeps the
    // total monotonic across the +/-1000 hardware limits.
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
        .flags.accum_count = true,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &s_pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_PIN_A,
        .level_gpio_num = ENCODER_PIN_B,
    };
    pcnt_channel_handle_t chan_a;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_a_config, &chan_a));
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_PIN_B,
        .level_gpio_num = ENCODER_PIN_A,
    };
    pcnt_channel_handle_t chan_b;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_b_config, &chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // Watch points at the limits are required for accum_count to latch
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_pcnt_unit, PCNT_HIGH_LIMIT));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_pcnt_unit, PCNT_LOW_LIMIT));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt_unit));

    xTaskCreatePinnedToCore(button_poll_task, "encoder_btn", 2048, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Encoder initialized, PCNT quadrature (A=%d B=%d BTN=%d), state A=%d B=%d BTN=%d",
             ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_BTN,
             gpio_get_level(ENCODER_PIN_A), gpio_get_level(ENCODER_PIN_B),
             gpio_get_level(ENCODER_PIN_BTN));
}

int encoder_get_diff(void)
{
    int count = 0;
    pcnt_unit_get_count(s_pcnt_unit, &count);
    int delta = count - s_reported_count;
    // Truncating division keeps the sub-detent remainder banked in hardware,
    // so a detent that straddles two reads is never lost or double-counted
    int detents = delta / STEPS_PER_DETENT;
    if (detents != 0) {
        s_reported_count += detents * STEPS_PER_DETENT;
    }
    return detents;
}

bool encoder_button_pressed(void)
{
    return s_btn_pressed;
}

encoder_btn_event_t encoder_get_button_event(void)
{
    encoder_btn_event_t evt = s_btn_event;
    s_btn_event = ENCODER_BTN_NONE;
    return evt;
}
