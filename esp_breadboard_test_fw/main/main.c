/*
 * PelletPirate ESP_Breadboard validation firmware
 * Board: PelletPirate_ESP breakout, ESP32-S3-WROOM-1U-N16R8
 * Proves: boot, 16MB flash, 8MB octal PSRAM, USB-Serial-JTAG console,
 *         BOOT button (IO0) readback.
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define BOOT_BTN_GPIO 0
#define LED_GPIO      12   /* module pin 20, external test LED */
#define LED_PERIOD_MS 1500

static const char *TAG = "BREADBOARD";

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " PelletPirate ESP_Breadboard validation build");
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "Chip: ESP32-S3, %d core(s), rev v%d.%d",
             chip.cores, chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "Features: WiFi%s%s",
             (chip.features & CHIP_FEATURE_BT) ? " + BT" : "",
             (chip.features & CHIP_FEATURE_BLE) ? " + BLE" : "");
    ESP_LOGI(TAG, "Flash: %" PRIu32 " MB (expect 16)", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "PSRAM: %u MB total, %u bytes free (expect 8 MB)",
             (unsigned)(psram_total / (1024 * 1024)), (unsigned)psram_free);

    if (psram_total >= 8 * 1024 * 1024 - 65536) {
        ESP_LOGI(TAG, "PSRAM check: PASS");
    } else {
        ESP_LOGE(TAG, "PSRAM check: FAIL — octal PSRAM not detected!");
    }

    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BOOT_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led);

    int last = 1;
    int led_on = 0;
    uint32_t seconds = 0;
    while (1) {
        int now = gpio_get_level(BOOT_BTN_GPIO);
        if (now != last) {
            ESP_LOGI(TAG, "BOOT button %s", now ? "RELEASED" : "PRESSED");
            last = now;
        }
        if (seconds % (LED_PERIOD_MS / 2 / 100) == 0) {
            led_on = !led_on;
            gpio_set_level(LED_GPIO, led_on);
        }
        if (seconds % 50 == 0) {
            ESP_LOGI(TAG, "alive %" PRIu32 " s, free heap %u, free PSRAM %u",
                     seconds / 10,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        }
        seconds++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
