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
#include "driver/gpio.h"
#include "hx8357d.h"
#include "encoder.h"
#include "grill_state.h"
#include "ui.h"

static const char *TAG = "pelletpirate";

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
    // during boot, so drive it LOW before any other initialization.
    gpio_set_level(BACKLIGHT_CTRL_GPIO, 0);
    gpio_config_t bl_ctrl_cfg = {
        .pin_bit_mask = 1ULL << BACKLIGHT_CTRL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl_ctrl_cfg);
    gpio_set_level(BACKLIGHT_CTRL_GPIO, 0);

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
    // Color test — RED screen for 2 seconds to verify color rendering
    ESP_LOGI(TAG, "Color test: RED (0xF800)");
    hx8357d_fill_screen(HX8357D_RED);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Color test: GREEN (0x07E0)");
    hx8357d_fill_screen(HX8357D_GREEN);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Color test: BLUE (0x001F)");
    hx8357d_fill_screen(HX8357D_BLUE);
    vTaskDelay(pdMS_TO_TICKS(2000));

    hx8357d_boot_splash();
    ESP_LOGI(TAG, "Boot splash displayed");

    // Init encoder and grill state
    encoder_init();
    grill_state_init();

    // LVGL takes over the display
    ESP_LOGI(TAG, "Starting LVGL...");
    ui_init();
    ESP_LOGI(TAG, "PelletPirate V2 running.");
}
