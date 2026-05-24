// Set Probes — wizard with two-column type selection
//
// Probe tabs show current settings as you scroll.
// Two columns: meat type (left) and alarm type (right).
// Saved selections stay highlighted.

#include "ui_set_probes.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_probes";

static const char *meat_types[] = {
    "Brisket", "Beef", "Chicken", "Fish", "Lamb",
    "Pork Butt", "Turkey", "Veg", "Wild Game",
};
#define NUM_MEAT_TYPES 9

static const char *alarm_types[] = {
    "Baste", "Custom", "Secret", "Turn", "Wrap",
    "Beer Me", "W. Shot", "Margarita",
};
#define NUM_ALARM_TYPES 8

typedef enum {
    STEP_PROBE_SELECT,
    STEP_SET_TARGET,
    STEP_MEAT_TYPE,
    STEP_SET_ALARM,
    STEP_ALARM_TYPE,
} wizard_step_t;

static lv_obj_t *s_screen;
static lv_timer_t *s_timer;

static lv_obj_t *s_probe_btns[NUM_MEAT_PROBES];
static lv_obj_t *s_btn_main;
static lv_obj_t *s_btn_save;

// Info display for current probe
static lv_obj_t *s_lbl_probe_temp;
static lv_obj_t *s_lbl_info_target;
static lv_obj_t *s_lbl_info_meat;
static lv_obj_t *s_lbl_info_alarm;

// Step display
static lv_obj_t *s_lbl_step_title;
static lv_obj_t *s_lbl_step_value;

// Two-column type labels
static lv_obj_t *s_meat_lbls[NUM_MEAT_TYPES];
static lv_obj_t *s_alarm_lbls[NUM_ALARM_TYPES];
static lv_obj_t *s_lbl_meat_title;
static lv_obj_t *s_lbl_alarm_title;

static wizard_step_t s_step;
static int s_probe_idx;
static int s_cursor;
static int s_digit_vals[3];
static int s_digit_pos;

static float s_work_target;
static char s_work_food[16];
static float s_work_alarm;
static char s_work_alarm_type[16];

static int s_saved_meat_idx;
static int s_saved_alarm_idx;

#define PROBE_SEL_COUNT 6
#define PROBE_SEL_MAIN 4
#define PROBE_SEL_SAVE 5

static const int target_digit_min[] = {1, 0, 0};
static const int target_digit_max[] = {4, 9, 9};

static void update_display(void);

static void go_main_direct(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    ui_encoder_set_direct(false);
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    lv_scr_load(menu);
}

static int get_digit_value(void)
{
    return s_digit_vals[0] * 100 + s_digit_vals[1] * 10 + s_digit_vals[2];
}

static void save_probe(void)
{
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    gs->probes[s_probe_idx].enabled = true;
    gs->probes[s_probe_idx].target_temp = s_work_target;
    strncpy(gs->probes[s_probe_idx].food_type, s_work_food, sizeof(gs->probes[s_probe_idx].food_type));
    gs->probes[s_probe_idx].alarm_temp = s_work_alarm;
    strncpy(gs->probes[s_probe_idx].alarm_type, s_work_alarm_type, sizeof(gs->probes[s_probe_idx].alarm_type));
    grill_state_unlock();
    ESP_LOGI(TAG, "Probe %d: %s t=%.0f a=%.0f %s", s_probe_idx + 1,
             s_work_food, s_work_target, s_work_alarm, s_work_alarm_type);
}

static void show_probe_info(void)
{
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    probe_state_t *p = &gs->probes[s_probe_idx];
    lv_label_set_text_fmt(s_lbl_probe_temp, "%.0f\xC2\xB0""F", p->current_temp);
    if (p->enabled && p->target_temp > 0) {
        lv_label_set_text_fmt(s_lbl_info_target, "Target: %.0f\xC2\xB0""F", p->target_temp);
        lv_label_set_text_fmt(s_lbl_info_meat, "Meat: %s", p->food_type);
        if (p->alarm_temp > 0) {
            char buf[48];
            snprintf(buf, sizeof(buf), "Alarm: %.0f\xC2\xB0""F - %s", p->alarm_temp, p->alarm_type);
            lv_label_set_text(s_lbl_info_alarm, buf);
        } else {
            lv_label_set_text(s_lbl_info_alarm, "Alarm: OFF");
        }
    } else {
        lv_label_set_text(s_lbl_info_target, "Not configured");
        lv_label_set_text(s_lbl_info_meat, "");
        lv_label_set_text(s_lbl_info_alarm, "");
    }
    // Find saved indices for column highlights
    s_saved_meat_idx = -1;
    s_saved_alarm_idx = -1;
    for (int i = 0; i < NUM_MEAT_TYPES; i++) {
        if (strcmp(meat_types[i], p->food_type) == 0) s_saved_meat_idx = i;
    }
    for (int i = 0; i < NUM_ALARM_TYPES; i++) {
        if (strcmp(alarm_types[i], p->alarm_type) == 0) s_saved_alarm_idx = i;
    }
    grill_state_unlock();
}

static void update_columns(void)
{
    // Meat type column
    for (int i = 0; i < NUM_MEAT_TYPES; i++) {
        if (s_step == STEP_MEAT_TYPE && i == s_cursor) {
            // Active cursor
            lv_obj_set_style_text_color(s_meat_lbls[i], lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_color(lv_obj_get_parent(s_meat_lbls[i]), UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_opa(lv_obj_get_parent(s_meat_lbls[i]), LV_OPA_COVER, 0);
        } else if (i == s_saved_meat_idx) {
            // Saved selection
            lv_obj_set_style_text_color(s_meat_lbls[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_color(lv_obj_get_parent(s_meat_lbls[i]), lv_color_hex(0x331800), 0);
            lv_obj_set_style_bg_opa(lv_obj_get_parent(s_meat_lbls[i]), LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_text_color(s_meat_lbls[i], UI_COLOR_TEXT_DIM, 0);
            lv_obj_set_style_bg_color(lv_obj_get_parent(s_meat_lbls[i]), lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(lv_obj_get_parent(s_meat_lbls[i]), LV_OPA_COVER, 0);
        }
    }

    // Alarm type column
    for (int i = 0; i < NUM_ALARM_TYPES; i++) {
        if (s_step == STEP_ALARM_TYPE && i == s_cursor) {
            lv_obj_set_style_text_color(s_alarm_lbls[i], lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_color(lv_obj_get_parent(s_alarm_lbls[i]), UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_opa(lv_obj_get_parent(s_alarm_lbls[i]), LV_OPA_COVER, 0);
        } else if (i == s_saved_alarm_idx) {
            lv_obj_set_style_text_color(s_alarm_lbls[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_color(lv_obj_get_parent(s_alarm_lbls[i]), lv_color_hex(0x331800), 0);
            lv_obj_set_style_bg_opa(lv_obj_get_parent(s_alarm_lbls[i]), LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_text_color(s_alarm_lbls[i], UI_COLOR_TEXT_DIM, 0);
            lv_obj_set_style_bg_color(lv_obj_get_parent(s_alarm_lbls[i]), lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(lv_obj_get_parent(s_alarm_lbls[i]), LV_OPA_COVER, 0);
        }
    }

    // Column titles
    lv_obj_set_style_text_color(s_lbl_meat_title,
        s_step == STEP_MEAT_TYPE ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(s_lbl_alarm_title,
        s_step == STEP_ALARM_TYPE ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM, 0);
}

static void wizard_timer_cb(lv_timer_t *timer)
{
    if (!s_screen) return;
    int diff = encoder_get_diff();
    encoder_btn_event_t btn = encoder_get_button_event();

    switch (s_step) {
    case STEP_PROBE_SELECT:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= PROBE_SEL_COUNT) s_cursor = PROBE_SEL_COUNT - 1;
            if (s_cursor < NUM_MEAT_PROBES) { s_probe_idx = s_cursor; show_probe_info(); }
            update_display();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_cursor == PROBE_SEL_MAIN || s_cursor == PROBE_SEL_SAVE) {
                go_main_direct(); return;
            }
            s_probe_idx = s_cursor;
            grill_state_lock();
            probe_state_t *p = &grill_state_get()->probes[s_probe_idx];
            s_work_target = p->target_temp > 0 ? p->target_temp : 203;
            strncpy(s_work_food, p->food_type, sizeof(s_work_food));
            s_work_alarm = p->alarm_temp;
            strncpy(s_work_alarm_type, p->alarm_type, sizeof(s_work_alarm_type));
            grill_state_unlock();
            s_digit_vals[0] = ((int)s_work_target / 100) % 10;
            s_digit_vals[1] = ((int)s_work_target / 10) % 10;
            s_digit_vals[2] = (int)s_work_target % 10;
            if (s_digit_vals[0] < 1) s_digit_vals[0] = 1;
            s_digit_pos = 0;
            encoder_get_button_event(); encoder_get_diff();
            s_step = STEP_SET_TARGET;
            update_display();
        }
        break;

    case STEP_SET_TARGET:
        if (diff != 0) {
            s_digit_vals[s_digit_pos] += diff;
            if (s_digit_vals[s_digit_pos] > target_digit_max[s_digit_pos])
                s_digit_vals[s_digit_pos] = target_digit_min[s_digit_pos];
            if (s_digit_vals[s_digit_pos] < target_digit_min[s_digit_pos])
                s_digit_vals[s_digit_pos] = target_digit_max[s_digit_pos];
            update_display();
        }
        if (btn == ENCODER_BTN_SHORT) {
            s_digit_pos++;
            if (s_digit_pos >= 3) {
                s_work_target = (float)get_digit_value();
                s_cursor = 0;
                for (int i = 0; i < NUM_MEAT_TYPES; i++)
                    if (strcmp(meat_types[i], s_work_food) == 0) { s_cursor = i; break; }
                s_step = STEP_MEAT_TYPE;
            }
            update_display();
        }
        break;

    case STEP_MEAT_TYPE:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= NUM_MEAT_TYPES) s_cursor = NUM_MEAT_TYPES - 1;
            update_display();
        }
        if (btn == ENCODER_BTN_SHORT) {
            strncpy(s_work_food, meat_types[s_cursor], sizeof(s_work_food));
            s_saved_meat_idx = s_cursor;
            int alm = (int)s_work_alarm;
            s_digit_vals[0] = alm > 0 ? ((alm / 100) % 10) : 0;
            s_digit_vals[1] = (alm / 10) % 10;
            s_digit_vals[2] = alm % 10;
            if (s_digit_vals[0] < 0) s_digit_vals[0] = 0;
            s_digit_pos = 0;
            s_step = STEP_SET_ALARM;
            update_display();
        }
        break;

    case STEP_SET_ALARM:
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
            update_display();
        }
        if (btn == ENCODER_BTN_SHORT) {
            if (s_digit_pos == 0 && s_digit_vals[0] == 0) {
                s_work_alarm = 0; strncpy(s_work_alarm_type, "", sizeof(s_work_alarm_type));
                save_probe(); show_probe_info();
                s_cursor = s_probe_idx; s_step = STEP_PROBE_SELECT;
                update_display(); break;
            }
            s_digit_pos++;
            if (s_digit_pos >= 3) {
                s_work_alarm = (float)get_digit_value();
                s_cursor = 0;
                for (int i = 0; i < NUM_ALARM_TYPES; i++)
                    if (strcmp(alarm_types[i], s_work_alarm_type) == 0) { s_cursor = i; break; }
                s_step = STEP_ALARM_TYPE;
            }
            update_display();
        }
        break;

    case STEP_ALARM_TYPE:
        if (diff != 0) {
            s_cursor += diff;
            if (s_cursor < 0) s_cursor = 0;
            if (s_cursor >= NUM_ALARM_TYPES) s_cursor = NUM_ALARM_TYPES - 1;
            update_display();
        }
        if (btn == ENCODER_BTN_SHORT) {
            strncpy(s_work_alarm_type, alarm_types[s_cursor], sizeof(s_work_alarm_type));
            s_saved_alarm_idx = s_cursor;
            save_probe(); show_probe_info();
            s_cursor = s_probe_idx; s_step = STEP_PROBE_SELECT;
            update_display();
        }
        break;
    }
}

static void update_display(void)
{
    // Probe tabs
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (i == s_probe_idx) {
            lv_obj_set_style_bg_color(s_probe_btns[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_opa(s_probe_btns[i], LV_OPA_COVER, 0);
        } else if (s_step == STEP_PROBE_SELECT && i == s_cursor) {
            lv_obj_set_style_bg_color(s_probe_btns[i], lv_color_hex(0x331800), 0);
            lv_obj_set_style_bg_opa(s_probe_btns[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(s_probe_btns[i], lv_color_hex(0x000000), 0);
        }
    }

    // Main/Save highlights
    lv_obj_set_style_bg_color(s_btn_main,
        (s_step == STEP_PROBE_SELECT && s_cursor == PROBE_SEL_MAIN) ? lv_color_hex(0x331800) : lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_color(s_btn_save,
        (s_step == STEP_PROBE_SELECT && s_cursor == PROBE_SEL_SAVE) ? lv_color_hex(0x331800) : lv_color_hex(0x000000), 0);

    // Step title/value
    char buf[64];
    switch (s_step) {
    case STEP_PROBE_SELECT:
        lv_label_set_text(s_lbl_step_title, "Select probe, then press");
        lv_label_set_text(s_lbl_step_value, "");
        break;
    case STEP_SET_TARGET:
        lv_label_set_text(s_lbl_step_title, "SET TARGET");
        snprintf(buf, sizeof(buf), "%s%d%s %s%d%s %s%d%s \xC2\xB0""F",
            s_digit_pos==0?"[":"", s_digit_vals[0], s_digit_pos==0?"]":"",
            s_digit_pos==1?"[":"", s_digit_vals[1], s_digit_pos==1?"]":"",
            s_digit_pos==2?"[":"", s_digit_vals[2], s_digit_pos==2?"]":"");
        lv_label_set_text(s_lbl_step_value, buf);
        lv_obj_set_style_text_color(s_lbl_step_value, UI_COLOR_ACCENT, 0);
        break;
    case STEP_MEAT_TYPE:
        lv_label_set_text(s_lbl_step_title, "Select MEAT TYPE");
        lv_label_set_text(s_lbl_step_value, "");
        break;
    case STEP_SET_ALARM:
        lv_label_set_text(s_lbl_step_title, "SET ALARM");
        if (s_digit_pos == 0 && s_digit_vals[0] == 0)
            snprintf(buf, sizeof(buf), "[OFF]");
        else
            snprintf(buf, sizeof(buf), "%s%d%s %s%d%s %s%d%s \xC2\xB0""F",
                s_digit_pos==0?"[":"", s_digit_vals[0], s_digit_pos==0?"]":"",
                s_digit_pos==1?"[":"", s_digit_vals[1], s_digit_pos==1?"]":"",
                s_digit_pos==2?"[":"", s_digit_vals[2], s_digit_pos==2?"]":"");
        lv_label_set_text(s_lbl_step_value, buf);
        lv_obj_set_style_text_color(s_lbl_step_value, UI_COLOR_ACCENT, 0);
        break;
    case STEP_ALARM_TYPE:
        lv_label_set_text(s_lbl_step_title, "Select ALARM TYPE");
        lv_label_set_text(s_lbl_step_value, "");
        break;
    }

    update_columns();
}

lv_obj_t *ui_set_probes_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_step = STEP_PROBE_SELECT;
    s_probe_idx = 0; s_cursor = 0;
    s_saved_meat_idx = -1; s_saved_alarm_idx = -1;

    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    int y = 4;

    // Cook Mode
    grill_state_lock();
    grill_mode_t mode = grill_state_get()->mode;
    grill_state_unlock();
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text_fmt(lbl, "Cook Mode: %s", grill_mode_name(mode));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 22;

    // Probe tabs
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "PROBE:");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl, 8, y + 6);

    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        s_probe_btns[i] = lv_obj_create(s_screen);
        lv_obj_remove_style_all(s_probe_btns[i]);
        lv_obj_set_size(s_probe_btns[i], 42, 30);
        lv_obj_set_pos(s_probe_btns[i], 78 + i * 50, y);
        lv_obj_set_style_border_color(s_probe_btns[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_probe_btns[i], 2, 0);
        lv_obj_set_style_radius(s_probe_btns[i], 4, 0);
        lv_obj_set_style_bg_color(s_probe_btns[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_probe_btns[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_probe_btns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = lv_label_create(s_probe_btns[i]);
        lv_label_set_text_fmt(l, "%d", i + 1);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(l);
    }
    y += 34;

    // Probe temp + info
    s_lbl_probe_temp = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_probe_temp, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_probe_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_lbl_probe_temp, 8, y);

    s_lbl_info_target = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_info_target, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_info_target, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_info_target, 160, y);

    s_lbl_info_meat = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_info_meat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_info_meat, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_info_meat, 160, y + 18);

    s_lbl_info_alarm = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_info_alarm, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_info_alarm, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_info_alarm, 160, y + 36);
    y += 56;

    // Step title + value
    s_lbl_step_title = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_step_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_step_title, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(s_lbl_step_title, LV_ALIGN_TOP_MID, 0, y);
    y += 18;

    s_lbl_step_value = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_step_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lbl_step_value, UI_COLOR_ACCENT, 0);
    lv_obj_align(s_lbl_step_value, LV_ALIGN_TOP_MID, 0, y);
    y += 30;

    // Two columns: meat type (left) and alarm type (right)
    int col_y = y;
    int col_left = 8;
    int col_right = 165;
    int row_h = 22;

    s_lbl_meat_title = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_meat_title, "MEAT TYPE");
    lv_obj_set_style_text_font(s_lbl_meat_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_meat_title, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_meat_title, col_left, col_y);

    s_lbl_alarm_title = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_alarm_title, "ALARM TYPE");
    lv_obj_set_style_text_font(s_lbl_alarm_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_alarm_title, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_alarm_title, col_right, col_y);
    col_y += 18;

    for (int i = 0; i < NUM_MEAT_TYPES; i++) {
        lv_obj_t *row = lv_obj_create(s_screen);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 148, row_h - 2);
        lv_obj_set_pos(row, col_left, col_y + i * row_h);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), 0);
        lv_obj_set_style_radius(row, 2, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_meat_lbls[i] = lv_label_create(row);
        lv_label_set_text(s_meat_lbls[i], meat_types[i]);
        lv_obj_set_style_text_font(s_meat_lbls[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_meat_lbls[i], UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(s_meat_lbls[i], 4, 1);
    }

    for (int i = 0; i < NUM_ALARM_TYPES; i++) {
        lv_obj_t *row = lv_obj_create(s_screen);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 148, row_h - 2);
        lv_obj_set_pos(row, col_right, col_y + i * row_h);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), 0);
        lv_obj_set_style_radius(row, 2, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_alarm_lbls[i] = lv_label_create(row);
        lv_label_set_text(s_alarm_lbls[i], alarm_types[i]);
        lv_obj_set_style_text_font(s_alarm_lbls[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_alarm_lbls[i], UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(s_alarm_lbls[i], 4, 1);
    }

    // Main / Save buttons
    s_btn_main = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_main, 90, 32);
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

    s_btn_save = lv_button_create(s_screen);
    lv_obj_set_size(s_btn_save, 90, 32);
    lv_obj_align(s_btn_save, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
    lv_obj_set_style_bg_color(s_btn_save, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_btn_save, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_btn_save, 2, 0);
    lv_obj_set_style_radius(s_btn_save, 4, 0);
    lv_obj_set_style_shadow_width(s_btn_save, 0, 0);
    lv_obj_t *l2 = lv_label_create(s_btn_save);
    lv_label_set_text(l2, "Save");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l2, UI_COLOR_ACCENT, 0);
    lv_obj_center(l2);

    show_probe_info();
    update_display();

    ui_encoder_set_direct(true);
    encoder_get_button_event(); encoder_get_diff();
    s_timer = lv_timer_create(wizard_timer_cb, 80, NULL);

    return s_screen;
}
