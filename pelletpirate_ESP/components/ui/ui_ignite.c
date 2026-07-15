// Start Now — Ignite (and Stop Ignite)
//
// Two-step confirmation in both directions:
// - Grill off: Start Ignite button begins ignition (disabled above 115°F)
// - Already igniting: the same screen offers a red Stop Ignite instead,
//   so an accidental menu click can't kill an ignition in progress
// Either action returns to the main menu.

#include "ui_ignite.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "cooklog.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_ignite";

static lv_obj_t *s_screen;

static void go_main(lv_event_t *e)
{
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    ui_load_screen(menu);
}

static void do_ignite(lv_event_t *e)
{
    grill_state_lock();
    grill_state_t *gs = grill_state_get();

    if (gs->grill_temp >= IGNITE_DISABLE_TEMP) {
        ESP_LOGW(TAG, "Grill too hot — ignite disabled");
        grill_state_unlock();
        return;
    }

    ESP_LOGI(TAG, "IGNITION STARTED");
    gs->mode = GRILL_MODE_START;
    gs->fan_on = true;
    gs->auger_on = true;
    gs->igniter_on = true;
    grill_state_unlock();
    cooklog_event("lcd", "MODE Off>Ignite (start)");

    go_main(e);
}

static void do_stop(lv_event_t *e)
{
    ESP_LOGI(TAG, "IGNITE STOPPED");
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    gs->mode = GRILL_MODE_OFF;
    gs->fan_on = false;
    gs->auger_on = false;
    gs->igniter_on = false;
    grill_state_unlock();
    cooklog_event("lcd", "MODE Ignite>Off (stop)");

    go_main(e);
}

lv_obj_t *ui_ignite_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    float cur_temp = gs->grill_temp;
    int cur_target = gs->grill_target;
    grill_mode_t cur_mode = gs->mode;
    bool too_hot = cur_temp >= IGNITE_DISABLE_TEMP;
    bool igniting = (cur_mode == GRILL_MODE_START);
    grill_state_unlock();

    int y = 4;

    // Cook Mode — montserrat_20 to match main menu
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text_fmt(lbl, "Cook Mode: %s", grill_mode_name(cur_mode));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 26;

    // CURRENT
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "CURRENT");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 18;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)cur_temp);
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 56;

    // TARGET
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "TARGET");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 18;

    lbl = lv_label_create(s_screen);
    lv_label_set_text_fmt(lbl, "%d\xC2\xB0""F", cur_target);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 50;

    // Info text — montserrat_20
    if (igniting) {
        lbl = lv_label_create(s_screen);
        lv_label_set_text(lbl, "IGNITION IN PROGRESS\nPress STOP IGNITE to cancel");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_RED, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 20);
    } else if (too_hot) {
        lbl = lv_label_create(s_screen);
        lv_label_set_text(lbl, "GRILL TOO HOT\nIgnite disabled above 115\xC2\xB0""F");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_RED, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 20);
    } else {
        lbl = lv_label_create(s_screen);
        lv_label_set_text(lbl, "Press START IGNITE\nto begin ignition");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 20);
    }

    // Encoder group
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);

    // Main button — solid orange + white text when focused
    lv_obj_t *btn_main = lv_button_create(s_screen);
    lv_obj_set_size(btn_main, 110, 38);
    lv_obj_align(btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(btn_main, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn_main, 2, 0);
    lv_obj_set_style_radius(btn_main, 4, 0);
    lv_obj_set_style_shadow_width(btn_main, 0, 0);
    // Focused: solid orange bg
    lv_obj_set_style_bg_color(btn_main, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn_main, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_main, go_main, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(btn_main);
    lv_label_set_text(l1, "Main");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l1);
    if (g) lv_group_add_obj(g, btn_main);

    // Action button: Stop Ignite (red) while igniting, else Start Ignite
    if (igniting || !too_hot) {
        lv_color_t accent = igniting ? UI_COLOR_RED : UI_COLOR_ACCENT;
        lv_obj_t *btn_ign = lv_button_create(s_screen);
        lv_obj_set_size(btn_ign, 170, 38);
        lv_obj_align(btn_ign, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
        lv_obj_set_style_bg_color(btn_ign, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(btn_ign, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn_ign, accent, 0);
        lv_obj_set_style_border_width(btn_ign, 2, 0);
        lv_obj_set_style_radius(btn_ign, 4, 0);
        lv_obj_set_style_shadow_width(btn_ign, 0, 0);
        lv_obj_set_style_bg_color(btn_ign, accent, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(btn_ign, LV_OPA_COVER, LV_STATE_FOCUSED);
        lv_obj_add_event_cb(btn_ign, igniting ? do_stop : do_ignite,
                            LV_EVENT_CLICKED, NULL);
        lv_obj_t *l2 = lv_label_create(btn_ign);
        lv_label_set_text(l2, igniting ? "Stop Ignite" : "Start Ignite");
        lv_obj_set_style_text_font(l2, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l2, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(l2);
        if (g) lv_group_add_obj(g, btn_ign);
        // Start defaults focused; Stop does NOT (Main is the safe default)
        if (!igniting) lv_group_focus_obj(btn_ign);
    }

    return s_screen;
}
