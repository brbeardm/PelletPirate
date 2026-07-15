// Set Probes — 3-level scroll navigation
//
// Level 1: [1][2][3][4] Main Save
// Level 2 (probe selected): Set Target | Set Alarm | Save
// Level 3a (Set Target — full): target digits → meat → alarm digits → alarm type → Save
// Level 3b (Set Alarm — alarm only): alarm digits → alarm type → Save
//
// Target=0 → skip meat/alarm, go to Save (probe disabled)
// Alarm=OFF → skip alarm type, go to Save

#include "ui_set_probes.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "cooklog.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_probes";

static const char *meat_types[] = {
    "Brisket", "Beef", "Chicken", "Fish", "Lamb",
    "Pork Butt", "Turkey", "Veg (for girls)", "Wild Game",
};
#define NUM_MEAT_TYPES 9

static const char *alarm_types[] = {
    "Baste", "Custom", "Secret", "Turn", "Wrap",
    "Beer Me", "Whiskey Shot", "Margarita",
};
#define NUM_ALARM_TYPES 8

typedef enum {
    LVL1_SCROLL,        // [1][2][3][4] Main Save
    LVL2_SCROLL,        // Set Target | Set Alarm | Save
    LVL3_TARGET_DIGITS, // editing target temp digits
    LVL3_PICK_MEAT,     // selecting meat type
    LVL3_ALARM_DIGITS,  // editing alarm digits
    LVL3_PICK_ALARM,    // selecting alarm type
    LVL3_SAVE,          // Save highlighted, waiting for click
} nav_state_t;

// Level 1 items: probes + Main (no Save — nothing to save yet)
enum { L1_P1=0, L1_P2, L1_P3, L1_P4, L1_MAIN, L1_COUNT };
// Level 2 items
enum { L2_TARGET=0, L2_ALARM, L2_SAVE, L2_COUNT };

static lv_obj_t *s_screen;
static lv_timer_t *s_timer;

static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_probe_temp;
static lv_obj_t *s_probe_btns[4];
static lv_obj_t *s_lbl_target_title, *s_lbl_alarm_title;
static lv_obj_t *s_lbl_target_val, *s_lbl_alarm_val;

// Digit edit overlay — 3 boxes that appear during digit editing
static lv_obj_t *s_digit_boxes[3];
static lv_obj_t *s_digit_lbls[3];
static bool s_digit_overlay_target;  // true = editing target, false = editing alarm
static lv_obj_t *s_meat_rows[NUM_MEAT_TYPES], *s_meat_lbls[NUM_MEAT_TYPES];
static lv_obj_t *s_alarm_rows[NUM_ALARM_TYPES], *s_alarm_lbls[NUM_ALARM_TYPES];
static lv_obj_t *s_btn_main, *s_btn_save;

static nav_state_t s_state;
static int s_cursor;         // position within current level
static int s_probe_idx;      // selected probe 0-3
static int s_digit_vals[3];
static int s_digit_pos;
static bool s_full_workflow;  // true = 3a (target+alarm), false = 3b (alarm only)

static float s_work_target;
static char s_work_food[16];
static float s_work_alarm;
static char s_work_alarm_type[16];
static int s_saved_meat_idx, s_saved_alarm_idx;

static void update_all(void);

static void go_main_direct(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    ui_encoder_set_direct(false);
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    ui_load_screen(menu);
}

static int get_digit_value(void) {
    return s_digit_vals[0] * 100 + s_digit_vals[1] * 10 + s_digit_vals[2];
}

static void load_probe_data(void) {
    grill_state_lock();
    probe_state_t *p = &grill_state_get()->probes[s_probe_idx];

    char buf[16];
    snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)p->current_temp);
    lv_label_set_text(s_lbl_probe_temp, buf);

    snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)p->target_temp);
    lv_label_set_text(s_lbl_target_val, buf);

    if (p->alarm_temp > 0) {
        snprintf(buf, sizeof(buf), "%d\xC2\xB0""F", (int)p->alarm_temp);
        lv_label_set_text(s_lbl_alarm_val, buf);
    } else {
        lv_label_set_text(s_lbl_alarm_val, "OFF");
    }

    s_saved_meat_idx = -1;
    s_saved_alarm_idx = -1;
    for (int i = 0; i < NUM_MEAT_TYPES; i++)
        if (strcmp(meat_types[i], p->food_type) == 0) s_saved_meat_idx = i;
    for (int i = 0; i < NUM_ALARM_TYPES; i++)
        if (strcmp(alarm_types[i], p->alarm_type) == 0) s_saved_alarm_idx = i;

    s_work_target = p->target_temp;
    strncpy(s_work_food, p->food_type, sizeof(s_work_food));
    s_work_alarm = p->alarm_temp;
    strncpy(s_work_alarm_type, p->alarm_type, sizeof(s_work_alarm_type));
    grill_state_unlock();
}

static void save_probe(void) {
    grill_state_lock();
    probe_state_t *p = &grill_state_get()->probes[s_probe_idx];
    p->target_temp = s_work_target;
    strncpy(p->food_type, s_work_food, sizeof(p->food_type));
    p->alarm_temp = s_work_alarm;
    strncpy(p->alarm_type, s_work_alarm_type, sizeof(p->alarm_type));
    p->enabled = (s_work_target > 0);
    grill_state_save_to_nvs();
    grill_state_unlock();
    ESP_LOGI(TAG, "Saved probe %d: %s t=%.0f a=%.0f %s",
             s_probe_idx+1, s_work_food, s_work_target, s_work_alarm, s_work_alarm_type);
    cooklog_event("lcd", "PROBE %d cfg tg=%d al=%d %s/%s", s_probe_idx + 1,
                  (int)s_work_target, (int)s_work_alarm, s_work_food, s_work_alarm_type);
    load_probe_data();
}

static void go_to_save_state(void) {
    s_state = LVL3_SAVE;
    s_cursor = 0;
    update_all();
}

static void init_target_digits(void) {
    int t = (int)s_work_target;
    s_digit_vals[0] = (t / 100) % 10;
    s_digit_vals[1] = (t / 10) % 10;
    s_digit_vals[2] = t % 10;
    s_digit_pos = 0;
}

static void init_alarm_digits(void) {
    int a = (int)s_work_alarm;
    s_digit_vals[0] = a > 0 ? (a / 100) % 10 : 0;
    s_digit_vals[1] = (a / 10) % 10;
    s_digit_vals[2] = a % 10;
    s_digit_pos = 0;
}

static void update_columns(void) {
    for (int i = 0; i < NUM_MEAT_TYPES; i++) {
        bool active = (s_state == LVL3_PICK_MEAT && i == s_cursor);
        bool saved = (i == s_saved_meat_idx);
        lv_obj_set_style_bg_color(s_meat_rows[i],
            (active || saved) ? UI_COLOR_ACCENT : lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(s_meat_lbls[i],
            active ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
    }
    for (int i = 0; i < NUM_ALARM_TYPES; i++) {
        bool active = (s_state == LVL3_PICK_ALARM && i == s_cursor);
        bool saved = (i == s_saved_alarm_idx);
        lv_obj_set_style_bg_color(s_alarm_rows[i],
            (active || saved) ? UI_COLOR_ACCENT : lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(s_alarm_lbls[i],
            active ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
    }
}

static void update_all(void) {
    // Probe tabs
    for (int i = 0; i < 4; i++) {
        bool selected = (i == s_probe_idx);
        bool focused = (s_state == LVL1_SCROLL && s_cursor == i);
        lv_obj_set_style_bg_color(s_probe_btns[i],
            selected ? UI_COLOR_ACCENT :
            focused ? lv_color_hex(0x663300) : lv_color_hex(0x000000), 0);
    }

    // Set Target / Set Alarm titles
    bool tgt_active = (s_state == LVL2_SCROLL && s_cursor == L2_TARGET) ||
                      s_state == LVL3_TARGET_DIGITS || s_state == LVL3_PICK_MEAT;
    bool alm_active = (s_state == LVL2_SCROLL && s_cursor == L2_ALARM) ||
                      s_state == LVL3_ALARM_DIGITS || s_state == LVL3_PICK_ALARM;

    lv_obj_set_style_text_color(s_lbl_target_title,
        tgt_active ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(s_lbl_alarm_title,
        alm_active ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM, 0);

    // Digit editing overlay
    bool editing_digits = (s_state == LVL3_TARGET_DIGITS || s_state == LVL3_ALARM_DIGITS);
    for (int i = 0; i < 3; i++) {
        if (editing_digits) {
            lv_obj_clear_flag(s_digit_boxes[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(s_digit_lbls[i], "%d", s_digit_vals[i]);
            if (i == s_digit_pos) {
                // Active digit: orange bg, black text (inverted)
                lv_obj_set_style_bg_color(s_digit_boxes[i], UI_COLOR_ACCENT, 0);
                lv_obj_set_style_text_color(s_digit_lbls[i], lv_color_hex(0x000000), 0);
            } else {
                // Inactive digit: black bg, white text
                lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x000000), 0);
                lv_obj_set_style_text_color(s_digit_lbls[i], lv_color_hex(0xFFFFFF), 0);
            }
            // Handle alarm OFF display
            if (s_state == LVL3_ALARM_DIGITS && s_digit_pos == 0 && s_digit_vals[0] == 0 && i == 0) {
                lv_label_set_text(s_digit_lbls[0], "OFF");
            }
        } else {
            lv_obj_add_flag(s_digit_boxes[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Position digit boxes over target or alarm value area
    if (s_state == LVL3_TARGET_DIGITS) {
        for (int i = 0; i < 3; i++)
            lv_obj_set_pos(s_digit_boxes[i], 16 + i * 40, lv_obj_get_y(s_lbl_target_val));
        lv_obj_add_flag(s_lbl_target_val, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lbl_alarm_val, LV_OBJ_FLAG_HIDDEN);
    } else if (s_state == LVL3_ALARM_DIGITS) {
        for (int i = 0; i < 3; i++)
            lv_obj_set_pos(s_digit_boxes[i], 200 + i * 40, lv_obj_get_y(s_lbl_alarm_val));
        lv_obj_add_flag(s_lbl_alarm_val, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lbl_target_val, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_lbl_target_val, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lbl_alarm_val, LV_OBJ_FLAG_HIDDEN);
    }

    // Target value (when not editing)
    if (s_state != LVL3_TARGET_DIGITS) {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0""F", (int)s_work_target);
        lv_label_set_text(s_lbl_target_val, tbuf);
        lv_obj_set_style_text_color(s_lbl_target_val, UI_COLOR_ACCENT, 0);
    }

    // Alarm value (when not editing)
    if (s_state != LVL3_ALARM_DIGITS) {
        char abuf[16];
        if (s_work_alarm > 0)
            snprintf(abuf, sizeof(abuf), "%d\xC2\xB0""F", (int)s_work_alarm);
        else
            snprintf(abuf, sizeof(abuf), "OFF");
        lv_label_set_text(s_lbl_alarm_val, abuf);
        lv_obj_set_style_text_color(s_lbl_alarm_val, UI_COLOR_ACCENT, 0);
    }

    // Main button — solid orange bg + white text when focused
    bool main_focus = (s_state == LVL1_SCROLL && s_cursor == L1_MAIN);
    lv_obj_set_style_bg_color(s_btn_main,
        main_focus ? UI_COLOR_ACCENT : lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_main, LV_OPA_COVER, 0);

    // Save button — only active in Level 2 Save, Level 3 Save, or completing workflows
    bool save_active = (s_state == LVL2_SCROLL && s_cursor == L2_SAVE) ||
                       s_state == LVL3_SAVE;
    bool save_gray = (s_state == LVL1_SCROLL);  // nothing to save at Level 1

    lv_obj_set_style_bg_color(s_btn_save,
        save_active ? UI_COLOR_ACCENT : lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_btn_save, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_btn_save,
        save_gray ? UI_COLOR_TEXT_DIM : UI_COLOR_ACCENT, 0);
    lv_obj_t *save_lbl = lv_obj_get_child(s_btn_save, 0);
    if (save_lbl) lv_obj_set_style_text_color(save_lbl,
        save_gray ? UI_COLOR_TEXT_DIM : lv_color_hex(0xFFFFFF), 0);

    update_columns();
}

static void timer_cb(lv_timer_t *timer) {
    if (!s_screen) return;
    int diff = encoder_get_diff();
    encoder_btn_event_t btn = encoder_get_button_event();

    switch (s_state) {

    case LVL1_SCROLL:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= L1_COUNT) s_cursor = L1_COUNT - 1;
            if (s_cursor < 4) { s_probe_idx = s_cursor; load_probe_data(); }
            update_all();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_cursor < 4) {
                // Select probe → Level 2
                s_probe_idx = s_cursor;
                load_probe_data();
                s_state = LVL2_SCROLL;
                s_cursor = L2_TARGET;
                encoder_get_button_event(); encoder_get_diff();
                update_all();
            } else if (s_cursor == L1_MAIN) {
                go_main_direct(); return;
            }
        }
        break;

    case LVL2_SCROLL:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= L2_COUNT) s_cursor = L2_COUNT - 1;
            update_all();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_cursor == L2_TARGET) {
                // Full workflow: target → meat → alarm → alarm type → save
                s_full_workflow = true;
                init_target_digits();
                s_state = LVL3_TARGET_DIGITS;
                s_digit_pos = 0;
                encoder_get_button_event(); encoder_get_diff();
                update_all();
            } else if (s_cursor == L2_ALARM) {
                // Alarm only workflow
                s_full_workflow = false;
                init_alarm_digits();
                s_state = LVL3_ALARM_DIGITS;
                s_digit_pos = 0;
                encoder_get_button_event(); encoder_get_diff();
                update_all();
            } else if (s_cursor == L2_SAVE) {
                save_probe();
                s_state = LVL1_SCROLL;
                s_cursor = s_probe_idx;
                update_all();
            }
        }
        break;

    case LVL3_TARGET_DIGITS:
        if (diff != 0) {
            int mx = (s_digit_pos == 0) ? 4 : 9;
            s_digit_vals[s_digit_pos] += diff;
            if (s_digit_vals[s_digit_pos] > mx) s_digit_vals[s_digit_pos] = 0;
            if (s_digit_vals[s_digit_pos] < 0) s_digit_vals[s_digit_pos] = mx;
            update_all();
        }
        if (btn == ENCODER_BTN_SHORT) {
            s_digit_pos++;
            if (s_digit_pos >= 3) {
                s_work_target = (float)get_digit_value();
                if (s_work_target > 0) {
                    char tbuf[16];
                    snprintf(tbuf, sizeof(tbuf), "%.0f\xC2\xB0""F", s_work_target);  // LVGL fmt has no %f
                    lv_label_set_text(s_lbl_target_val, tbuf);
                } else {
                    lv_label_set_text(s_lbl_target_val, "0\xC2\xB0""F");
                }
                if (s_work_target == 0) {
                    // Target=0 → probe disabled, skip meat/alarm, clear everything
                    strncpy(s_work_food, "", sizeof(s_work_food));
                    s_work_alarm = 0;
                    strncpy(s_work_alarm_type, "", sizeof(s_work_alarm_type));
                    s_saved_meat_idx = -1;
                    s_saved_alarm_idx = -1;
                    lv_label_set_text(s_lbl_alarm_val, "OFF");
                    go_to_save_state();
                } else {
                    // → Pick meat type
                    s_cursor = s_saved_meat_idx >= 0 ? s_saved_meat_idx : 0;
                    s_state = LVL3_PICK_MEAT;
                    update_all();
                }
            } else {
                update_all();
            }
        }
        break;

    case LVL3_PICK_MEAT:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= NUM_MEAT_TYPES) s_cursor = NUM_MEAT_TYPES - 1;
            update_all();
        }
        if (btn == ENCODER_BTN_SHORT) {
            strncpy(s_work_food, meat_types[s_cursor], sizeof(s_work_food));
            s_saved_meat_idx = s_cursor;
            // → Alarm digits (part of full workflow)
            init_alarm_digits();
            s_state = LVL3_ALARM_DIGITS;
            s_digit_pos = 0;
            update_all();
        }
        break;

    case LVL3_ALARM_DIGITS:
        if (diff != 0) {
            if (s_digit_pos == 0) {
                s_digit_vals[0] += diff;
                if (s_digit_vals[0] > 4) s_digit_vals[0] = 0;
                if (s_digit_vals[0] < 0) s_digit_vals[0] = 4;
            } else {
                s_digit_vals[s_digit_pos] += diff;
                if (s_digit_vals[s_digit_pos] > 9) s_digit_vals[s_digit_pos] = 0;
                if (s_digit_vals[s_digit_pos] < 0) s_digit_vals[s_digit_pos] = 9;
            }
            update_all();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_digit_pos == 0 && s_digit_vals[0] == 0) {
                // OFF → skip alarm type
                s_work_alarm = 0;
                strncpy(s_work_alarm_type, "", sizeof(s_work_alarm_type));
                s_saved_alarm_idx = -1;
                lv_label_set_text(s_lbl_alarm_val, "OFF");
                go_to_save_state();
            } else {
                s_digit_pos++;
                if (s_digit_pos >= 3) {
                    s_work_alarm = (float)get_digit_value();
                    char abuf[16];
                    snprintf(abuf, sizeof(abuf), "%.0f\xC2\xB0""F", s_work_alarm);  // LVGL fmt has no %f
                    lv_label_set_text(s_lbl_alarm_val, abuf);
                    // → Pick alarm type
                    s_cursor = s_saved_alarm_idx >= 0 ? s_saved_alarm_idx : 0;
                    s_state = LVL3_PICK_ALARM;
                    update_all();
                } else {
                    update_all();
                }
            }
        }
        break;

    case LVL3_PICK_ALARM:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= NUM_ALARM_TYPES) s_cursor = NUM_ALARM_TYPES - 1;
            update_all();
        }
        if (btn == ENCODER_BTN_SHORT) {
            strncpy(s_work_alarm_type, alarm_types[s_cursor], sizeof(s_work_alarm_type));
            s_saved_alarm_idx = s_cursor;
            go_to_save_state();
        }
        break;

    case LVL3_SAVE:
        // Save is highlighted, just waiting for click
        if (btn == ENCODER_BTN_SHORT) {
            save_probe();
            s_state = LVL1_SCROLL;
            s_cursor = s_probe_idx;
            update_all();
        }
        break;
    }
}

lv_obj_t *ui_set_probes_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_state = LVL1_SCROLL;
    s_cursor = 0;
    s_probe_idx = 0;
    s_saved_meat_idx = -1;
    s_saved_alarm_idx = -1;

    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    int y = 2;

    // Cook Mode
    grill_state_lock();
    grill_mode_t mode = grill_state_get()->mode;
    grill_state_unlock();
    s_lbl_mode = lv_label_create(s_screen);
    lv_label_set_text_fmt(s_lbl_mode, "Cook Mode: %s", grill_mode_name(mode));
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_lbl_mode, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(s_lbl_mode, 8, y);
    y += 24;

    // Probe temp HUGE
    s_lbl_probe_temp = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_probe_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_probe_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_probe_temp, LV_ALIGN_TOP_MID, 0, y);
    y += 52;

    // Set Probe + tabs
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "Set\nProbe");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl, 4, y);

    for (int i = 0; i < 4; i++) {
        s_probe_btns[i] = lv_obj_create(s_screen);
        lv_obj_remove_style_all(s_probe_btns[i]);
        lv_obj_set_size(s_probe_btns[i], 50, 36);
        lv_obj_set_pos(s_probe_btns[i], 68 + i * 60, y);
        lv_obj_set_style_border_color(s_probe_btns[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_probe_btns[i], 2, 0);
        lv_obj_set_style_radius(s_probe_btns[i], 4, 0);
        lv_obj_set_style_bg_color(s_probe_btns[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_probe_btns[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_probe_btns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = lv_label_create(s_probe_btns[i]);
        lv_label_set_text_fmt(l, "%d", i + 1);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(l);
    }
    y += 42;

    // Set Target / Set Alarm
    s_lbl_target_title = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_target_title, "Set Target");
    lv_obj_set_style_text_font(s_lbl_target_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_target_title, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_target_title, 16, y);

    s_lbl_alarm_title = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_alarm_title, "Set Alarm");
    lv_obj_set_style_text_font(s_lbl_alarm_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_alarm_title, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_alarm_title, 200, y);
    y += 18;

    s_lbl_target_val = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_target_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_target_val, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(s_lbl_target_val, 16, y);

    s_lbl_alarm_val = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_alarm_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_alarm_val, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(s_lbl_alarm_val, 200, y);

    // Digit edit boxes (hidden initially, shown during digit editing)
    for (int i = 0; i < 3; i++) {
        s_digit_boxes[i] = lv_obj_create(s_screen);
        lv_obj_remove_style_all(s_digit_boxes[i]);
        lv_obj_set_size(s_digit_boxes[i], 36, 34);
        lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_digit_boxes[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_digit_boxes[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_digit_boxes[i], 2, 0);
        lv_obj_set_style_radius(s_digit_boxes[i], 4, 0);
        lv_obj_clear_flag(s_digit_boxes[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_digit_boxes[i], LV_OBJ_FLAG_HIDDEN);

        s_digit_lbls[i] = lv_label_create(s_digit_boxes[i]);
        lv_label_set_text(s_digit_lbls[i], "0");
        lv_obj_set_style_text_font(s_digit_lbls[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(s_digit_lbls[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(s_digit_lbls[i]);
    }

    y += 36;

    // Two columns
    int col_left = 4, col_right = 168, row_h = 22;

    for (int i = 0; i < NUM_MEAT_TYPES; i++) {
        s_meat_rows[i] = lv_obj_create(s_screen);
        lv_obj_remove_style_all(s_meat_rows[i]);
        lv_obj_set_size(s_meat_rows[i], 155, row_h);
        lv_obj_set_pos(s_meat_rows[i], col_left, y + i * row_h);
        lv_obj_set_style_bg_opa(s_meat_rows[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_meat_rows[i], lv_color_hex(0x000000), 0);
        lv_obj_clear_flag(s_meat_rows[i], LV_OBJ_FLAG_SCROLLABLE);
        s_meat_lbls[i] = lv_label_create(s_meat_rows[i]);
        lv_label_set_text(s_meat_lbls[i], meat_types[i]);
        lv_obj_set_style_text_font(s_meat_lbls[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_meat_lbls[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_meat_lbls[i], 4, 1);
    }

    for (int i = 0; i < NUM_ALARM_TYPES; i++) {
        s_alarm_rows[i] = lv_obj_create(s_screen);
        lv_obj_remove_style_all(s_alarm_rows[i]);
        lv_obj_set_size(s_alarm_rows[i], 148, row_h);
        lv_obj_set_pos(s_alarm_rows[i], col_right, y + i * row_h);
        lv_obj_set_style_bg_opa(s_alarm_rows[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_alarm_rows[i], lv_color_hex(0x000000), 0);
        lv_obj_clear_flag(s_alarm_rows[i], LV_OBJ_FLAG_SCROLLABLE);
        s_alarm_lbls[i] = lv_label_create(s_alarm_rows[i]);
        lv_label_set_text(s_alarm_lbls[i], alarm_types[i]);
        lv_obj_set_style_text_font(s_alarm_lbls[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_alarm_lbls[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_alarm_lbls[i], 4, 1);
    }

    // Main / Save
    s_btn_main = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_main, 90, 32);
    lv_obj_align(s_btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_obj_set_style_bg_color(s_btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_main, 2, 0);
    lv_obj_set_style_radius(s_btn_main, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_main, 0, 0);
    lv_obj_t *l1 = lv_label_create(s_btn_main);
    lv_label_set_text(l1, "Main");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l1);

    s_btn_save = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_save, 90, 32);
    lv_obj_align(s_btn_save, LV_ALIGN_BOTTOM_LEFT, 110, -4);
    lv_obj_set_style_bg_color(s_btn_save, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_btn_save, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_save, 2, 0);
    lv_obj_set_style_radius(s_btn_save, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_save, 0, 0);
    lv_obj_t *l2 = lv_label_create(s_btn_save);
    lv_label_set_text(l2, "Save");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l2);

    load_probe_data();
    update_all();

    ui_encoder_set_direct(true);
    encoder_get_button_event(); encoder_get_diff();
    s_timer = lv_timer_create(timer_cb, 80, NULL);

    return s_screen;
}
