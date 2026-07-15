// LVGL initialization and display driver bridge

#include "ui.h"
#include "ui_styles.h"
#include "ui_splash.h"
#include "ui_main_menu.h"
#include "ui_dashboard.h"
#include "hx8357d.h"
#include "encoder.h"
#include "grill_state.h"
#include "cooklog.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "ui";

static lv_display_t *s_display;
static lv_indev_t *s_encoder_indev;

#define BUF_LINES (HX8357D_HEIGHT / 10)
#define BUF_PIXELS (HX8357D_WIDTH * BUF_LINES)
#define BYTES_PER_PIXEL 2
#define BUF_SIZE_BYTES (BUF_PIXELS * BYTES_PER_PIXEL)

// LVGL display flush callback
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    size_t len_bytes = (size_t)w * h * BYTES_PER_PIXEL;

    hx8357d_flush_area(area->x1, area->y1, area->x2, area->y2, px_map, len_bytes);
    lv_display_flush_ready(disp);
}

// When true, LVGL encoder callback is disabled — screen polls encoder directly
static bool s_encoder_direct = false;

void ui_encoder_set_direct(bool direct)
{
    s_encoder_direct = direct;
}

// LVGL encoder read callback
static void lvgl_encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (s_encoder_direct) {
        // Screen is handling encoder directly — don't consume events
        data->enc_diff = 0;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    int diff = encoder_get_diff();
    if (diff > 0) diff = 1;
    else if (diff < 0) diff = -1;
    data->enc_diff = diff;
    data->state = encoder_button_pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void ui_load_screen(lv_obj_t *new_screen)
{
    // Group edit mode (used by the WiFi keyboard) must never leak into the
    // next screen — in edit mode rotation goes to the focused widget
    // instead of moving focus, freezing all row navigation.
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_set_editing(g, false);

    lv_obj_t *old = lv_screen_active();
    lv_scr_load(new_screen);
    if (old && old != new_screen) {
        lv_obj_delete(old);
    }
}

// LVGL tick provider
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(2);
}

// Called when splash animation completes — load the main menu
static void on_splash_complete(void)
{
    ESP_LOGI(TAG, "Splash complete, loading main menu...");
    lv_obj_t *menu = ui_main_menu_create();
    ui_load_screen(menu);
}

// --- Alarm banner ---
// Overlay on lv_layer_top() so it shows above every screen and survives
// screen changes. Ack: hold the encoder button ~1.2s while the banner is
// visible (uses encoder_button_pressed() state, which does not consume
// events — note the active screen may also act on the press).

static lv_obj_t *s_alarm_banner = NULL;
static lv_obj_t *s_alarm_label = NULL;
static int64_t s_ack_hold_start = 0;

#define ALARM_ACK_HOLD_US (1200 * 1000)

static void alarm_banner_create(void)
{
    s_alarm_banner = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_alarm_banner);
    lv_obj_set_size(s_alarm_banner, 320, 46);
    lv_obj_set_pos(s_alarm_banner, 0, 0);
    lv_obj_set_style_bg_color(s_alarm_banner, UI_COLOR_RED, 0);
    lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_alarm_banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_alarm_banner, LV_OBJ_FLAG_CLICKABLE);

    s_alarm_label = lv_label_create(s_alarm_banner);
    lv_obj_set_style_text_font(s_alarm_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_alarm_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(s_alarm_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_alarm_label);
}

static void ui_alarm_update(void)
{
    if (!grill_state_alarm_active()) {
        if (s_alarm_banner) lv_obj_add_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
        s_ack_hold_start = 0;
        return;
    }

    if (!s_alarm_banner) alarm_banner_create();

    char txt[64];
    char full[112];
    if (grill_state_alarm_text(txt, sizeof(txt))) {
        snprintf(full, sizeof(full), "%s\nHold button to silence", txt);
        lv_label_set_text(s_alarm_label, full);
    }

    // Blink between red and dark red so it reads as an alert
    bool bright = (lv_tick_get() / 600) % 2 == 0;
    lv_obj_set_style_bg_color(s_alarm_banner,
        bright ? UI_COLOR_RED : lv_color_hex(0x661111), 0);
    lv_obj_clear_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);

    // Hold-to-acknowledge
    if (encoder_button_pressed()) {
        if (s_ack_hold_start == 0) {
            s_ack_hold_start = esp_timer_get_time();
        } else if (esp_timer_get_time() - s_ack_hold_start > ALARM_ACK_HOLD_US) {
            grill_state_alarm_ack();
            cooklog_event("lcd", "ALARM ACK");
            s_ack_hold_start = 0;
        }
    } else {
        s_ack_hold_start = 0;
    }
}

// LVGL task
static void lvgl_task(void *arg)
{
    while (1) {
        // Update active screen data
        ui_main_menu_update();
        ui_dashboard_update();
        ui_alarm_update();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(16));  // ~60fps
    }
}

void ui_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL...");

    lv_init();

    // Display setup
    s_display = lv_display_create(HX8357D_WIDTH, HX8357D_HEIGHT);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565_SWAPPED);

    void *buf1 = heap_caps_malloc(BUF_SIZE_BYTES, MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(BUF_SIZE_BYTES, MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers");
        return;
    }

    lv_display_set_buffers(s_display, buf1, buf2,
                           BUF_SIZE_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);

    // Disable default theme to prevent blue cursor/focus artifacts
    lv_display_set_theme(s_display, NULL);

    // Encoder input
    s_encoder_indev = lv_indev_create();
    lv_indev_set_type(s_encoder_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(s_encoder_indev, lvgl_encoder_read_cb);

    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(s_encoder_indev, g);

    // Tick timer (2ms)
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 2000);

    // Init styles
    ui_styles_init();

    // Show splash, then transition to COOK screen
    ui_splash_create(on_splash_complete);

    // Start LVGL handler task
    xTaskCreate(lvgl_task, "lvgl", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "LVGL initialized, splash screen active");
}

lv_indev_t *ui_get_encoder_indev(void)
{
    return s_encoder_indev;
}
