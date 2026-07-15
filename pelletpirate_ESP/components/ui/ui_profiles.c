// Cook Profiles — save/load snapshots of grill target + probe configs.
//
// SAVE CURRENT COOK at top, then saved profiles newest-first (click to
// load, hold to arm delete then click to confirm; scrolling away cancels),
// Main at the bottom. Profiles are auto-named from date + first configured
// meat; loading applies immediately and returns to the menu.
//
// Rows use LV_EVENT_SHORT_CLICKED, not CLICKED: LVGL still fires CLICKED
// on the release of a long press, which would confirm the delete that
// same press just armed.

#include "ui_profiles.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "cooklog.h"
#include "profiles.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_profiles";

#define MAX_ROWS 8

static lv_obj_t *s_screen;
static profile_info_t s_profiles[PROFILES_LIST_MAX];
static int s_count;
static lv_obj_t *s_lbl_status;
static int s_confirm_idx = -1;  // row armed for delete, -1 = none

// Screen changes are deferred via lv_async_call: deleting the old screen
// inside a button's own event handler crashes. lv_group_remove_all_objs()
// in the new screen's create runs first, so by deletion time the button
// has no group, LVGL skips its indev reset (lv_obj_tree.c obj_delete_core),
// and the release sequence then sends CLICKED to the freed button.
static void async_show_menu(void *unused)
{
    ui_load_screen(ui_main_menu_create());
}

static void async_show_profiles(void *unused)
{
    ui_load_screen(ui_profiles_create());
}

static void go_main(lv_event_t *e)
{
    s_screen = NULL;
    lv_async_call(async_show_menu, NULL);
}

static void save_clicked(lv_event_t *e)
{
    if (!s_screen) return;  // screen change already pending
    char name[PROFILE_NAME_MAX];
    if (profiles_save_current(name, sizeof(name))) {
        cooklog_event("lcd", "PROFILE SAVE %s", name);
        // Rebuild the screen so the new profile appears in the list
        s_screen = NULL;
        lv_async_call(async_show_profiles, NULL);
    } else {
        lv_label_set_text(s_lbl_status, "Save failed (storage?)");
    }
}

static void profile_long_pressed(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_count || s_confirm_idx == idx) return;
    s_confirm_idx = idx;
    lv_obj_t *btn = lv_event_get_target(e);
    lv_label_set_text(lv_obj_get_child(btn, 0), "DELETE?");
    lv_obj_set_style_bg_color(btn, UI_COLOR_RED, LV_STATE_FOCUSED);
    lv_label_set_text(s_lbl_status, "Click = delete, scroll away = keep");
}

static void profile_defocused(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_confirm_idx != idx) return;
    s_confirm_idx = -1;
    lv_obj_t *btn = lv_event_get_target(e);
    lv_label_set_text(lv_obj_get_child(btn, 0), s_profiles[idx].display);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_label_set_text(s_lbl_status, "");
}

static void profile_clicked(lv_event_t *e)
{
    if (!s_screen) return;  // screen change already pending
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_count) return;
    if (s_confirm_idx == idx) {
        s_confirm_idx = -1;
        if (profiles_delete(s_profiles[idx].fname)) {
            cooklog_event("lcd", "PROFILE DELETE %s", s_profiles[idx].display);
            s_screen = NULL;
            lv_async_call(async_show_profiles, NULL);
        } else {
            lv_label_set_text(s_lbl_status, "Delete failed");
        }
        return;
    }
    if (profiles_load(s_profiles[idx].fname)) {
        cooklog_event("lcd", "PROFILE LOAD %s", s_profiles[idx].display);
        ESP_LOGI(TAG, "loaded %s", s_profiles[idx].fname);
        go_main(e);  // back to menu, loaded target visible immediately
    } else {
        lv_label_set_text(s_lbl_status, "Load failed");
    }
}

// Accent-colored row text would vanish on the orange focus bar — invert
// to black while focused (same convention as digit editing).
static void accent_row_focus_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0),
        lv_event_get_code(e) == LV_EVENT_FOCUSED ? lv_color_hex(0x000000)
                                                 : UI_COLOR_ACCENT, 0);
}

static lv_obj_t *make_row(int y, const char *text, bool accent)
{
    lv_obj_t *btn = lv_button_create(s_screen);
    lv_obj_set_size(btn, 304, 36);
    lv_obj_set_pos(btn, 8, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, accent ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_left(btn, 6, 0);
    lv_obj_set_style_pad_top(btn, 2, 0);
    lv_obj_set_style_pad_bottom(btn, 2, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_FOCUSED);

    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l, accent ? UI_COLOR_ACCENT : lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    if (accent) {
        lv_obj_add_event_cb(btn, accent_row_focus_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(btn, accent_row_focus_cb, LV_EVENT_DEFOCUSED, NULL);
    }
    return btn;
}

lv_obj_t *ui_profiles_create(void)
{
    ui_encoder_set_direct(false);
    encoder_get_button_event();
    encoder_get_diff();

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_group_t *g = lv_group_get_default();
    if (g) { lv_group_remove_all_objs(g); lv_group_set_wrap(g, false); }

    int y = 4;

    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "COOK PROFILES");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 34;

    // Save-current row
    lv_obj_t *btn_save = make_row(y, "SAVE CURRENT COOK", true);
    lv_obj_add_event_cb(btn_save, save_clicked, LV_EVENT_CLICKED, NULL);
    if (g) lv_group_add_obj(g, btn_save);
    y += 44;

    // Saved profiles, newest first
    s_confirm_idx = -1;
    s_count = profiles_list(s_profiles, PROFILES_LIST_MAX);
    int shown = s_count < MAX_ROWS ? s_count : MAX_ROWS;
    for (int i = 0; i < shown; i++) {
        lv_obj_t *btn = make_row(y, s_profiles[i].display, false);
        lv_obj_add_event_cb(btn, profile_clicked, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, profile_long_pressed, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, profile_defocused, LV_EVENT_DEFOCUSED, (void *)(intptr_t)i);
        if (g) lv_group_add_obj(g, btn);
        y += 40;
    }
    if (s_count > 0) {
        // Bottom-right, opposite the Main button, so it never crowds the list
        lv_obj_t *hint = lv_label_create(s_screen);
        lv_label_set_text(hint, "Click = load  Hold = delete");
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -18);
    }
    if (s_count == 0) {
        lv_obj_t *none = lv_label_create(s_screen);
        lv_label_set_text(none, "No saved cooks yet");
        lv_obj_set_style_text_font(none, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(none, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(none, 14, y + 6);
        y += 30;
    }

    s_lbl_status = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_status, "");
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_status, UI_COLOR_RED, 0);
    lv_obj_set_pos(s_lbl_status, 14, y + 6);

    // Main button
    lv_obj_t *btn_main = lv_button_create(s_screen);
    lv_obj_set_size(btn_main, 110, 38);
    lv_obj_align(btn_main, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(btn_main, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(btn_main, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_main, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn_main, 2, 0);
    lv_obj_set_style_radius(btn_main, 4, 0);
    lv_obj_set_style_shadow_width(btn_main, 0, 0);
    lv_obj_set_style_bg_color(btn_main, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn_main, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_main, go_main, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(btn_main);
    lv_label_set_text(l1, "Main");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l1);
    if (g) lv_group_add_obj(g, btn_main);

    return s_screen;
}
