// Set Target Temp — digit-by-digit entry
//
// 3-digit display: [X][X][X]°F
// Encoder rotates to change active digit, press to advance.
// 1st digit: 1-4, 2nd: 0-9, 3rd: 0-9
// Range: 100-499°F
// Main = discard, Save = apply

#include "ui_set_target.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_target";

static lv_obj_t *s_screen;
static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_current;
static lv_obj_t *s_digits[3];       // the 3 digit labels
static lv_obj_t *s_digit_boxes[3];  // highlight boxes behind digits
static lv_obj_t *s_btn_main;
static lv_obj_t *s_btn_save;

static int s_digit_vals[3];   // current digit values
static int s_active_digit;    // which digit is being edited (0-2), 3 = buttons
static lv_timer_t *s_digit_timer = NULL;

static const int digit_min[] = {1, 0, 0};
static const int digit_max[] = {4, 9, 9};

// Custom encoder handler — we manage focus ourselves for digit editing
static void screen_encoder_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_param(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_indev_get_key(indev);

        if (s_active_digit < 3) {
            // Editing a digit
            if (key == LV_KEY_RIGHT || key == LV_KEY_UP) {
                s_digit_vals[s_active_digit]++;
                if (s_digit_vals[s_active_digit] > digit_max[s_active_digit])
                    s_digit_vals[s_active_digit] = digit_min[s_active_digit];
            } else if (key == LV_KEY_LEFT || key == LV_KEY_DOWN) {
                s_digit_vals[s_active_digit]--;
                if (s_digit_vals[s_active_digit] < digit_min[s_active_digit])
                    s_digit_vals[s_active_digit] = digit_max[s_active_digit];
            }
            // Update digit display
            lv_label_set_text_fmt(s_digits[s_active_digit], "%d", s_digit_vals[s_active_digit]);
        }
    }
}

static void update_digit_highlight(void)
{
    for (int i = 0; i < 3; i++) {
        if (i == s_active_digit) {
            lv_obj_set_style_border_color(s_digit_boxes[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_border_opa(s_digit_boxes[i], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x331800), 0);
        } else {
            lv_obj_set_style_border_opa(s_digit_boxes[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x000000), 0);
        }
    }
}

static void go_main(lv_event_t *e)
{
    if (s_digit_timer) { lv_timer_delete(s_digit_timer); s_digit_timer = NULL; }
    ui_encoder_set_direct(false);
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    lv_scr_load(menu);
}

static void do_save(lv_event_t *e)
{
    int temp = s_digit_vals[0] * 100 + s_digit_vals[1] * 10 + s_digit_vals[2];
    ESP_LOGI(TAG, "Setting target to %d", temp);

    grill_state_lock();
    grill_state_get()->grill_target = temp;
    grill_state_unlock();

    go_main(e);
}

// Timer to poll encoder directly for digit editing

static void digit_edit_timer_cb(lv_timer_t *timer)
{
    if (!s_screen || s_active_digit >= 3) return;

    int diff = encoder_get_diff();
    if (diff > 0) {
        s_digit_vals[s_active_digit]++;
        if (s_digit_vals[s_active_digit] > digit_max[s_active_digit])
            s_digit_vals[s_active_digit] = digit_min[s_active_digit];
        lv_label_set_text_fmt(s_digits[s_active_digit], "%d", s_digit_vals[s_active_digit]);
    } else if (diff < 0) {
        s_digit_vals[s_active_digit]--;
        if (s_digit_vals[s_active_digit] < digit_min[s_active_digit])
            s_digit_vals[s_active_digit] = digit_max[s_active_digit];
        lv_label_set_text_fmt(s_digits[s_active_digit], "%d", s_digit_vals[s_active_digit]);
    }

    // Check button press to advance
    encoder_btn_event_t btn = encoder_get_button_event();
    if (btn == ENCODER_BTN_SHORT) {
        s_active_digit++;
        if (s_active_digit >= 3) {
            // Done with digits — stop timer, let LVGL group handle Main/Save buttons
            if (s_digit_timer) {
                lv_timer_delete(s_digit_timer);
                s_digit_timer = NULL;
            }
            // Focus Save button
            lv_group_t *g = lv_group_get_default();
            if (g) {
                lv_group_remove_all_objs(g);
                lv_group_add_obj(g, s_btn_main);
                lv_group_add_obj(g, s_btn_save);
                lv_group_focus_obj(s_btn_save);
            }
        }
        update_digit_highlight();
    }
}

lv_obj_t *ui_set_target_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Read current state
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    int cur_target = gs->grill_target;
    float cur_temp = gs->grill_temp;
    grill_mode_t cur_mode = gs->mode;
    grill_state_unlock();

    // Init digit values from current target
    s_digit_vals[0] = (cur_target / 100) % 10;
    s_digit_vals[1] = (cur_target / 10) % 10;
    s_digit_vals[2] = cur_target % 10;
    if (s_digit_vals[0] < 1) s_digit_vals[0] = 1;
    if (s_digit_vals[0] > 4) s_digit_vals[0] = 4;
    s_active_digit = 0;

    int y = 4;

    // Cook Mode status
    s_lbl_mode = lv_label_create(s_screen);
    lv_label_set_text_fmt(s_lbl_mode, "Cook Mode: %s", grill_mode_name(cur_mode));
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
    lv_label_set_text_fmt(s_lbl_current, "%.0f\xC2\xB0""F", cur_temp);
    lv_obj_set_style_text_font(s_lbl_current, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_current, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_current, LV_ALIGN_TOP_MID, 0, y);
    y += 60;

    // TARGET label
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "TARGET");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    y += 20;

    // --- Digit boxes ---
    int digit_w = 60;
    int digit_gap = 10;
    int digits_total = 3 * digit_w + 2 * digit_gap;
    int digit_x_start = (320 - digits_total) / 2;

    for (int i = 0; i < 3; i++) {
        int x = digit_x_start + i * (digit_w + digit_gap);

        // Box
        s_digit_boxes[i] = lv_obj_create(s_screen);
        lv_obj_set_size(s_digit_boxes[i], digit_w, 70);
        lv_obj_set_pos(s_digit_boxes[i], x, y);
        lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_digit_boxes[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_digit_boxes[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_digit_boxes[i], 2, 0);
        lv_obj_set_style_border_opa(s_digit_boxes[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(s_digit_boxes[i], 6, 0);
        lv_obj_clear_flag(s_digit_boxes[i], LV_OBJ_FLAG_SCROLLABLE);

        // Digit label
        s_digits[i] = lv_label_create(s_digit_boxes[i]);
        lv_label_set_text_fmt(s_digits[i], "%d", s_digit_vals[i]);
        lv_obj_set_style_text_font(s_digits[i], &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(s_digits[i], UI_COLOR_ACCENT, 0);
        lv_obj_center(s_digits[i]);
    }
    y += 80;

    // °F suffix
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "\xC2\xB0""F");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lbl, digit_x_start + digits_total + 5, y - 50);

    // Highlight first digit
    update_digit_highlight();

    // --- Main / Save buttons ---
    s_btn_main = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_main, 100, 36);
    lv_obj_align(s_btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_main, 2, 0);
    lv_obj_set_style_radius(s_btn_main, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_main, 0, 0);
    lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x331800), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_main, go_main, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_main = lv_label_create(s_btn_main);
    lv_label_set_text(lbl_main, "Main");
    lv_obj_set_style_text_font(lbl_main, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_main, UI_COLOR_ACCENT, 0);
    lv_obj_center(lbl_main);

    s_btn_save = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_save, 100, 36);
    lv_obj_align(s_btn_save, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(s_btn_save, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_btn_save, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_save, 2, 0);
    lv_obj_set_style_radius(s_btn_save, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_save, 0, 0);
    lv_obj_set_style_bg_color(s_btn_save, lv_color_hex(0x331800), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_btn_save, do_save, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(s_btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_save, UI_COLOR_ACCENT, 0);
    lv_obj_center(lbl_save);

    // Don't add buttons to group yet — digit timer handles input during digit editing
    // Buttons get added to group after all 3 digits are confirmed
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    // Take direct control of encoder (bypass LVGL indev)
    ui_encoder_set_direct(true);
    s_digit_timer = lv_timer_create(digit_edit_timer_cb, 50, NULL);

    return s_screen;
}
