#include "ui_styles.h"

lv_style_t style_screen;
lv_style_t style_panel;
lv_style_t style_title;
lv_style_t style_temp_large;
lv_style_t style_temp_small;
lv_style_t style_label;
lv_style_t style_label_dim;
lv_style_t style_status_bar;
lv_style_t style_icon_active;
lv_style_t style_icon_inactive;

void ui_styles_init(void)
{
    // Screen background
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, UI_COLOR_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, UI_COLOR_TEXT);

    // Panel / card
    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, UI_COLOR_PANEL);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_radius(&style_panel, 8);
    lv_style_set_pad_all(&style_panel, 8);
    lv_style_set_border_width(&style_panel, 0);

    // Title text
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, UI_COLOR_TEXT);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);

    // Large temp display (grill temp)
    lv_style_init(&style_temp_large);
    lv_style_set_text_color(&style_temp_large, UI_COLOR_ACCENT);
    lv_style_set_text_font(&style_temp_large, &lv_font_montserrat_48);

    // Small temp display (probe temps)
    lv_style_init(&style_temp_small);
    lv_style_set_text_color(&style_temp_small, UI_COLOR_TEXT);
    lv_style_set_text_font(&style_temp_small, &lv_font_montserrat_16);

    // General label
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, UI_COLOR_TEXT);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_14);

    // Dimmed label
    lv_style_init(&style_label_dim);
    lv_style_set_text_color(&style_label_dim, UI_COLOR_TEXT_DIM);
    lv_style_set_text_font(&style_label_dim, &lv_font_montserrat_14);

    // Status bar
    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, lv_color_hex(0x0F0F1A));
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_pad_all(&style_status_bar, 4);
    lv_style_set_border_width(&style_status_bar, 0);

    // Active icon
    lv_style_init(&style_icon_active);
    lv_style_set_text_color(&style_icon_active, UI_COLOR_GREEN);
    lv_style_set_text_font(&style_icon_active, &lv_font_montserrat_14);

    // Inactive icon
    lv_style_init(&style_icon_inactive);
    lv_style_set_text_color(&style_icon_inactive, UI_COLOR_TEXT_DIM);
    lv_style_set_text_font(&style_icon_inactive, &lv_font_montserrat_14);
}
