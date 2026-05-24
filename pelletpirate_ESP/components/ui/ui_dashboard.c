// In Cook Dashboard
//
// Encoder: rotate = ±5°F on target. Press = confirm.
// Continue rotating past target → focus moves to Main button.
// Press Main → back to menu. Long press → back to menu.

#include "ui_dashboard.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_dash";

static lv_obj_t *s_screen;
static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_current;
static lv_obj_t *s_lbl_target;
static lv_obj_t *s_btn_main;

typedef struct {
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_temp;
    lv_obj_t *lbl_alarm;
    lv_obj_t *lbl_goal;
} probe_row_t;

static probe_row_t s_probes[NUM_MEAT_PROBES];

typedef enum {
    DASH_FOCUS_TARGET,
    DASH_FOCUS_MAIN,
} dash_focus_t;

typedef enum {
    DASH_MODE_NAV,      // rotate moves focus between Target and Main
    DASH_MODE_ADJUST,   // rotate adjusts target ±5°
} dash_mode_t;

static dash_focus_t s_focus;
static dash_mode_t s_mode;
static int s_adj_target;
static lv_timer_t *s_enc_timer = NULL;

static void go_main_direct(void)
{
    if (s_enc_timer) { lv_timer_delete(s_enc_timer); s_enc_timer = NULL; }
    ui_encoder_set_direct(false);
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    lv_scr_load(menu);
}

static void update_focus_visual(void)
{
    if (s_focus == DASH_FOCUS_TARGET) {
        lv_obj_set_style_text_color(s_lbl_target,
            s_mode == DASH_MODE_ADJUST ? UI_COLOR_GREEN : UI_COLOR_ACCENT, 0);
        // Show border around target when focused
        lv_obj_set_style_border_width(s_lbl_target, s_mode == DASH_MODE_ADJUST ? 2 : 1, 0);
        lv_obj_set_style_border_color(s_lbl_target, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_opa(s_lbl_target, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
    } else {
        lv_obj_set_style_text_color(s_lbl_target, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_opa(s_lbl_target, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x331800), 0);
    }
}

static void dash_encoder_timer_cb(lv_timer_t *timer)
{
    if (!s_screen) return;

    int diff = encoder_get_diff();
    encoder_btn_event_t btn = encoder_get_button_event();

    if (s_mode == DASH_MODE_NAV) {
        // Navigation mode: rotate moves focus between Target and Main
        if (diff != 0) {
            if (s_focus == DASH_FOCUS_TARGET && diff > 0) {
                s_focus = DASH_FOCUS_MAIN;
            } else if (s_focus == DASH_FOCUS_MAIN && diff < 0) {
                s_focus = DASH_FOCUS_TARGET;
            }
            update_focus_visual();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_focus == DASH_FOCUS_TARGET) {
                // Enter adjustment mode
                s_mode = DASH_MODE_ADJUST;
                update_focus_visual();
            } else {
                // Main button — go back
                go_main_direct();
                return;
            }
        }
    } else {
        // Adjustment mode: rotate changes target ±5°
        if (diff != 0) {
            s_adj_target += diff * 5;
            if (s_adj_target < TARGET_TEMP_MIN) s_adj_target = TARGET_TEMP_MIN;
            if (s_adj_target > TARGET_TEMP_MAX) s_adj_target = TARGET_TEMP_MAX;
            lv_label_set_text_fmt(s_lbl_target, "%d\xC2\xB0""F", s_adj_target);
        }
        if (btn == ENCODER_BTN_SHORT) {
            // Confirm adjustment, back to nav mode
            ESP_LOGI(TAG, "Target confirmed: %d", s_adj_target);
            grill_state_lock();
            grill_state_get()->grill_target = s_adj_target;
            grill_state_unlock();
            s_mode = DASH_MODE_NAV;
            update_focus_visual();
        }
    }

    if (btn == ENCODER_BTN_LONG) {
        go_main_direct();
    }
}

lv_obj_t *ui_dashboard_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    grill_state_lock();
    s_adj_target = grill_state_get()->grill_target;
    grill_state_unlock();

    s_mode = DASH_MODE_NAV;
    s_focus = DASH_FOCUS_TARGET;

    int y = 4;

    // Cook Mode
    s_lbl_mode = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_mode, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(s_lbl_mode, 8, y);
    y += 22;

    // CURRENT
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "CURRENT");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 16;

    s_lbl_current = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_current, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_current, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_current, LV_ALIGN_TOP_MID, 0, y);
    y += 56;

    // TARGET
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "TARGET");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 16;

    s_lbl_target = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_target, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_target, UI_COLOR_ACCENT, 0);
    lv_obj_align(s_lbl_target, LV_ALIGN_TOP_MID, 0, y);
    y += 40;

    // Divider
    lv_obj_t *div = lv_obj_create(s_screen);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, 304, 2);
    lv_obj_set_pos(div, 8, y);
    lv_obj_set_style_bg_color(div, UI_COLOR_BAR_BG, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    y += 6;

    // Probe rows
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        s_probes[i].lbl_name = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_name, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_name, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_probes[i].lbl_name, 8, y);

        s_probes[i].lbl_temp = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_temp, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_temp, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_probes[i].lbl_temp, 250, y);
        y += 20;

        s_probes[i].lbl_alarm = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_alarm, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_alarm, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(s_probes[i].lbl_alarm, 16, y);

        s_probes[i].lbl_goal = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_goal, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_goal, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(s_probes[i].lbl_goal, 230, y);
        y += 22;
    }

    // Main button
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    s_btn_main = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_main, 100, 32);
    lv_obj_align(s_btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_main, 2, 0);
    lv_obj_set_style_radius(s_btn_main, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_main, 0, 0);
    lv_obj_t *l1 = lv_label_create(s_btn_main);
    lv_label_set_text(l1, "Main");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l1, UI_COLOR_ACCENT, 0);
    lv_obj_center(l1);

    ui_encoder_set_direct(true);
    encoder_get_button_event();
    encoder_get_diff();
    s_enc_timer = lv_timer_create(dash_encoder_timer_cb, 80, NULL);

    ui_dashboard_update();
    update_focus_visual();

    return s_screen;
}

void ui_dashboard_update(void)
{
    if (!s_screen) return;

    grill_state_lock();
    grill_state_t *gs = grill_state_get();

    lv_label_set_text_fmt(s_lbl_mode, "Cook Mode: %s", grill_mode_name(gs->mode));
    lv_label_set_text_fmt(s_lbl_current, "%.0f\xC2\xB0""F", gs->grill_temp);

    if (s_mode != DASH_MODE_ADJUST) {
        s_adj_target = gs->grill_target;
        lv_label_set_text_fmt(s_lbl_target, "%d\xC2\xB0""F", gs->grill_target);
    }

    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (gs->probes[i].enabled) {
            lv_label_set_text_fmt(s_probes[i].lbl_name, "%d %s", i + 1, gs->probes[i].food_type);
            lv_label_set_text_fmt(s_probes[i].lbl_temp, "%.0f\xC2\xB0""F", gs->probes[i].current_temp);
            if (gs->probes[i].alarm_temp > 0) {
                char buf[48];
                snprintf(buf, sizeof(buf), "Alarm: %.0f\xC2\xB0""F \xE2\x80\x93 %s",
                         gs->probes[i].alarm_temp, gs->probes[i].alarm_type);
                lv_label_set_text(s_probes[i].lbl_alarm, buf);
            } else {
                lv_label_set_text(s_probes[i].lbl_alarm, "Alarm: not set");
            }
            lv_label_set_text_fmt(s_probes[i].lbl_goal, "Goal %.0f\xC2\xB0""F", gs->probes[i].target_temp);
        } else {
            lv_label_set_text_fmt(s_probes[i].lbl_name, "%d not set", i + 1);
            lv_label_set_text(s_probes[i].lbl_temp, "0\xC2\xB0""F");
            lv_label_set_text(s_probes[i].lbl_alarm, "Alarm: not set");
            lv_label_set_text(s_probes[i].lbl_goal, "");
        }
    }

    grill_state_unlock();
}
