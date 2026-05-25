// PelletPirate Splash Screen
//
// Layout:
//   "PelletPirate"              ← LVGL montserrat_48, black text
//   [150x148 logo]              ← bitmap centered
//   "It's a Pirate's Life for Me!" ← LVGL montserrat_16, black text
//   [loading bar]

#include "ui_splash.h"
#include "ui_styles.h"
#include "hx8357d.h"

extern const lv_image_dsc_t logo_pelletpirate;

static lv_obj_t *s_screen;
static lv_obj_t *s_lbl_title;
static lv_obj_t *s_logo;
static lv_obj_t *s_lbl_tagline;
static lv_obj_t *s_bar_loading;
static void (*s_on_complete)(void) = NULL;

static void splash_timer_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    if (s_on_complete) {
        void (*cb)(void) = s_on_complete;
        s_on_complete = NULL;
        cb();
    }
}

static void loading_complete_cb(lv_event_t *e)
{
    if (s_on_complete) {
        lv_timer_create(splash_timer_cb, 300, NULL);
    }
}

void ui_splash_create(void (*on_complete)(void))
{
    s_on_complete = on_complete;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // "PelletPirate" title — large, centered above logo
    s_lbl_title = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_title, "PelletPirate");
    lv_obj_set_style_text_font(s_lbl_title, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(0x000000), 0);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_opa(s_lbl_title, LV_OPA_TRANSP, 0);

    // Logo centered
    s_logo = lv_image_create(s_screen);
    lv_image_set_src(s_logo, &logo_pelletpirate);
    lv_obj_align(s_logo, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_opa(s_logo, LV_OPA_TRANSP, 0);

    // Tagline below logo
    s_lbl_tagline = lv_label_create(s_screen);
    lv_label_set_text(s_lbl_tagline, "It's a Pirate's Life for Me!");
    lv_obj_set_style_text_font(s_lbl_tagline, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_tagline, lv_color_hex(0x000000), 0);
    lv_obj_align(s_lbl_tagline, LV_ALIGN_CENTER, 0, 100);
    lv_obj_set_style_opa(s_lbl_tagline, LV_OPA_TRANSP, 0);

    // Loading bar at bottom
    s_bar_loading = lv_bar_create(s_screen);
    lv_obj_set_size(s_bar_loading, 260, 8);
    lv_obj_align(s_bar_loading, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_bar_set_range(s_bar_loading, 0, 100);
    lv_bar_set_value(s_bar_loading, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_loading, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar_loading, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_loading, lv_color_hex(0x000000), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar_loading, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_loading, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_loading, 3, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_bar_loading, LV_OPA_TRANSP, 0);

    lv_scr_load(s_screen);

    // Animation: title fade in (0 -> 0.6s)
    lv_anim_t a_title;
    lv_anim_init(&a_title);
    lv_anim_set_var(&a_title, s_lbl_title);
    lv_anim_set_values(&a_title, 0, 255);
    lv_anim_set_duration(&a_title, 600);
    lv_anim_set_delay(&a_title, 200);
    lv_anim_set_exec_cb(&a_title, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&a_title);

    // Logo fade in (0.3s -> 1.1s)
    lv_anim_t a_logo;
    lv_anim_init(&a_logo);
    lv_anim_set_var(&a_logo, s_logo);
    lv_anim_set_values(&a_logo, 0, 255);
    lv_anim_set_duration(&a_logo, 800);
    lv_anim_set_delay(&a_logo, 300);
    lv_anim_set_exec_cb(&a_logo, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&a_logo);

    // Tagline fade in (0.8s -> 1.4s)
    lv_anim_t a_tag;
    lv_anim_init(&a_tag);
    lv_anim_set_var(&a_tag, s_lbl_tagline);
    lv_anim_set_values(&a_tag, 0, 255);
    lv_anim_set_duration(&a_tag, 600);
    lv_anim_set_delay(&a_tag, 800);
    lv_anim_set_exec_cb(&a_tag, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&a_tag);

    // Loading bar appears (1.2s)
    lv_anim_t a_bar_show;
    lv_anim_init(&a_bar_show);
    lv_anim_set_var(&a_bar_show, s_bar_loading);
    lv_anim_set_values(&a_bar_show, 0, 255);
    lv_anim_set_duration(&a_bar_show, 300);
    lv_anim_set_delay(&a_bar_show, 1200);
    lv_anim_set_exec_cb(&a_bar_show, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&a_bar_show);

    // Loading bar fills (1.4s -> 3.0s)
    lv_anim_t a_bar_fill;
    lv_anim_init(&a_bar_fill);
    lv_anim_set_var(&a_bar_fill, s_bar_loading);
    lv_anim_set_values(&a_bar_fill, 0, 100);
    lv_anim_set_duration(&a_bar_fill, 1600);
    lv_anim_set_delay(&a_bar_fill, 1400);
    lv_anim_set_exec_cb(&a_bar_fill, (lv_anim_exec_xcb_t)lv_bar_set_value);
    lv_anim_set_completed_cb(&a_bar_fill, loading_complete_cb);
    lv_anim_start(&a_bar_fill);
}
