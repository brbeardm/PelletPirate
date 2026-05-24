// PelletPirate Main Menu
//
// Top: Cook Mode + Current temp + Target temp (editable in-place)
// Menu items: SET TARGET TEMP, START NOW – IGNITE, COOK MODE, SET PROBES, SETTINGS, GRAPHS
// Bottom: Cook Dashboard button
//
// SET TARGET TEMP doesn't navigate — it edits the target digits in-place on this screen.

#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "ui_ignite.h"
#include "ui_cook_mode.h"
#include "ui_set_probes.h"
#include "ui_dashboard.h"
#include "grill_state.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_menu";

static lv_obj_t *s_screen;
static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_current;

// Target temp display — 3 separate digit labels for in-place editing
static lv_obj_t *s_lbl_target;          // shown in menu mode (e.g. "225°F")
static lv_obj_t *s_digit_container;     // shown in edit mode
static lv_obj_t *s_digit_lbls[3];
static lv_obj_t *s_digit_boxes[3];

// Menu buttons (stored for re-adding to group after edit)
static lv_obj_t *s_menu_btns[7];  // 6 menu items + dashboard
static lv_obj_t *s_lbl_ignite;    // ignite button label (dynamic text)
#define NUM_MENU_ITEMS 6

// Digit editing state
static bool s_editing = false;
static int s_digit_vals[3];
static int s_digit_pos;
static lv_timer_t *s_edit_timer = NULL;

static const int digit_min[] = {1, 0, 0};
static const int digit_max[] = {4, 9, 9};

typedef enum {
    MENU_SET_TARGET = 0,
    MENU_IGNITE,
    MENU_COOK_MODE,
    MENU_SET_PROBES,
    MENU_SETTINGS,
    MENU_GRAPHS,
    MENU_DASHBOARD,
} menu_item_t;

static const char *menu_labels[] = {
    "SET TARGET TEMP",
    "START NOW - IGNITE",
    "COOK MODE",
    "SET PROBES",
    "SETTINGS",
    "GRAPHS",
};

static void setup_menu_group(void);

// --- Digit editing ---

static void update_digit_display(void)
{
    for (int i = 0; i < 3; i++) {
        lv_label_set_text_fmt(s_digit_lbls[i], "%d", s_digit_vals[i]);
        if (i == s_digit_pos) {
            lv_obj_set_style_border_opa(s_digit_boxes[i], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x331800), 0);
        } else {
            lv_obj_set_style_border_opa(s_digit_boxes[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x000000), 0);
        }
    }
}

static void finish_editing(bool save)
{
    if (s_edit_timer) { lv_timer_delete(s_edit_timer); s_edit_timer = NULL; }
    ui_encoder_set_direct(false);
    s_editing = false;

    if (save) {
        int temp = s_digit_vals[0] * 100 + s_digit_vals[1] * 10 + s_digit_vals[2];
        ESP_LOGI(TAG, "Target set to %d", temp);
        grill_state_lock();
        grill_state_get()->grill_target = temp;
        grill_state_unlock();
    }

    // Hide digit editor, show normal target label
    lv_obj_add_flag(s_digit_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_lbl_target, LV_OBJ_FLAG_HIDDEN);

    // Restore menu group
    setup_menu_group();
}

static void edit_timer_cb(lv_timer_t *timer)
{
    if (!s_editing) return;

    int diff = encoder_get_diff();
    if (diff > 0) {
        s_digit_vals[s_digit_pos]++;
        if (s_digit_vals[s_digit_pos] > digit_max[s_digit_pos])
            s_digit_vals[s_digit_pos] = digit_min[s_digit_pos];
        update_digit_display();
    } else if (diff < 0) {
        s_digit_vals[s_digit_pos]--;
        if (s_digit_vals[s_digit_pos] < digit_min[s_digit_pos])
            s_digit_vals[s_digit_pos] = digit_max[s_digit_pos];
        update_digit_display();
    }

    encoder_btn_event_t btn = encoder_get_button_event();
    if (btn == ENCODER_BTN_SHORT) {
        s_digit_pos++;
        if (s_digit_pos >= 3) {
            finish_editing(true);
        } else {
            update_digit_display();
        }
    }
}

static void start_target_edit(void)
{
    grill_state_lock();
    int cur = grill_state_get()->grill_target;
    grill_state_unlock();

    s_digit_vals[0] = (cur / 100) % 10;
    s_digit_vals[1] = (cur / 10) % 10;
    s_digit_vals[2] = cur % 10;
    if (s_digit_vals[0] < 1) s_digit_vals[0] = 1;
    if (s_digit_vals[0] > 4) s_digit_vals[0] = 4;
    s_digit_pos = 0;
    s_editing = true;

    // Show digit editor, hide normal target label
    lv_obj_clear_flag(s_digit_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_lbl_target, LV_OBJ_FLAG_HIDDEN);
    update_digit_display();

    // Remove menu from group, take direct encoder control
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    // Consume any pending button event from the menu click that got us here
    encoder_get_button_event();
    encoder_get_diff();

    ui_encoder_set_direct(true);
    // Delay timer start slightly so the button release doesn't bleed through
    s_edit_timer = lv_timer_create(edit_timer_cb, 80, NULL);
}

// --- Navigation ---

static void navigate_to(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *next = NULL;

    switch (idx) {
    case MENU_SET_TARGET:
        start_target_edit();
        return;  // don't navigate — edit in place
    case MENU_IGNITE: {
        grill_state_lock();
        grill_mode_t im = grill_state_get()->mode;
        grill_state_unlock();
        if (im == GRILL_MODE_START) {
            // STOP — kill ignition immediately, no confirmation needed
            ESP_LOGI(TAG, "IGNITE STOPPED");
            grill_state_lock();
            grill_state_t *gs2 = grill_state_get();
            gs2->mode = GRILL_MODE_OFF;
            gs2->fan_on = false;
            gs2->auger_on = false;
            gs2->igniter_on = false;
            grill_state_unlock();
            ui_main_menu_update();
            return;
        }
        next = ui_ignite_create();
        break;
    }
    case MENU_COOK_MODE: {
        grill_state_lock();
        grill_mode_t m = grill_state_get()->mode;
        grill_state_unlock();
        // Can't set cook mode in Off or Ignite — grill not at temp yet
        if (m == GRILL_MODE_OFF || m == GRILL_MODE_START) {
            ESP_LOGI(TAG, "Cook mode not available in %s", grill_mode_name(m));
            return;
        }
        next = ui_cook_mode_create();
        break;
    }
    case MENU_SET_PROBES:
        next = ui_set_probes_create();
        break;
    case MENU_DASHBOARD:
        next = ui_dashboard_create();
        break;
    case MENU_SETTINGS:
    case MENU_GRAPHS:
        ESP_LOGI(TAG, "%s: coming soon", menu_labels[idx]);
        return;
    }

    if (next) {
        s_screen = NULL;
        lv_scr_load(next);
    }
}

// --- Group setup ---

static void setup_menu_group(void)
{
    lv_group_t *g = lv_group_get_default();
    if (!g) return;
    lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);
    for (int i = 0; i < NUM_MENU_ITEMS + 1; i++) {
        lv_group_add_obj(g, s_menu_btns[i]);
    }
}

// --- Screen creation ---

lv_obj_t *ui_main_menu_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_editing = false;
    int y = 4;

    // Cook Mode status — same font as menu items
    s_lbl_mode = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_lbl_mode, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(s_lbl_mode, 8, y);
    y += 26;

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

    int target_y = y;

    // Normal target label (shown when not editing)
    s_lbl_target = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_target, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_target, UI_COLOR_ACCENT, 0);
    lv_obj_align(s_lbl_target, LV_ALIGN_TOP_MID, 0, target_y);

    // Digit editor container (hidden initially)
    s_digit_container = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_digit_container);
    lv_obj_set_size(s_digit_container, 220, 40);
    lv_obj_set_pos(s_digit_container, 50, target_y);
    lv_obj_clear_flag(s_digit_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_digit_container, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < 3; i++) {
        s_digit_boxes[i] = lv_obj_create(s_digit_container);
        lv_obj_remove_style_all(s_digit_boxes[i]);
        lv_obj_set_size(s_digit_boxes[i], 50, 38);
        lv_obj_set_pos(s_digit_boxes[i], i * 58, 0);
        lv_obj_set_style_bg_color(s_digit_boxes[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_digit_boxes[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_digit_boxes[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_digit_boxes[i], 2, 0);
        lv_obj_set_style_border_opa(s_digit_boxes[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(s_digit_boxes[i], 4, 0);
        lv_obj_clear_flag(s_digit_boxes[i], LV_OBJ_FLAG_SCROLLABLE);

        s_digit_lbls[i] = lv_label_create(s_digit_boxes[i]);
        lv_label_set_text(s_digit_lbls[i], "0");
        lv_obj_set_style_text_font(s_digit_lbls[i], &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(s_digit_lbls[i], UI_COLOR_ACCENT, 0);
        lv_obj_center(s_digit_lbls[i]);
    }

    // °F after digits
    lv_obj_t *lbl_f = lv_label_create(s_digit_container);
    lv_label_set_text(lbl_f, "\xC2\xB0""F");
    lv_obj_set_style_text_font(lbl_f, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_f, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lbl_f, 178, 8);

    y = target_y + 36;

    // MAIN MENU label
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "MAIN MENU");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 20;

    // Menu item buttons
    for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        s_menu_btns[i] = lv_button_create(s_screen);
        s_menu_btns[i] = lv_button_create(s_screen);
        lv_obj_set_size(s_menu_btns[i], 304, 36);
        lv_obj_set_pos(s_menu_btns[i], 8, y);
        lv_obj_set_style_bg_color(s_menu_btns[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_menu_btns[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_menu_btns[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_menu_btns[i], 2, 0);
        lv_obj_set_style_border_opa(s_menu_btns[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(s_menu_btns[i], 4, 0);
        lv_obj_set_style_pad_left(s_menu_btns[i], 6, 0);
        lv_obj_set_style_pad_top(s_menu_btns[i], 2, 0);
        lv_obj_set_style_pad_bottom(s_menu_btns[i], 2, 0);
        lv_obj_set_style_shadow_width(s_menu_btns[i], 0, 0);
        lv_obj_set_style_border_opa(s_menu_btns[i], LV_OPA_COVER, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(s_menu_btns[i], UI_COLOR_ACCENT, LV_STATE_FOCUSED);

        lv_obj_add_event_cb(s_menu_btns[i], navigate_to, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *l = lv_label_create(s_menu_btns[i]);
        lv_label_set_text(l, menu_labels[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

        if (i == MENU_IGNITE) s_lbl_ignite = l;

        y += 40;
    }

    // Cook Dashboard button
    s_menu_btns[NUM_MENU_ITEMS] = lv_button_create(s_screen);
    lv_obj_set_size(s_menu_btns[NUM_MENU_ITEMS], 150, 32);
    lv_obj_align(s_menu_btns[NUM_MENU_ITEMS], LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_obj_set_style_bg_color(s_menu_btns[NUM_MENU_ITEMS], lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(s_menu_btns[NUM_MENU_ITEMS], UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_menu_btns[NUM_MENU_ITEMS], 2, 0);
    lv_obj_set_style_radius(s_menu_btns[NUM_MENU_ITEMS], 4, 0);
    lv_obj_set_style_shadow_width(s_menu_btns[NUM_MENU_ITEMS], 0, 0);
    lv_obj_set_style_bg_color(s_menu_btns[NUM_MENU_ITEMS], UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(s_menu_btns[NUM_MENU_ITEMS], LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s_menu_btns[NUM_MENU_ITEMS], navigate_to, LV_EVENT_CLICKED, (void *)(intptr_t)MENU_DASHBOARD);

    lv_obj_t *lbl_dash = lv_label_create(s_menu_btns[NUM_MENU_ITEMS]);
    lv_label_set_text(lbl_dash, "Cook Dashboard");
    lv_obj_set_style_text_font(lbl_dash, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_dash, UI_COLOR_ACCENT, 0);
    lv_obj_center(lbl_dash);

    // Setup encoder group
    setup_menu_group();

    // Initial data
    ui_main_menu_update();

    return s_screen;
}

void ui_main_menu_update(void)
{
    if (!s_screen) return;

    grill_state_lock();
    grill_state_t *gs = grill_state_get();

    lv_label_set_text_fmt(s_lbl_mode, "Cook Mode: %s", grill_mode_name(gs->mode));
    lv_label_set_text_fmt(s_lbl_current, "%.0f\xC2\xB0""F", gs->grill_temp);

    if (!s_editing) {
        lv_label_set_text_fmt(s_lbl_target, "%d\xC2\xB0""F", gs->grill_target);
    }

    // Dynamic ignite label
    if (s_lbl_ignite) {
        if (gs->mode == GRILL_MODE_START) {
            lv_label_set_text(s_lbl_ignite, "STOP NOW - IGNITE OFF");
        } else {
            lv_label_set_text(s_lbl_ignite, "START NOW - IGNITE");
        }
        // Gray out when in active cook modes (not Off or Ignite)
        bool ign_disabled = (gs->mode != GRILL_MODE_OFF && gs->mode != GRILL_MODE_START);
        lv_obj_set_style_text_color(s_lbl_ignite,
            ign_disabled ? UI_COLOR_TEXT_DIM : lv_color_hex(0xFFFFFF), 0);
    }

    // Gray out COOK MODE when in Off or Ignite
    if (s_menu_btns[MENU_COOK_MODE]) {
        bool cm_disabled = (gs->mode == GRILL_MODE_OFF || gs->mode == GRILL_MODE_START);
        lv_obj_t *child = lv_obj_get_child(s_menu_btns[MENU_COOK_MODE], 0);
        if (child) {
            lv_obj_set_style_text_color(child,
                cm_disabled ? UI_COLOR_TEXT_DIM : lv_color_hex(0xFFFFFF), 0);
        }
    }

    grill_state_unlock();
}
