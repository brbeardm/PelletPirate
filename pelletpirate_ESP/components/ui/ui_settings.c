// Settings screen
//
// BRIGHTNESS: click to adjust — rotate changes level in 10% steps with the
// backlight updating live, click again to save. Main returns to the menu.

#include "ui_settings.h"
#include "ui_main_menu.h"
#include "ui_wifi.h"
#include "ui.h"
#include "ui_styles.h"
#include "backlight.h"
#include "encoder.h"
#include "grill_state.h"
#include "cooklog.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_settings";

static lv_obj_t *s_screen;
static lv_obj_t *s_btn_bright;
static lv_obj_t *s_lbl_bright_val;
static lv_obj_t *s_btn_main;
static lv_obj_t *s_btn_wifi;
static lv_obj_t *s_lbl_wifi;
static lv_timer_t *s_wifi_timer = NULL;
static lv_obj_t *s_btn_log;
static lv_obj_t *s_lbl_log_val;
static lv_obj_t *s_btn_jack;
static lv_obj_t *s_lbl_jack_val;

// Brightness adjust state
static bool s_adjusting = false;
static int s_level;
static lv_timer_t *s_adj_timer = NULL;

static void setup_group(void)
{
    lv_group_t *g = lv_group_get_default();
    if (!g) return;
    lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);
    lv_group_add_obj(g, s_btn_bright);
    lv_group_add_obj(g, s_btn_log);
    lv_group_add_obj(g, s_btn_jack);
    lv_group_add_obj(g, s_btn_wifi);
    lv_group_add_obj(g, s_btn_main);
}

static void update_jack_display(void)
{
    grill_state_lock();
    bool off = (grill_state_get()->mode == GRILL_MODE_OFF);
    grill_state_unlock();
    char buf[16];
    snprintf(buf, sizeof(buf), off ? "J%d" : "J%d (run)",
             grill_state_get_grill_jack() + 1);
    lv_label_set_text(s_lbl_jack_val, buf);
}

// Click cycles the grill RTD's jack J1..J5. Refused while the grill runs —
// the grill channel drives PID/igniter/fault-shutdown and must never be
// remapped mid-cook.
static void jack_clicked(lv_event_t *e)
{
    int next = (grill_state_get_grill_jack() + 1) % 5;
    grill_state_set_grill_jack(next);   // no-op unless mode is Off
    update_jack_display();
}

static void jack_focus_cb(lv_event_t *e)
{
    lv_obj_set_style_text_color(s_lbl_jack_val,
        lv_event_get_code(e) == LV_EVENT_FOCUSED ? lv_color_hex(0x000000)
                                                 : UI_COLOR_ACCENT, 0);
}

static void update_log_display(void)
{
    int iv = cooklog_get_interval();
    lv_label_set_text(s_lbl_log_val, iv == 0 ? "OFF" : (iv == 30 ? "30s" : "10s"));
}

// Click cycles 30s -> 10s -> OFF -> 30s
static void log_clicked(lv_event_t *e)
{
    int iv = cooklog_get_interval();
    int next = (iv == 30) ? 10 : (iv == 10) ? 0 : 30;
    cooklog_set_interval(next);
    update_log_display();
}

static void update_bright_display(void);

// Accent-colored value text would vanish on the orange focus bar — invert
// to black while the row is focused (same convention as digit editing).
static void bright_focus_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED)
        lv_obj_set_style_text_color(s_lbl_bright_val, lv_color_hex(0x000000), 0);
    else
        update_bright_display();  // restores accent, or green mid-adjust
}

static void log_focus_cb(lv_event_t *e)
{
    lv_obj_set_style_text_color(s_lbl_log_val,
        lv_event_get_code(e) == LV_EVENT_FOCUSED ? lv_color_hex(0x000000)
                                                 : UI_COLOR_ACCENT, 0);
}

static void update_bright_display(void)
{
    char buf[16];
    if (s_adjusting) {
        snprintf(buf, sizeof(buf), "< %d%% >", s_level);
        lv_obj_set_style_text_color(s_lbl_bright_val, UI_COLOR_GREEN, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", backlight_get_percent());
        lv_obj_set_style_text_color(s_lbl_bright_val, UI_COLOR_ACCENT, 0);
    }
    lv_label_set_text(s_lbl_bright_val, buf);
}

static void finish_adjust(void)
{
    if (s_adj_timer) { lv_timer_delete(s_adj_timer); s_adj_timer = NULL; }
    ui_encoder_set_direct(false);
    s_adjusting = false;

    backlight_save_to_nvs();
    ESP_LOGI(TAG, "Brightness saved: %d%%", s_level);

    update_bright_display();
    setup_group();
}

static void adjust_timer_cb(lv_timer_t *timer)
{
    if (!s_adjusting) return;
    if (ui_encoder_swallowed()) {   // alarm-ack gesture owns the encoder
        encoder_get_diff(); encoder_get_button_event();
        return;
    }

    int diff = encoder_get_diff();
    if (diff != 0) {
        s_level += (diff > 0) ? 10 : -10;
        if (s_level < 10) s_level = 10;
        if (s_level > 100) s_level = 100;
        backlight_set_percent(s_level);  // live update while rotating
        update_bright_display();
    }

    if (encoder_get_button_event() == ENCODER_BTN_SHORT) {
        finish_adjust();
    }
}

static void bright_clicked(lv_event_t *e)
{
    s_level = backlight_get_percent();
    s_adjusting = true;
    update_bright_display();

    // Remove menu from group, take direct encoder control
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    // Consume any pending events from the click that got us here
    encoder_get_button_event();
    encoder_get_diff();

    ui_encoder_set_direct(true);
    // Delay timer start slightly so the button release doesn't bleed through
    s_adj_timer = lv_timer_create(adjust_timer_cb, 80, NULL);
}

static void wifi_info_refresh(lv_timer_t *timer)
{
    char buf[96];
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    if (gs->wifi_connected && gs->wifi_ip[0]) {
        snprintf(buf, sizeof(buf), "Connected  %d dBm\nhttp://%s\npelletpirate.local",
                 gs->wifi_rssi, gs->wifi_ip);
    } else if (gs->wifi_ap_active) {
        snprintf(buf, sizeof(buf), "Setup mode - on your phone join\n"
                                   "'PelletPirate-Setup' then open\nhttp://192.168.4.1");
    } else {
        snprintf(buf, sizeof(buf), "Not connected");
    }
    grill_state_unlock();
    lv_label_set_text(s_lbl_wifi, buf);
}

static void go_main(lv_event_t *e)
{
    if (s_wifi_timer) { lv_timer_delete(s_wifi_timer); s_wifi_timer = NULL; }
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    ui_load_screen(menu);
}

static void go_wifi(lv_event_t *e)
{
    if (s_wifi_timer) { lv_timer_delete(s_wifi_timer); s_wifi_timer = NULL; }
    s_screen = NULL;
    ui_load_screen(ui_wifi_create());
}

lv_obj_t *ui_settings_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_adjusting = false;

    int y = 4;

    // Title
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "SETTINGS");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 34;

    // Brightness row
    s_btn_bright = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_bright, 304, 36);
    lv_obj_set_pos(s_btn_bright, 8, y);
    lv_obj_set_style_bg_color(s_btn_bright, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_bright, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_bright, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_bright, 2, 0);
    lv_obj_set_style_border_opa(s_btn_bright, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_btn_bright, 4, 0);
    lv_obj_set_style_pad_left(s_btn_bright, 6, 0);
    lv_obj_set_style_pad_right(s_btn_bright, 6, 0);
    lv_obj_set_style_pad_top(s_btn_bright, 2, 0);
    lv_obj_set_style_pad_bottom(s_btn_bright, 2, 0);
    lv_obj_set_style_shadow_width(s_btn_bright, 0, 0);
    lv_obj_set_style_border_opa(s_btn_bright, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(s_btn_bright, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_bright, bright_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_btn_bright, bright_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_btn_bright, bright_focus_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *l = lv_label_create(s_btn_bright);
    lv_label_set_text(l, "BRIGHTNESS");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

    s_lbl_bright_val = lv_label_create(s_btn_bright);
    lv_obj_set_style_text_font(s_lbl_bright_val, &lv_font_montserrat_20, 0);
    lv_obj_align(s_lbl_bright_val, LV_ALIGN_RIGHT_MID, 0, 0);
    update_bright_display();
    y += 48;

    // Cook logging interval (click cycles 30s -> 10s -> OFF)
    s_btn_log = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_log, 304, 36);
    lv_obj_set_pos(s_btn_log, 8, y);
    lv_obj_set_style_bg_color(s_btn_log, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_log, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_log, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_log, 2, 0);
    lv_obj_set_style_border_opa(s_btn_log, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_btn_log, 4, 0);
    lv_obj_set_style_pad_left(s_btn_log, 6, 0);
    lv_obj_set_style_pad_right(s_btn_log, 6, 0);
    lv_obj_set_style_pad_top(s_btn_log, 2, 0);
    lv_obj_set_style_pad_bottom(s_btn_log, 2, 0);
    lv_obj_set_style_shadow_width(s_btn_log, 0, 0);
    lv_obj_set_style_border_opa(s_btn_log, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(s_btn_log, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_log, log_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_btn_log, log_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_btn_log, log_focus_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *ll = lv_label_create(s_btn_log);
    lv_label_set_text(ll, "COOK LOG");
    lv_obj_set_style_text_font(ll, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ll, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(ll, LV_ALIGN_LEFT_MID, 0, 0);

    s_lbl_log_val = lv_label_create(s_btn_log);
    lv_obj_set_style_text_font(s_lbl_log_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_lbl_log_val, UI_COLOR_ACCENT, 0);
    lv_obj_align(s_lbl_log_val, LV_ALIGN_RIGHT_MID, 0, 0);
    update_log_display();
    y += 48;

    // WI-FI status (info only, not in the encoder group)
    lv_obj_t *lbl_w = lv_label_create(s_screen);
    lv_label_set_text(lbl_w, "WI-FI");
    lv_obj_set_style_text_font(lbl_w, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_w, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl_w, 14, y);
    y += 28;

    s_lbl_wifi = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_wifi, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_wifi, UI_COLOR_ACCENT2, 0);
    lv_obj_set_pos(s_lbl_wifi, 14, y);
    wifi_info_refresh(NULL);
    s_wifi_timer = lv_timer_create(wifi_info_refresh, 2000, NULL);
    y += 68;  // info label is up to 3 lines

    // WI-FI SETUP row — scan/select/password screen
    s_btn_wifi = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_wifi, 304, 36);
    lv_obj_set_pos(s_btn_wifi, 8, y);
    lv_obj_set_style_bg_color(s_btn_wifi, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_wifi, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_wifi, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_wifi, 2, 0);
    lv_obj_set_style_border_opa(s_btn_wifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_btn_wifi, 4, 0);
    lv_obj_set_style_pad_left(s_btn_wifi, 6, 0);
    lv_obj_set_style_pad_top(s_btn_wifi, 2, 0);
    lv_obj_set_style_pad_bottom(s_btn_wifi, 2, 0);
    lv_obj_set_style_shadow_width(s_btn_wifi, 0, 0);
    lv_obj_set_style_border_opa(s_btn_wifi, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(s_btn_wifi, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_wifi, go_wifi, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lw = lv_label_create(s_btn_wifi);
    lv_label_set_text(lw, "WI-FI SETUP");
    lv_obj_set_style_text_font(lw, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lw, LV_ALIGN_LEFT_MID, 0, 0);
    y += 48;

    // GRILL JACK row — which physical jack carries the pit RTD.
    // Click cycles J1..J5; only takes effect while the grill is Off.
    s_btn_jack = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_jack, 304, 36);
    lv_obj_set_pos(s_btn_jack, 8, y);
    lv_obj_set_style_bg_color(s_btn_jack, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_jack, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_jack, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_jack, 2, 0);
    lv_obj_set_style_border_opa(s_btn_jack, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_btn_jack, 4, 0);
    lv_obj_set_style_pad_left(s_btn_jack, 6, 0);
    lv_obj_set_style_pad_right(s_btn_jack, 6, 0);
    lv_obj_set_style_pad_top(s_btn_jack, 2, 0);
    lv_obj_set_style_pad_bottom(s_btn_jack, 2, 0);
    lv_obj_set_style_shadow_width(s_btn_jack, 0, 0);
    lv_obj_set_style_border_opa(s_btn_jack, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(s_btn_jack, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_jack, jack_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_btn_jack, jack_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_btn_jack, jack_focus_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *lj = lv_label_create(s_btn_jack);
    lv_label_set_text(lj, "GRILL JACK");
    lv_obj_set_style_text_font(lj, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lj, LV_ALIGN_LEFT_MID, 0, 0);

    s_lbl_jack_val = lv_label_create(s_btn_jack);
    lv_obj_set_style_text_font(s_lbl_jack_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_lbl_jack_val, UI_COLOR_ACCENT, 0);
    lv_obj_align(s_lbl_jack_val, LV_ALIGN_RIGHT_MID, 0, 0);
    update_jack_display();

    // Main button
    s_btn_main = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_main, 110, 38);
    lv_obj_align(s_btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_main, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_main, 2, 0);
    lv_obj_set_style_radius(s_btn_main, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_main, 0, 0);
    lv_obj_set_style_bg_color(s_btn_main, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(s_btn_main, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_main, go_main, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(s_btn_main);
    lv_label_set_text(l1, "Main");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l1);

    setup_group();

    return s_screen;
}
