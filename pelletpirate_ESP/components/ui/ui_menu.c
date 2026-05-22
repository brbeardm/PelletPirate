// PelletPirate Main Menu
//
// 7 menu items, encoder navigates, press selects.
// Cook -> COOK screen, rest are stubs.

#include "ui_menu.h"
#include "ui_home.h"
#include "ui_styles.h"
#include "esp_log.h"

static const char *TAG = "ui_menu";

static lv_obj_t *s_screen;

// Menu item labels
static const char *menu_items[] = {
    "COOK",
    "PROBES",
    "PROGRAMS",
    "CONTROL",
    "HISTORY",
    "SETTINGS",
    "DIAGNOSTICS",
};
#define MENU_COUNT 7

// Stub screen — shown for unimplemented menu items
static lv_obj_t *s_stub_screen = NULL;

static void go_back_to_menu(lv_event_t *e)
{
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}

static void show_stub(const char *title)
{
    // Delete old stub if exists
    if (s_stub_screen) {
        lv_obj_delete(s_stub_screen);
    }

    s_stub_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_stub_screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_stub_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_stub_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *lbl = lv_label_create(s_stub_screen);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 40);

    // Coming soon text
    lv_obj_t *lbl2 = lv_label_create(s_stub_screen);
    lv_label_set_text(lbl2, "Coming Soon");
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl2, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl2, LV_ALIGN_CENTER, 0, 0);

    // Back button — encoder can press this
    lv_obj_t *btn = lv_button_create(s_stub_screen);
    lv_obj_set_size(btn, 200, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PANEL, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, go_back_to_menu, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "< BACK");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(btn_lbl, UI_COLOR_TEXT, 0);
    lv_obj_center(btn_lbl);

    // Add back button to encoder group
    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_remove_all_objs(g);
        lv_group_add_obj(g, btn);
    }

    lv_scr_load_anim(s_stub_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

static void menu_item_clicked(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Menu item %d selected: %s", idx, menu_items[idx]);

    if (idx == 0) {
        // COOK — load the COOK screen
        lv_obj_t *cook = ui_home_create();
        lv_scr_load_anim(cook, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    } else {
        // Stub for everything else
        show_stub(menu_items[idx]);
    }
}

lv_obj_t *ui_menu_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "PELLET PIRATE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_ACCENT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    // Divider
    lv_obj_t *div = lv_obj_create(s_screen);
    lv_obj_set_size(div, 300, 2);
    lv_obj_set_pos(div, 10, 46);
    lv_obj_set_style_bg_color(div, UI_COLOR_BAR_BG, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_radius(div, 0, 0);

    // Get default encoder group — disable wrap to avoid skip bug
    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_remove_all_objs(g);
        lv_group_set_wrap(g, false);
    }

    // Menu buttons — big, bold, easy to read
    for (int i = 0; i < MENU_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(s_screen);
        lv_obj_set_size(btn, 300, 52);
        lv_obj_set_pos(btn, 10, 56 + i * 58);
        lv_obj_set_style_bg_color(btn, UI_COLOR_PANEL, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_width(btn, 0, 0);

        // Highlight style when focused by encoder
        lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        lv_obj_add_event_cb(btn, menu_item_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, menu_items[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
        lv_obj_center(lbl);

        // Add to encoder group
        if (g) {
            lv_group_add_obj(g, btn);
        }
    }

    return s_screen;
}
