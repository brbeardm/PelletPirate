// PelletPirate V2 - Main Application
//
// Boot sequence:
// 1. Init LCD, show boot splash (immediate visual feedback)
// 2. Init encoder and grill state
// 3. Start LVGL UI (overwrites splash with dashboard)

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "hx8357d.h"
#include "backlight.h"
#include "encoder.h"
#include "grill_state.h"
#include "max31865.h"
#include "actuator.h"
#include "webui.h"
#include "cooklog.h"
#include "ui.h"

static const char *TAG = "pelletpirate";

// MAX31865 chip selects: MAX1..MAX5 (V2 schematic)
// Provisional channel map: MAX1 = grill RTD, MAX2-5 = meat probes 1-4
static const int s_rtd_cs[5] = { 27, 13, 5, 26, 21 };
static max31865_handle_t s_rtd[5];

// Median of the last 3 raw samples per channel: one glitched SPI read or
// conversion spike can no longer reach grill_state (which treats 0.0F as
// a fault). A real fault still propagates on the second bad sample (~4s).
static float med3(float a, float b, float c)
{
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    return b;
}

static const char *s_rtd_name[5] = { "grill", "probe 1", "probe 2", "probe 3", "probe 4" };

static void temp_task(void *arg)
{
    esp_task_wdt_add(NULL);

    float hist[5][3];
    int samples = 0;
    bool was_fault[5] = { false };

    while (1) {
        esp_task_wdt_reset();

        float t[5];
        for (int i = 0; i < 5; i++) {
            float raw = max31865_get_temp_f(&s_rtd[i]);
            hist[i][samples % 3] = raw;
            t[i] = (samples >= 2) ? med3(hist[i][0], hist[i][1], hist[i][2]) : raw;
        }
        samples++;
        ESP_LOGI(TAG, "RTD: grill=%.1fF p1=%.1fF p2=%.1fF p3=%.1fF p4=%.1fF",
                 t[0], t[1], t[2], t[3], t[4]);

        bool in_use[5] = { true };  // grill channel always matters
        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        gs->grill_temp = t[0];
        for (int i = 0; i < NUM_MEAT_PROBES; i++) {
            gs->probes[i].current_temp = t[i + 1];
            in_use[i + 1] = gs->probes[i].enabled;
        }
        grill_state_unlock();

        // Audit-log fault transitions on channels that are actually in use
        // (empty meat-probe jacks read 0.0F permanently — that's normal)
        for (int i = 0; i < 5; i++) {
            bool fault = (t[i] <= 0.0f);
            if (in_use[i] && fault != was_fault[i]) {
                cooklog_event("auto", "RTD %s %s", s_rtd_name[i],
                              fault ? "FAULT" : "OK");
            }
            was_fault[i] = fault;
        }

        // Evaluate alarms against the fresh readings
        grill_state_alarms_update();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

#define BACKLIGHT_CTRL_GPIO 17  // TPS61165 CTRL (backlight enable)
#define HEARTBEAT_LED_GPIO  2   // Onboard blue LED on DevKitC

static void heartbeat_timer_cb(void *arg)
{
    static bool led_on = false;
    led_on = !led_on;
    gpio_set_level(HEARTBEAT_LED_GPIO, led_on);
}

void app_main(void)
{
    // TPS61165 backlight driver enable (CTRL): must never float or go high
    // during boot, so drive it LOW before any other initialization. The
    // triac outputs (fan/igniter/auger) get the same treatment — a floating
    // MOC3063 input on AC power is unacceptable.
    const int boot_low_pins[] = {
        BACKLIGHT_CTRL_GPIO,
        ACTUATOR_FAN_GPIO, ACTUATOR_IGNITER_GPIO, ACTUATOR_AUGER_GPIO,
    };
    for (int i = 0; i < 4; i++) {
        gpio_set_level(boot_low_pins[i], 0);
        gpio_config_t low_cfg = {
            .pin_bit_mask = 1ULL << boot_low_pins[i],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&low_cfg);
        gpio_set_level(boot_low_pins[i], 0);
    }

    // Heartbeat LED: power/alive indicator for bench debugging. Toggled at
    // 1 Hz by a periodic esp_timer so it never blocks the rest of the program.
    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << HEARTBEAT_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    const esp_timer_create_args_t heartbeat_args = {
        .callback = heartbeat_timer_cb,
        .name = "heartbeat",
    };
    esp_timer_handle_t heartbeat_timer;
    ESP_ERROR_CHECK(esp_timer_create(&heartbeat_args, &heartbeat_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(heartbeat_timer, 500 * 1000)); // toggle every 500 ms = 1 Hz blink

    // Park all MAX31865 chip selects high before the LCD starts clocking
    // the shared SPI bus, so the MAXes can't see stray traffic.
    for (int i = 0; i < 5; i++) {
        gpio_set_level(s_rtd_cs[i], 1);
        gpio_config_t cs_cfg = {
            .pin_bit_mask = 1ULL << s_rtd_cs[i],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&cs_cfg);
        gpio_set_level(s_rtd_cs[i], 1);
    }

    // LCD first — boot splash for immediate visual feedback
    hx8357d_config_t lcd_cfg = {
        .spi_host = SPI2_HOST,
        .pin_mosi = 23,
        .pin_sclk = 18,
        .pin_miso = 19,
        .pin_cs   = 25,
        .pin_dc   = 14,
        .pin_rst  = 16,
        .spi_clock_speed_hz = 4 * 1000 * 1000,
    };

    esp_err_t ret = hx8357d_init(&lcd_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed");
        return;
    }

    // LCD controller is initialized — safe to light the backlight.
    // backlight_init() takes over IO17 from the boot-time GPIO hold above
    // and drives it with LEDC PWM at the saved brightness level.
    backlight_init();
    backlight_on();
    hx8357d_boot_splash();
    ESP_LOGI(TAG, "Boot splash displayed");

    // Init encoder and grill state
    encoder_init();
    grill_state_init();

    // Cook audit logging to the LittleFS partition
    cooklog_init();

    // Init the 5 RTD converters on the shared SPI bus and start polling.
    // A failed channel logs an error and reads as 0.0F; the rest keep going.
    for (int i = 0; i < 5; i++) {
        max31865_config_t cfg = {
            .spi_host = SPI2_HOST,
            .pin_cs = s_rtd_cs[i],
            .ref_resistor = 400.0f,   // R21 etc., verified from schematic
            .rtd_nominal = 100.0f,    // PT100
        };
        max31865_init(&s_rtd[i], &cfg);
    }
    xTaskCreatePinnedToCore(temp_task, "rtd_temps", 4096, NULL, 4, NULL, 1);

    // Fan/auger/igniter control (reads grill_state, drives the triac outputs)
    actuator_init();

    // LVGL takes over the display
    ESP_LOGI(TAG, "Starting LVGL...");
    ui_init();

    // WiFi dashboard — async, LCD-first; failure never blocks the grill
    webui_init();

    ESP_LOGI(TAG, "PelletPirate V2 running.");
}
