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
#include "hx8357d.h"
#include "encoder.h"
#include "grill_state.h"
#include "ui.h"

static const char *TAG = "pelletpirate";

void app_main(void)
{
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
