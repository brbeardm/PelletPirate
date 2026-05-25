// Cook Mode selection
//
// List: SMOKE, SUPER SMOKE, COOK, KEEP WARM, SHUTDOWN, RE-IGNITE, OFF
// Current active mode shown with distinct indicator.
// Encoder scrolls, press selects, Save applies, Main discards.

#include "ui_cook_mode.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "esp_log.h"

static const char *TAG = "ui_cookmode";

static lv_obj_t *s_screen;
static grill_mode_t s_selected_mode;

static const char *mode_labels[] = {
    "SMOKE",
    "SUPER SMOKE",
    "COOK",
    "KEEP WARM",
    "SHUTDOWN",
    "RE-IGNITE",
    "OFF",
};

static const grill_mode_t mode_values[] = {
    GRILL_MODE_SMOKE,
    GRILL_MODE_SUPER_SMOKE,
    GRILL_MODE_COOK,
    GRILL_MODE_KEEP_WARM,
    GRILL_MODE_SHUTDOWN,
    GRILL_MODE_REIGNITE,
    GRILL_MODE_OFF,
};

#define NUM_MODES 7

static lv_obj_t *s_mode_btns[NUM_MODES];

static void go_main(lv_event_t *e)
{
    s_screen = NULL;
    lv_obj_t *menu = ui_main_menu_create();
    ui_load_screen(menu);
}

static void do_save(lv_event_t *e)
{
    ESP_LOGI(TAG, "Setting cook mode to %s", grill_mode_name(s_selected_mode));
    grill_state_lock();
    grill_state_get()->mode = s_selected_mode;
    grill_state_unlock();
    go_main(e);
}

static void mode_clicked(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_selected_mode = mode_values[idx];

    // Update visual — highlight selected mode
    for (int i = 0; i < NUM_MODES; i++) {
        if (mode_values[i] == s_selected_mode) {
            lv_obj_set_style_bg_color(s_mode_btns[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_opa(s_mode_btns[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(s_mode_btns[i], lv_color_hex(0x000000), 0);
        }
    }
}

lv_obj_t *ui_cook_mode_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    grill_mode_t active = gs->mode;
    float cur_temp = gs->grill_temp;
    int cur_target = gs->grill_target;
    grill_state_unlock();

    s_selected_mode = active;

    int y = 4;

    // Cook Mode status — same font as menu items
    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text_fmt(lbl, "Cook Mode: %s", grill_mode_name(active));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 26;

    // Current temp
    lv_obj_t *lbl_cur = lv_label_create(s_screen);
    lv_label_set_text(lbl_cur, "CURRENT");
    lv_obj_set_style_text_font(lbl_cur, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_cur, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl_cur, LV_ALIGN_TOP_MID, 0, y);
    y += 16;

    lv_obj_t *lbl_temp = lv_label_create(s_screen);
    lv_label_set_text_fmt(lbl_temp, "%.0f\xC2\xB0""F", cur_temp);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_MID, 0, y);
    y += 56;

    // Target
    lv_obj_t *lbl_tgt = lv_label_create(s_screen);
    lv_label_set_text(lbl_tgt, "TARGET");
    lv_obj_set_style_text_font(lbl_tgt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_tgt, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl_tgt, LV_ALIGN_TOP_MID, 0, y);
    y += 16;

    lv_obj_t *lbl_tval = lv_label_create(s_screen);
    lv_label_set_text_fmt(lbl_tval, "%d\xC2\xB0""F", cur_target);
    lv_obj_set_style_text_font(lbl_tval, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_tval, UI_COLOR_ACCENT, 0);
    lv_obj_align(lbl_tval, LV_ALIGN_TOP_MID, 0, y);
    y += 36;

    // "SET COOK MODE" title
    lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "SET COOK MODE");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl, 8, y);
    y += 26;

    // Encoder group
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);

    // Mode buttons
    for (int i = 0; i < NUM_MODES; i++) {
        s_mode_btns[i] = lv_button_create(s_screen);
        lv_obj_set_size(s_mode_btns[i], 304, 34);
        lv_obj_set_pos(s_mode_btns[i], 8, y);
        lv_obj_set_style_radius(s_mode_btns[i], 4, 0);
        lv_obj_set_style_border_color(s_mode_btns[i], UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_mode_btns[i], 2, 0);
        lv_obj_set_style_border_opa(s_mode_btns[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(s_mode_btns[i], 0, 0);
        lv_obj_set_style_pad_left(s_mode_btns[i], 8, 0);

        // Current active mode gets filled bg
        if (mode_values[i] == active) {
            lv_obj_set_style_bg_color(s_mode_btns[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_opa(s_mode_btns[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(s_mode_btns[i], lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(s_mode_btns[i], LV_OPA_COVER, 0);
        }

        // Focus style: orange border
        lv_obj_set_style_border_opa(s_mode_btns[i], LV_OPA_COVER, LV_STATE_FOCUSED);

        lv_obj_add_event_cb(s_mode_btns[i], mode_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *lbl_m = lv_label_create(s_mode_btns[i]);
        lv_label_set_text(lbl_m, mode_labels[i]);
        lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_m, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl_m, LV_ALIGN_LEFT_MID, 0, 0);

        if (g) lv_group_add_obj(g, s_mode_btns[i]);
        y += 37;
    }

    // Main / Save buttons
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

    lv_obj_t *btn_save = lv_button_create(s_screen);
    lv_obj_set_size(btn_save, 110, 38);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(btn_save, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_save, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn_save, 2, 0);
    lv_obj_set_style_radius(btn_save, 4, 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    lv_obj_set_style_bg_color(btn_save, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn_save, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_save, do_save, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = lv_label_create(btn_save);
    lv_label_set_text(l2, "Save");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l2);
    if (g) lv_group_add_obj(g, btn_save);

    return s_screen;
}
