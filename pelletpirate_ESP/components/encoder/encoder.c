// Rotary encoder driver for PEC16-4215F-S0024
// Quadrature decode on IO34 (A) / IO35 (B), button on IO36
// IO34/35/36 are input-only GPIOs — no pull-up available on-chip,
// external pull-ups required on the V2 board.

#include "encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "encoder";

// Encoder state (accessed from polling task only, no mutex needed)
static volatile int s_enc_diff = 0;
static volatile bool s_btn_pressed = false;
static volatile encoder_btn_event_t s_btn_event = ENCODER_BTN_NONE;

static uint8_t s_last_ab = 0;
static int64_t s_btn_press_time = 0;
static bool s_btn_was_pressed = false;
static bool s_long_press_fired = false;

// Quadrature state table: maps (old_AB << 2 | new_AB) to direction
// +1 = CW step, -1 = CCW step, 0 = no step or invalid
static const int8_t s_quad_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

static int s_log_count = 0;

static void encoder_poll_task(void *arg)
{
    // Read initial state
    s_last_ab = (gpio_get_level(ENCODER_PIN_A) << 1) | gpio_get_level(ENCODER_PIN_B);
    ESP_LOGI(TAG, "Initial state: A=%d B=%d BTN=%d",
             gpio_get_level(ENCODER_PIN_A), gpio_get_level(ENCODER_PIN_B),
             gpio_get_level(ENCODER_PIN_BTN));

    while (1) {
        // --- Quadrature decode ---
        uint8_t a = gpio_get_level(ENCODER_PIN_A);
        uint8_t b = gpio_get_level(ENCODER_PIN_B);
        uint8_t new_ab = (a << 1) | b;

        if (new_ab != s_last_ab) {
            int8_t dir = s_quad_table[(s_last_ab << 2) | new_ab];
            if (dir != 0) {
                s_enc_diff += dir;
            }
            if (s_log_count < 20) {
                ESP_LOGI(TAG, "A=%d B=%d dir=%d diff=%d", a, b, dir, s_enc_diff);
                s_log_count++;
            }
            s_last_ab = new_ab;
        }

        // --- Button handling ---
        // IO36 is active-low (pulled high externally, low when pressed)
        bool pressed = (gpio_get_level(ENCODER_PIN_BTN) == 0);
        s_btn_pressed = pressed;

        if (pressed && !s_btn_was_pressed) {
            // Button just pressed — record time
            s_btn_press_time = esp_timer_get_time();
            s_long_press_fired = false;
        }

        if (pressed && s_btn_was_pressed && !s_long_press_fired) {
            // Button held — check for long press
            int64_t held_ms = (esp_timer_get_time() - s_btn_press_time) / 1000;
            if (held_ms >= ENCODER_LONG_PRESS_MS) {
                s_btn_event = ENCODER_BTN_LONG;
                s_long_press_fired = true;
            }
        }

        if (!pressed && s_btn_was_pressed) {
            // Button just released
            if (!s_long_press_fired) {
                s_btn_event = ENCODER_BTN_SHORT;
            }
        }

        s_btn_was_pressed = pressed;

        // Poll at ~10ms — still responsive for human rotation speed,
        // and yields enough for IDLE task to feed the watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void encoder_init(void)
{
    // Configure encoder pins as inputs (no internal pull on IO34/35/36)
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

    // Pin to CPU 1 so CPU 0's IDLE task can feed the watchdog
    xTaskCreatePinnedToCore(encoder_poll_task, "encoder", 2048, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Encoder initialized (A=%d B=%d BTN=%d)",
             ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_BTN);
}

int encoder_get_diff(void)
{
    int diff = s_enc_diff;
    s_enc_diff = 0;
    return diff;
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
