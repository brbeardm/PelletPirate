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
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "boardpins.h"
#include "hx8357d.h"
#include "backlight.h"
#include "encoder.h"
#include "grill_state.h"
#include "max31865.h"
#include "actuator.h"
#include "webui.h"
#include "cooklog.h"
#include "cookprog.h"
#include "ui.h"

static const char *TAG = "pelletpirate";

// MAX31865 chip selects: MAX1..MAX5 (V2 schematic)
// Provisional channel map: MAX1 = grill RTD, MAX2-5 = meat probes 1-4
static const int s_rtd_cs[5] = {
    BOARD_MAX1_CS, BOARD_MAX2_CS, BOARD_MAX3_CS, BOARD_MAX4_CS, BOARD_MAX5_CS
};
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
    // Only channels that produced a valid reading this session get FAULT/OK
    // audit events — an enabled probe with nothing in the jack would
    // otherwise write one bogus FAULT row into every cook log
    bool seen_valid[5] = { false };

    while (1) {
        esp_task_wdt_reset();

        // Jack assignment: logical channel 0 (grill) reads whichever jack
        // Settings selected; meat probes fill the remaining jacks in J
        // order. The map is rebuilt every cycle so a change (only allowed
        // while Off) applies atomically between full scans.
        int gj = grill_state_get_grill_jack();
        int map[5];
        map[0] = gj;
        for (int j = 0, k = 1; j < 5; j++) {
            if (j != gj) map[k++] = j;
        }

        float t[5];
        for (int i = 0; i < 5; i++) {
            float raw = max31865_get_temp_f(&s_rtd[map[i]]);
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
        grill_state_graph_record();  // rolling 1h ring for Graphs/web
        grill_state_unlock();

        // Audit-log fault transitions on channels that are actually in use
        // (empty meat-probe jacks read 0.0F permanently — that's normal)
        for (int i = 0; i < 5; i++) {
            bool fault = (t[i] <= 0.0f);
            if (!fault) seen_valid[i] = true;
            if (in_use[i] && seen_valid[i] && fault != was_fault[i]) {
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

// After an unexpected reboot (power outage, OTA apply), resume a cook that
// was in progress. Waits for the median filter to deliver a real grill
// reading, then: SHUTDOWN always resumes (the burn-off must finish — fan
// on, no fuel, regardless of temp); active cook modes resume only if the
// grill is still hot enough that the fire is plausibly alive. A cold grill
// never self-ignites — that decision needs a human at the grill.
static void resume_task(void *arg)
{
    grill_mode_t saved = GRILL_MODE_OFF;
    int target = 0;
    int64_t wall = 0;
    if (!grill_state_load_run(&saved, &target, &wall) || saved == GRILL_MODE_OFF) {
        vTaskDelete(NULL);
    }

    float temp = 0.0f;
    for (int i = 0; i < 15 && temp <= 0.0f; i++) {   // up to ~30s for a reading
        vTaskDelay(pdMS_TO_TICKS(2000));
        grill_state_lock();
        temp = grill_state_get()->grill_temp;
        grill_state_unlock();
    }

    if (saved == GRILL_MODE_SHUTDOWN) {
        ESP_LOGW(TAG, "resume: power lost during SHUTDOWN — restarting burn-off");
        cooklog_mark_resume();
        grill_state_lock();
        grill_state_get()->mode = GRILL_MODE_SHUTDOWN;
        grill_state_unlock();
        cooklog_event("auto", "RESUME Shutdown after power loss (%.0fF)", temp);
    } else if (temp >= IGNITE_DISABLE_TEMP) {
        // Ignite that was interrupted resumes as Cook — above 115F the
        // actuator would promote it immediately anyway
        grill_mode_t m = (saved == GRILL_MODE_START) ? GRILL_MODE_COOK : saved;
        ESP_LOGW(TAG, "resume: grill still %.0fF — resuming %s at %dF",
                 temp, grill_mode_name(m), target);
        cooklog_mark_resume();   // continue the interrupted cook's CSV
        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        gs->mode = m;
        gs->grill_target = target;
        gs->cook_start_wall = wall;   // ET/display keep the original start
        grill_state_unlock();
        cooklog_event("auto", "RESUME %s after power loss (%.0fF)",
                      grill_mode_name(m), temp);
    } else {
        ESP_LOGW(TAG, "resume: cook was active (%s) but grill reads %.0fF — "
                 "not auto-igniting, start manually", grill_mode_name(saved), temp);
        grill_state_persist_run(GRILL_MODE_OFF, target);
    }
    vTaskDelete(NULL);
}

#define BACKLIGHT_CTRL_GPIO BOARD_BACKLIGHT_GPIO  // TPS61165 CTRL (backlight enable)
#define HEARTBEAT_LED_GPIO  BOARD_HEARTBEAT_GPIO  // DevKit LED on V2; NC module pad on V4

static void heartbeat_timer_cb(void *arg)
{
    static bool led_on = false;
    led_on = !led_on;
    gpio_set_level(HEARTBEAT_LED_GPIO, led_on);
}

// OTA rollback: an OTA'd image must prove itself before it becomes the
// default. 60s of uptime means the watchdogged control/temp tasks are
// alive — good enough; if we crash or hang before this fires, the next
// boot reverts to the previous firmware.
static void ota_mark_valid_cb(void *arg)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(running, &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "OTA image marked valid (60s healthy)");
    }
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
        .pin_mosi = BOARD_SPI_MOSI,
        .pin_sclk = BOARD_SPI_SCLK,
        .pin_miso = BOARD_SPI_MISO,
        .pin_cs   = BOARD_LCD_CS,
        .pin_dc   = BOARD_LCD_DC,
        .pin_rst  = BOARD_LCD_RST,
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

    // Cook profile engine (needs the LittleFS mount from cooklog_init)
    cookprog_init();

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

    // Power-loss cook resume (one-shot; deletes itself when decided)
    xTaskCreate(resume_task, "resume", 3072, NULL, 3, NULL);

    // OTA self-check: mark this image valid after 60s of healthy uptime
    const esp_timer_create_args_t ota_valid_args = {
        .callback = ota_mark_valid_cb,
        .name = "ota_valid",
    };
    esp_timer_handle_t ota_valid_timer;
    if (esp_timer_create(&ota_valid_args, &ota_valid_timer) == ESP_OK) {
        esp_timer_start_once(ota_valid_timer, 60 * 1000000ULL);
    }

    // LVGL takes over the display
    ESP_LOGI(TAG, "Starting LVGL...");
    ui_init();

    // WiFi dashboard — async, LCD-first; failure never blocks the grill
    webui_init();

    ESP_LOGI(TAG, "PelletPirate V2 running.");
}
