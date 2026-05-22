// PelletPirate Splash Screen
//
// Logo (300x448) centered on white background, fades in.
// Loading bar below, fills, then transitions to COOK screen.

#include "ui_splash.h"
#include "ui_styles.h"
#include "hx8357d.h"

extern const lv_image_dsc_t logo_pelletpirate;

static lv_obj_t *s_screen;
static lv_obj_t *s_logo;
static lv_obj_t *s_bar_loading;
static void (*s_on_complete)(void) = NULL;

static void splash_timer_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    if (s_on_complete) {
        void (*cb)(void) = s_on_complete;
        s_on_complete = NULL;  // fire only once
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

    // Logo centered (300x448 on 320x480 = 10px margin each side, 16px top)
    s_logo = lv_image_create(s_screen);
    lv_image_set_src(s_logo, &logo_pelletpirate);
    lv_obj_center(s_logo);
    lv_obj_set_style_opa(s_logo, LV_OPA_TRANSP, 0);

    // Loading bar at bottom
    s_bar_loading = lv_bar_create(s_screen);
    lv_obj_set_size(s_bar_loading, 260, 6);
    lv_obj_align(s_bar_loading, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_bar_set_range(s_bar_loading, 0, 100);
    lv_bar_set_value(s_bar_loading, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_loading, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_loading, lv_color_hex(0x333333), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_loading, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_loading, 3, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_bar_loading, LV_OPA_TRANSP, 0);

    lv_scr_load(s_screen);

    // Logo fade in (0 -> 1s)
    lv_anim_t a_logo;
    lv_anim_init(&a_logo);
    lv_anim_set_var(&a_logo, s_logo);
    lv_anim_set_values(&a_logo, 0, 255);
    lv_anim_set_duration(&a_logo, 1000);
    lv_anim_set_delay(&a_logo, 200);
    lv_anim_set_exec_cb(&a_logo, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&a_logo);

    // Loading bar appears (1s)
    lv_anim_t a_bar_show;
    lv_anim_init(&a_bar_show);
    lv_anim_set_var(&a_bar_show, s_bar_loading);
    lv_anim_set_values(&a_bar_show, 0, 255);
    lv_anim_set_duration(&a_bar_show, 300);
    lv_anim_set_delay(&a_bar_show, 1000);
    lv_anim_set_exec_cb(&a_bar_show, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&a_bar_show);

    // Loading bar fills (1.2s -> 3.2s)
    lv_anim_t a_bar_fill;
    lv_anim_init(&a_bar_fill);
    lv_anim_set_var(&a_bar_fill, s_bar_loading);
    lv_anim_set_values(&a_bar_fill, 0, 100);
    lv_anim_set_duration(&a_bar_fill, 2000);
    lv_anim_set_delay(&a_bar_fill, 1200);
    lv_anim_set_exec_cb(&a_bar_fill, (lv_anim_exec_xcb_t)lv_bar_set_value);
    lv_anim_set_completed_cb(&a_bar_fill, loading_complete_cb);
    lv_anim_start(&a_bar_fill);
}
