// In Cook Dashboard — matches dashboard.png mockup
//
// Nav mode: rotate moves focus between Target and Main
// Click Target: shows "< -5°  225°F  +5° >" adjustment display
// Each probe has ET/EST progress bar below it

#include "ui_dashboard.h"
#include "ui_main_menu.h"
#include "ui_set_probes.h"
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
static lv_obj_t *s_lbl_adjust;
static lv_obj_t *s_btn_main;

typedef struct {
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_temp;
    lv_obj_t *lbl_alarm;
    lv_obj_t *lbl_goal;
    lv_obj_t *bar;          // progress bar (green=ET, gray=remaining)
    lv_obj_t *lbl_et;       // "ET: H:MM" overlaid on green portion
    lv_obj_t *lbl_est;      // "EST: H:MM" overlaid on gray portion
} probe_row_t;

static probe_row_t s_probes[NUM_MEAT_PROBES];

// Focus order: 0 = target, 1-4 = probe rows, 5 = Main
enum { DASH_FOCUS_TARGET = 0, DASH_FOCUS_P1 = 1, DASH_FOCUS_MAIN = 5, DASH_FOCUS_COUNT = 6 };
typedef enum { DASH_MODE_NAV, DASH_MODE_ADJUST } dash_mode_t;

static int s_focus;
static dash_mode_t s_mode;
static int s_adj_target;
static lv_timer_t *s_enc_timer = NULL;

static void go_main_direct(void)
{
    if (s_enc_timer) { lv_timer_delete(s_enc_timer); s_enc_timer = NULL; }
    ui_encoder_set_direct(false);
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    ui_load_screen(menu);
}

static void update_adjust_display(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "< -5\xC2\xB0   %d\xC2\xB0""F   +5\xC2\xB0 >", s_adj_target);
    lv_label_set_text(s_lbl_adjust, buf);
}

static void update_focus_visual(void)
{
    if (s_mode == DASH_MODE_ADJUST) {
        lv_obj_add_flag(s_lbl_target, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lbl_adjust, LV_OBJ_FLAG_HIDDEN);
        update_adjust_display();
        lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
        for (int i = 0; i < NUM_MEAT_PROBES; i++)
            lv_obj_set_style_border_opa(s_probes[i].lbl_name, LV_OPA_TRANSP, 0);
        return;
    }

    lv_obj_clear_flag(s_lbl_target, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_lbl_adjust, LV_OBJ_FLAG_HIDDEN);

    // Target
    if (s_focus == DASH_FOCUS_TARGET) {
        lv_obj_set_style_text_color(s_lbl_target, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_lbl_target, 2, 0);
        lv_obj_set_style_border_color(s_lbl_target, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_opa(s_lbl_target, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_border_opa(s_lbl_target, LV_OPA_TRANSP, 0);
    }

    // Probe rows (focus = orange border on the name label)
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        bool f = (s_focus == DASH_FOCUS_P1 + i);
        lv_obj_set_style_border_opa(s_probes[i].lbl_name, f ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }

    // Main button
    lv_obj_set_style_bg_color(s_btn_main,
        s_focus == DASH_FOCUS_MAIN ? UI_COLOR_ACCENT : lv_color_hex(0x000000), 0);
}

static void open_probe_setup(int probe_idx)
{
    if (s_enc_timer) { lv_timer_delete(s_enc_timer); s_enc_timer = NULL; }
    ui_encoder_set_direct(false);
    s_screen = NULL;
    ui_load_screen(ui_set_probes_create_for(probe_idx));
}

static void dash_encoder_timer_cb(lv_timer_t *timer)
{
    if (!s_screen) return;
    int diff = encoder_get_diff();
    encoder_btn_event_t btn = encoder_get_button_event();

    if (s_mode == DASH_MODE_NAV) {
        if (diff != 0) {
            s_focus += (diff > 0) ? 1 : -1;
            if (s_focus < 0) s_focus = 0;
            if (s_focus >= DASH_FOCUS_COUNT) s_focus = DASH_FOCUS_COUNT - 1;
            update_focus_visual();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_focus == DASH_FOCUS_TARGET) {
                s_mode = DASH_MODE_ADJUST;
                update_focus_visual();
            } else if (s_focus >= DASH_FOCUS_P1 && s_focus < DASH_FOCUS_MAIN) {
                open_probe_setup(s_focus - DASH_FOCUS_P1); return;
            } else {
                go_main_direct(); return;
            }
        }
    } else {
        if (diff != 0) {
            s_adj_target += diff * 5;
            if (s_adj_target < TARGET_TEMP_MIN) s_adj_target = TARGET_TEMP_MIN;
            if (s_adj_target > TARGET_TEMP_MAX) s_adj_target = TARGET_TEMP_MAX;
            update_adjust_display();
        }
        if (btn == ENCODER_BTN_SHORT) {
            ESP_LOGI(TAG, "Target confirmed: %d", s_adj_target);
            grill_state_lock();
            grill_state_get()->grill_target = s_adj_target;
            grill_state_save_to_nvs();
            grill_state_unlock();
            s_mode = DASH_MODE_NAV;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", s_adj_target);
            lv_label_set_text(s_lbl_target, buf);
            update_focus_visual();
        }
    }
    if (btn == ENCODER_BTN_LONG) go_main_direct();
}

static void format_time(char *buf, int bufsize, int minutes)
{
    snprintf(buf, bufsize, "%d:%02d", minutes / 60, minutes % 60);
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

    int y = 2;

    // Cook Mode — compact
    s_lbl_mode = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_mode, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(s_lbl_mode, 8, y);
    y += 20;

    // CURRENT
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "CURRENT");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 14;

    s_lbl_current = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_current, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_current, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_current, LV_ALIGN_TOP_MID, 0, y);
    y += 50;

    // TARGET
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "TARGET");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 14;

    s_lbl_target = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_target, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_target, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_pad_left(s_lbl_target, 8, 0);
    lv_obj_set_style_pad_right(s_lbl_target, 8, 0);
    lv_obj_set_style_pad_top(s_lbl_target, 2, 0);
    lv_obj_set_style_pad_bottom(s_lbl_target, 2, 0);
    lv_obj_set_style_radius(s_lbl_target, 4, 0);
    lv_obj_align(s_lbl_target, LV_ALIGN_TOP_MID, 0, y);

    s_lbl_adjust = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_adjust, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lbl_adjust, UI_COLOR_GREEN, 0);
    lv_obj_align(s_lbl_adjust, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_add_flag(s_lbl_adjust, LV_OBJ_FLAG_HIDDEN);

    y += 50;

    // Probe rows with ET/EST bars — bold fonts for distance readability
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        // Row 1: name + temp (BOLD — montserrat_24). Name label doubles as
        // the focus target for click-to-configure (orange border when focused).
        s_probes[i].lbl_name = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_name, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_name, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(s_probes[i].lbl_name, 2, 0);
        lv_obj_set_style_border_color(s_probes[i].lbl_name, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_opa(s_probes[i].lbl_name, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(s_probes[i].lbl_name, 4, 0);
        lv_obj_set_style_pad_left(s_probes[i].lbl_name, 3, 0);
        lv_obj_set_style_pad_right(s_probes[i].lbl_name, 3, 0);
        lv_obj_set_pos(s_probes[i].lbl_name, 4, y);

        s_probes[i].lbl_temp = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_temp, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_temp, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_probes[i].lbl_temp, 230, y);
        y += 26;

        // Row 2: alarm (red) + goal (green) — no labels, color-coded
        s_probes[i].lbl_alarm = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_alarm, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_alarm, UI_COLOR_RED, 0);
        lv_obj_set_pos(s_probes[i].lbl_alarm, 8, y);

        s_probes[i].lbl_goal = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_goal, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_goal, UI_COLOR_GREEN, 0);
        lv_obj_set_pos(s_probes[i].lbl_goal, 230, y);
        y += 22;

        // Row 3: ET/EST progress bar with text overlays
        s_probes[i].bar = lv_bar_create(s_screen);
        lv_obj_set_size(s_probes[i].bar, 304, 20);
        lv_obj_set_pos(s_probes[i].bar, 8, y);
        lv_bar_set_range(s_probes[i].bar, 0, 100);
        lv_bar_set_value(s_probes[i].bar, 50, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_probes[i].bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(s_probes[i].bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(s_probes[i].bar, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(s_probes[i].bar, 2, LV_PART_INDICATOR);

        // ET label on left (over green — black text for contrast)
        s_probes[i].lbl_et = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_et, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_et, lv_color_hex(0x000000), 0);
        lv_obj_set_pos(s_probes[i].lbl_et, 12, y + 1);

        // EST label on right (over gray — black text for contrast)
        s_probes[i].lbl_est = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_probes[i].lbl_est, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_probes[i].lbl_est, lv_color_hex(0x000000), 0);
        lv_obj_set_pos(s_probes[i].lbl_est, 200, y + 1);

        y += 24;
    }

    // Main button
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    s_btn_main = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_main, 110, 34);
    lv_obj_align(s_btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_main, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_main, 2, 0);
    lv_obj_set_style_radius(s_btn_main, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_main, 0, 0);
    lv_obj_t *l1 = lv_label_create(s_btn_main);
    lv_label_set_text(l1, "Main");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l1, lv_color_hex(0xFFFFFF), 0);
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

    char buf[48];
    snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)gs->grill_temp);
    lv_label_set_text(s_lbl_current, buf);

    if (s_mode != DASH_MODE_ADJUST) {
        s_adj_target = gs->grill_target;
        snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", gs->grill_target);
        lv_label_set_text(s_lbl_target, buf);
    }

    int elapsed = grill_state_get_elapsed_minutes();

    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (gs->probes[i].enabled && gs->probes[i].target_temp > 0) {
            lv_label_set_text_fmt(s_probes[i].lbl_name, "%d %s", i + 1, gs->probes[i].food_type);
            snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)gs->probes[i].current_temp);
            lv_label_set_text(s_probes[i].lbl_temp, buf);

            if (gs->probes[i].alarm_temp > 0) {
                snprintf(buf, sizeof(buf), "%d\xC2\xB0""F:%s",
                         (int)gs->probes[i].alarm_temp, gs->probes[i].alarm_type);
                lv_label_set_text(s_probes[i].lbl_alarm, buf);
            } else {
                lv_label_set_text(s_probes[i].lbl_alarm, "");
            }

            snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)gs->probes[i].target_temp);
            lv_label_set_text(s_probes[i].lbl_goal, buf);

            // ET/EST
            int est = grill_state_get_est_minutes(i);
            char et_buf[16], est_buf[16];

            format_time(et_buf, sizeof(et_buf), elapsed);
            snprintf(buf, sizeof(buf), "ET: %s", et_buf);
            lv_label_set_text(s_probes[i].lbl_et, buf);

            if (est >= 0) {
                format_time(est_buf, sizeof(est_buf), est);
                snprintf(buf, sizeof(buf), "EST: %s", est_buf);
                lv_label_set_text(s_probes[i].lbl_est, buf);

                // Progress bar: ET / (ET + EST) as percentage
                int total = elapsed + est;
                int pct = total > 0 ? (elapsed * 100 / total) : 0;
                if (pct > 100) pct = 100;
                lv_bar_set_value(s_probes[i].bar, pct, LV_ANIM_OFF);
            } else {
                lv_label_set_text(s_probes[i].lbl_est, "EST: --:--");
                lv_bar_set_value(s_probes[i].bar, 0, LV_ANIM_OFF);
            }

            lv_obj_clear_flag(s_probes[i].bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_probes[i].lbl_et, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_probes[i].lbl_est, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text_fmt(s_probes[i].lbl_name, "%d not set", i + 1);
            lv_label_set_text(s_probes[i].lbl_temp, "0\xC2\xB0""F");
            lv_label_set_text(s_probes[i].lbl_alarm, "");
            lv_label_set_text(s_probes[i].lbl_goal, "");
            lv_label_set_text(s_probes[i].lbl_et, "");
            lv_label_set_text(s_probes[i].lbl_est, "");
            lv_bar_set_value(s_probes[i].bar, 0, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_TEXT_DIM, LV_PART_INDICATOR);
        }
    }

    // Record history periodically
    grill_state_record_history();

    grill_state_unlock();
}
