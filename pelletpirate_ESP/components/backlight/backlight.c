#include "backlight.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "backlight";

#define BACKLIGHT_GPIO      17
#define BACKLIGHT_FREQ_HZ   25000   // TPS61165 PWM dimming band is 5-100 kHz
#define BACKLIGHT_RES       LEDC_TIMER_10_BIT
#define BACKLIGHT_DUTY_MAX  1023

#define NVS_NAMESPACE "pelletpirate"
#define NVS_KEY_LEVEL "bl_pct"

static int s_percent = 100;   // stored brightness level (10-100)
static int s_applied = 0;     // duty currently applied, as percent (0 = off)

static void apply_duty(int pct)
{
    uint32_t duty = (uint32_t)BACKLIGHT_DUTY_MAX * pct / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void backlight_init(void)
{
    // NVS may not be initialized yet this early in boot (grill_state_init
    // runs later) — nvs_flash_init() is idempotent, so call it here too.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        int32_t pct = 100;
        if (nvs_get_i32(handle, NVS_KEY_LEVEL, &pct) == ESP_OK) {
            if (pct < 10) pct = 10;
            if (pct > 100) pct = 100;
            s_percent = (int)pct;
        }
        nvs_close(handle);
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BACKLIGHT_RES,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BACKLIGHT_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    // duty 0 = CTRL held low = TPS61165 in shutdown (matches the early
    // boot GPIO hold this replaces)
    ledc_channel_config_t chan_cfg = {
        .gpio_num = BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));

    ESP_LOGI(TAG, "Backlight ready (off), saved level %d%%", s_percent);
}

void backlight_set_percent(int pct)
{
    if (pct <= 0) {
        apply_duty(0);
        s_applied = 0;
        ESP_LOGI(TAG, "Backlight off");
        return;
    }

    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;

    if (s_applied == 0) {
        // Coming from off: hold CTRL low > 2.5 ms so a device latched off
        // by OVP (open LED) resets and restarts cleanly.
        apply_duty(0);
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    apply_duty(pct);
    s_percent = pct;
    s_applied = pct;
    ESP_LOGI(TAG, "Backlight %d%%", pct);
}

void backlight_on(void)
{
    backlight_set_percent(s_percent);
}

int backlight_get_percent(void)
{
    return s_percent;
}

void backlight_save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }
    nvs_set_i32(handle, NVS_KEY_LEVEL, (int32_t)s_percent);
    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Brightness %d%% saved to NVS", s_percent);
    } else {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
}
