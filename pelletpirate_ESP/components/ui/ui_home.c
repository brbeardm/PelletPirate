// PelletPirate COOK Screen (Home Dashboard)
//
// Layout (320x480 portrait) — designed for arm's length readability:
//
// ┌──────────────────────────┐  0
// │ COOK              Off    │  top bar (36px)
// ├──────────────────────────┤  36
// │                          │
// │         225°F            │  grill temp HUGE (montserrat_48)
// │       SET 250°F          │  target (montserrat_16)
// │                          │
// ├──────────────────────────┤  160
// │ BRISKET     142° → 203° │  probe 1 (70px) — big text
// │ ████████████░░░░░░░░░░░  │
// ├──────────────────────────┤  230
// │ CHICKEN      88° → 165° │  probe 2 (70px)
// │ ████░░░░░░░░░░░░░░░░░░  │
// ├──────────────────────────┤  300
// │ PROBE 3         ---     │  probe 3 (70px)
// ├──────────────────────────┤  370
// │ PROBE 4         ---     │  probe 4 (70px)
// ├──────────────────────────┤  440
// │  FAN    AUG    IGN      │  status bar (40px) — bold
// └──────────────────────────┘  480

#include "ui_home.h"
#include "ui_menu.h"
#include "ui_styles.h"
#include "grill_state.h"
#include <stdio.h>

static lv_obj_t *s_screen;
static lv_obj_t *s_lbl_mode;
static lv_obj_t *s_lbl_grill_temp;
static lv_obj_t *s_lbl_target;

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_temps;
    lv_obj_t *bar;
} probe_ui_t;

static probe_ui_t s_probes[NUM_MEAT_PROBES];
static lv_obj_t *s_lbl_fan;
static lv_obj_t *s_lbl_auger;
static lv_obj_t *s_lbl_igniter;

static void go_back_to_menu(lv_event_t *e)
{
    s_screen = NULL;  // prevent ui_home_update from touching deleted widgets
    lv_obj_t *menu = ui_menu_create();
    lv_scr_load_anim(menu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);
}

static lv_obj_t *create_probe_row(lv_obj_t *parent, int index, int y_pos)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 310, 62);
    lv_obj_set_pos(panel, 5, y_pos);
    lv_obj_set_style_bg_color(panel, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_pad_all(panel, 6, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Probe name (left, bold)
    s_probes[index].lbl_name = lv_label_create(panel);
    lv_obj_set_style_text_font(s_probes[index].lbl_name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_probes[index].lbl_name, UI_COLOR_ACCENT2, 0);
    lv_obj_align(s_probes[index].lbl_name, LV_ALIGN_TOP_LEFT, 0, 0);

    char name_buf[24];
    snprintf(name_buf, sizeof(name_buf), "PROBE %d", index + 1);
    lv_label_set_text(s_probes[index].lbl_name, name_buf);

    // Temps (right side, bold)
    s_probes[index].lbl_temps = lv_label_create(panel);
    lv_obj_set_style_text_font(s_probes[index].lbl_temps, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_probes[index].lbl_temps, UI_COLOR_TEXT, 0);
    lv_obj_align(s_probes[index].lbl_temps, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_label_set_text(s_probes[index].lbl_temps, "---");

    // Progress bar (full width, thicker)
    s_probes[index].bar = lv_bar_create(panel);
    lv_obj_set_size(s_probes[index].bar, 290, 14);
    lv_obj_align(s_probes[index].bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(s_probes[index].bar, 0, 100);
    lv_bar_set_value(s_probes[index].bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_probes[index].bar, UI_COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_probes[index].bar, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_probes[index].bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(s_probes[index].bar, 6, LV_PART_INDICATOR);

    s_probes[index].panel = panel;
    return panel;
}

lv_obj_t *ui_home_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // === Top bar ===
    lv_obj_t *top_bar = lv_obj_create(s_screen);
    lv_obj_set_size(top_bar, 320, 36);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x0F0F1A), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 6, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button — returns to main menu
    lv_obj_t *btn_back = lv_button_create(top_bar);
    lv_obj_set_size(btn_back, 100, 28);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, go_back_to_menu, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_title = lv_label_create(btn_back);
    lv_label_set_text(lbl_title, "< COOK");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_ACCENT, 0);
    lv_obj_center(lbl_title);

    s_lbl_mode = lv_label_create(top_bar);
    lv_label_set_text(s_lbl_mode, "Off");
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_mode, UI_COLOR_TEXT, 0);
    lv_obj_align(s_lbl_mode, LV_ALIGN_RIGHT_MID, -4, 0);

    // === Grill temp section — BIG ===
    s_lbl_grill_temp = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_grill_temp, "---");
    lv_obj_set_style_text_font(s_lbl_grill_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_grill_temp, UI_COLOR_ACCENT, 0);
    lv_obj_align(s_lbl_grill_temp, LV_ALIGN_TOP_MID, 0, 60);

    // Target temp
    s_lbl_target = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_target, "SET 225\xC2\xB0""F");
    lv_obj_set_style_text_font(s_lbl_target, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_target, UI_COLOR_ACCENT2, 0);
    lv_obj_align(s_lbl_target, LV_ALIGN_TOP_MID, 0, 120);

    // Divider
    lv_obj_t *divider = lv_obj_create(s_screen);
    lv_obj_set_size(divider, 300, 2);
    lv_obj_set_pos(divider, 10, 152);
    lv_obj_set_style_bg_color(divider, UI_COLOR_BAR_BG, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);

    // === Probe rows ===
    create_probe_row(s_screen, 0, 160);
    create_probe_row(s_screen, 1, 230);
    create_probe_row(s_screen, 2, 300);
    create_probe_row(s_screen, 3, 370);

    // === Bottom status bar — BOLD ===
    lv_obj_t *status_bar = lv_obj_create(s_screen);
    lv_obj_set_size(status_bar, 320, 40);
    lv_obj_set_pos(status_bar, 0, 440);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x0F0F1A), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 4, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_fan = lv_label_create(status_bar);
    lv_label_set_text(s_lbl_fan, "FAN");
    lv_obj_set_style_text_font(s_lbl_fan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_fan, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(s_lbl_fan, LV_ALIGN_LEFT_MID, 20, 0);

    s_lbl_auger = lv_label_create(status_bar);
    lv_label_set_text(s_lbl_auger, "AUG");
    lv_obj_set_style_text_font(s_lbl_auger, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_auger, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(s_lbl_auger, LV_ALIGN_CENTER, 0, 0);

    s_lbl_igniter = lv_label_create(status_bar);
    lv_label_set_text(s_lbl_igniter, "IGN");
    lv_obj_set_style_text_font(s_lbl_igniter, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_igniter, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(s_lbl_igniter, LV_ALIGN_RIGHT_MID, -20, 0);

    // Add back button to encoder group
    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_remove_all_objs(g);
        lv_group_add_obj(g, btn_back);
    }

    return s_screen;
}

void ui_home_update(void)
{
    // Don't update if the home screen hasn't been created yet (splash is playing)
    if (s_screen == NULL) return;

    grill_state_lock();
    grill_state_t *gs = grill_state_get();

    // Mode
    lv_label_set_text(s_lbl_mode, grill_mode_name(gs->mode));

    // Grill temp — BIG
    if (gs->grill_temp > 0) {
        lv_label_set_text_fmt(s_lbl_grill_temp, "%.0f\xC2\xB0""F", gs->grill_temp);
    } else {
        lv_label_set_text(s_lbl_grill_temp, "---");
    }

    // Target
    lv_label_set_text_fmt(s_lbl_target, "SET %d\xC2\xB0""F", gs->grill_target);

    // Probes
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (gs->probes[i].enabled) {
            // Name
            lv_label_set_text(s_probes[i].lbl_name, gs->probes[i].food_type);

            // Temps
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0 \xE2\x86\x92 %.0f\xC2\xB0",
                     gs->probes[i].current_temp, gs->probes[i].target_temp);
            lv_label_set_text(s_probes[i].lbl_temps, buf);

            // Progress bar
            float range = gs->probes[i].target_temp - 32.0f;
            float progress = gs->probes[i].current_temp - 32.0f;
            int pct = (range > 0) ? (int)((progress / range) * 100.0f) : 0;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            lv_bar_set_value(s_probes[i].bar, pct, LV_ANIM_ON);
            lv_obj_clear_flag(s_probes[i].bar, LV_OBJ_FLAG_HIDDEN);

            // Color: orange -> yellow -> red as approaching target
            if (pct >= 90) {
                lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_RED, LV_PART_INDICATOR);
            } else if (pct >= 70) {
                lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_YELLOW, LV_PART_INDICATOR);
            } else {
                lv_obj_set_style_bg_color(s_probes[i].bar, UI_COLOR_ACCENT, LV_PART_INDICATOR);
            }
        } else {
            char name_buf[16];
            snprintf(name_buf, sizeof(name_buf), "PROBE %d", i + 1);
            lv_label_set_text(s_probes[i].lbl_name, name_buf);
            lv_label_set_text(s_probes[i].lbl_temps, "---");
            lv_bar_set_value(s_probes[i].bar, 0, LV_ANIM_OFF);
            lv_obj_add_flag(s_probes[i].bar, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Status icons — bold color when active
    lv_obj_set_style_text_color(s_lbl_fan,
        gs->fan_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(s_lbl_auger,
        gs->auger_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(s_lbl_igniter,
        gs->igniter_on ? UI_COLOR_RED : UI_COLOR_TEXT_DIM, 0);

    grill_state_unlock();
}
