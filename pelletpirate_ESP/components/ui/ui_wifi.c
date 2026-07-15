// WiFi setup — scan for networks, pick one, enter the password with the
// encoder via an LVGL keyboard (rotate moves keys, click presses; the
// checkmark key connects, the hide key cancels back to the list).
//
// The scan runs in a worker task so the UI keeps rendering; an lv_timer
// polls its completion flag and, while connecting, the live WiFi status
// in grill_state. Credentials persist to NVS via webui_wifi_set_credentials.
//
// All state transitions defer via lv_async_call — rebuilding widgets from
// inside their own event handlers crashes (see ui_profiles.c).

#include "ui_wifi.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "webui.h"
#include "encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_wifi";

#define MAX_APS 8

enum { SCAN_IDLE, SCAN_RUNNING, SCAN_DONE, SCAN_FAILED };

static lv_obj_t *s_screen;
static lv_obj_t *s_body;
static lv_obj_t *s_lbl_status;
static lv_obj_t *s_btn_main;
static lv_timer_t *s_timer;

static webui_ap_t s_aps[MAX_APS];
static int s_ap_count;
static volatile int s_scan_state = SCAN_IDLE;

static char s_sel_ssid[33];
static bool s_connecting;
static int s_connect_ticks;

static void build_list(void);
static void build_password(void);
static void build_connecting(void);

// --- Scan worker (blocking esp_wifi scan must not run in the LVGL task) ---

static void scan_task(void *arg)
{
    int n = webui_wifi_scan(s_aps, MAX_APS);
    s_ap_count = n > 0 ? n : 0;
    s_scan_state = (n < 0) ? SCAN_FAILED : SCAN_DONE;
    vTaskDelete(NULL);
}

static void start_scan(void)
{
    if (s_scan_state == SCAN_RUNNING) return;
    s_scan_state = SCAN_RUNNING;
    lv_label_set_text(s_lbl_status, "Scanning...");
    xTaskCreate(scan_task, "wifi_scan", 4096, NULL, 3, NULL);
}

// --- Deferred state transitions ---

static void async_list(void *unused)      { if (s_screen) build_list(); }
static void async_password(void *unused)  { if (s_screen) build_password(); }
static void async_connecting(void *unused){ if (s_screen) build_connecting(); }
static void async_rescan(void *unused)
{
    if (!s_screen) return;
    lv_obj_clean(s_body);
    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_remove_all_objs(g);
        lv_group_set_editing(g, false);
        lv_group_add_obj(g, s_btn_main);
    }
    start_scan();
}

static void go_main(lv_event_t *e)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_screen = NULL;
    ui_load_screen(ui_main_menu_create());
}

// --- Network list ---

static void net_clicked(lv_event_t *e)
{
    if (!s_screen) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_ap_count) return;
    strlcpy(s_sel_ssid, s_aps[idx].ssid, sizeof(s_sel_ssid));
    if (s_aps[idx].secure) {
        lv_async_call(async_password, NULL);
    } else {
        webui_wifi_set_credentials(s_sel_ssid, "");
        lv_async_call(async_connecting, NULL);
    }
}

static void rescan_clicked(lv_event_t *e)
{
    if (!s_screen) return;
    lv_async_call(async_rescan, NULL);
}

static lv_obj_t *list_row(int y, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(s_body);
    lv_obj_set_size(btn, 304, 34);
    lv_obj_set_pos(btn, 8, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_left(btn, 6, 0);
    lv_obj_set_style_pad_right(btn, 6, 0);
    lv_obj_set_style_pad_top(btn, 2, 0);
    lv_obj_set_style_pad_bottom(btn, 2, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

static void build_list(void)
{
    s_connecting = false;
    lv_obj_clean(s_body);
    lv_group_t *g = lv_group_get_default();
    if (g) { lv_group_remove_all_objs(g); lv_group_set_editing(g, false); }

    lv_obj_clear_flag(s_btn_main, LV_OBJ_FLAG_HIDDEN);

    if (s_ap_count == 0) {
        lv_label_set_text(s_lbl_status, "No networks found");
    } else {
        lv_label_set_text(s_lbl_status, "Select your network");
    }

    int y = 0;
    for (int i = 0; i < s_ap_count; i++) {
        lv_obj_t *btn = list_row(y, net_clicked, (void *)(intptr_t)i);

        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, s_aps[i].ssid);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_width(l, 220);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *r = lv_label_create(btn);
        char buf[24];
        snprintf(buf, sizeof(buf), "%s%d",
                 s_aps[i].secure ? LV_SYMBOL_WIFI " " : "", s_aps[i].rssi);
        lv_label_set_text(r, buf);
        lv_obj_set_style_text_font(r, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(r, UI_COLOR_TEXT_DIM, 0);
        lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);

        if (g) lv_group_add_obj(g, btn);
        y += 38;
    }

    lv_obj_t *btn_rescan = list_row(y + 6, rescan_clicked, NULL);
    lv_obj_t *l = lv_label_create(btn_rescan);
    lv_label_set_text(l, "RESCAN");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, UI_COLOR_ACCENT, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    if (g) lv_group_add_obj(g, btn_rescan);

    if (g) lv_group_add_obj(g, s_btn_main);
}

// --- Password entry ---

static lv_obj_t *s_ta;

static void kb_event(lv_event_t *e)
{
    if (!s_screen) return;
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        webui_wifi_set_credentials(s_sel_ssid, lv_textarea_get_text(s_ta));
        lv_async_call(async_connecting, NULL);
    } else {  // LV_EVENT_CANCEL — hide-keyboard key
        lv_async_call(async_list, NULL);
    }
}

static void build_password(void)
{
    lv_obj_clean(s_body);
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    // Keyboard needs the full lower screen — hide Main (cancel via kb)
    lv_obj_add_flag(s_btn_main, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(s_lbl_status, "Password for %s", s_sel_ssid);

    // The app runs themeless (ui.c disables the default theme), so the
    // textarea and keyboard get no styling for free — including the
    // selected-key highlight, without which the encoder looks dead.
    s_ta = lv_textarea_create(s_body);
    lv_textarea_set_one_line(s_ta, true);
    lv_textarea_set_max_length(s_ta, 64);
    lv_textarea_set_placeholder_text(s_ta, "password");
    lv_obj_set_size(s_ta, 304, 44);
    lv_obj_set_pos(s_ta, 8, 4);
    lv_obj_set_style_text_font(s_ta, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(s_ta, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_ta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(s_ta, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_ta, 2, 0);
    lv_obj_set_style_radius(s_ta, 4, 0);
    lv_obj_set_style_pad_all(s_ta, 10, 0);
    lv_obj_set_style_text_color(s_ta, UI_COLOR_TEXT_DIM, LV_PART_TEXTAREA_PLACEHOLDER);
    // Blinking cursor bar
    lv_obj_set_style_bg_color(s_ta, UI_COLOR_ACCENT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(s_ta, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_width(s_ta, 2, LV_PART_CURSOR);

    lv_obj_t *hint = lv_label_create(s_body);
    lv_label_set_text(hint, "Rotate = move   Click = type\n"
                            LV_SYMBOL_OK " connect       " LV_SYMBOL_KEYBOARD " cancel");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(hint, 8, 58);

    lv_obj_t *kb = lv_keyboard_create(s_body);
    lv_keyboard_set_textarea(kb, s_ta);
    lv_keyboard_set_popovers(kb, false);
    lv_obj_set_size(kb, 320, 250);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    // Keyboard body
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 2, 0);
    lv_obj_set_style_pad_gap(kb, 3, 0);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, 0);
    // Keys: dark tiles, white text
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    // Encoder-selected key: inverted, orange bg + black text
    lv_obj_set_style_bg_color(kb, UI_COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_text_color(kb, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    // While physically pressed
    lv_obj_set_style_bg_color(kb, UI_COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(kb, kb_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, kb_event, LV_EVENT_CANCEL, NULL);

    if (g) {
        lv_group_add_obj(g, kb);
        lv_group_focus_obj(kb);
        lv_group_set_editing(g, true);  // rotate moves keys immediately
    }
}

// --- Connecting ---

static void back_clicked(lv_event_t *e)
{
    if (!s_screen) return;
    lv_async_call(async_list, NULL);
}

static void build_connecting(void)
{
    lv_obj_clean(s_body);
    lv_group_t *g = lv_group_get_default();
    if (g) { lv_group_remove_all_objs(g); lv_group_set_editing(g, false); }

    lv_obj_clear_flag(s_btn_main, LV_OBJ_FLAG_HIDDEN);
    s_connecting = true;
    s_connect_ticks = 0;
    lv_label_set_text_fmt(s_lbl_status, "Connecting to %s ...", s_sel_ssid);
    lv_obj_set_style_text_color(s_lbl_status, UI_COLOR_ACCENT2, 0);

    lv_obj_t *btn_back = list_row(10, back_clicked, NULL);
    lv_obj_t *l = lv_label_create(btn_back);
    lv_label_set_text(l, "BACK TO LIST");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    if (g) { lv_group_add_obj(g, btn_back); lv_group_add_obj(g, s_btn_main); }
}

// --- Poll timer: scan completion + connection progress ---

static void timer_cb(lv_timer_t *t)
{
    if (!s_screen) return;

    if (s_scan_state == SCAN_DONE || s_scan_state == SCAN_FAILED) {
        int st = s_scan_state;
        s_scan_state = SCAN_IDLE;
        if (st == SCAN_FAILED) {
            lv_label_set_text(s_lbl_status, "Scan failed - RESCAN to retry");
            s_ap_count = 0;
        }
        build_list();
        return;
    }

    if (s_connecting) {
        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        bool up = gs->wifi_connected;
        char ip[20];
        strlcpy(ip, gs->wifi_ip, sizeof(ip));
        grill_state_unlock();

        if (up && ip[0]) {
            lv_label_set_text_fmt(s_lbl_status, "Connected!  http://%s", ip);
            lv_obj_set_style_text_color(s_lbl_status, UI_COLOR_GREEN, 0);
        } else if (++s_connect_ticks == 50) {  // ~20s at 400ms
            lv_label_set_text_fmt(s_lbl_status,
                "Still trying %s ...\nWrong password? BACK TO LIST to redo", s_sel_ssid);
            lv_obj_set_style_text_color(s_lbl_status, UI_COLOR_RED, 0);
        }
    }
}

lv_obj_t *ui_wifi_create(void)
{
    ui_encoder_set_direct(false);
    encoder_get_button_event();
    encoder_get_diff();

    s_connecting = false;
    s_scan_state = SCAN_IDLE;

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_group_t *g = lv_group_get_default();
    if (g) { lv_group_remove_all_objs(g); lv_group_set_wrap(g, false); }

    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "WI-FI SETUP");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, 4);

    s_lbl_status = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_status, "");
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_status, UI_COLOR_ACCENT2, 0);
    lv_obj_set_pos(s_lbl_status, 8, 34);

    s_body = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, 320, 400);
    lv_obj_set_pos(s_body, 0, 76);
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);

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
    if (g) lv_group_add_obj(g, s_btn_main);

    s_timer = lv_timer_create(timer_cb, 400, NULL);
    start_scan();

    ESP_LOGI(TAG, "WiFi setup screen");
    return s_screen;
}
