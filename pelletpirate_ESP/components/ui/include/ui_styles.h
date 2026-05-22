#ifndef UI_STYLES_H
#define UI_STYLES_H

#include "lvgl.h"

// PelletPirate color palette — dark theme for outdoor use
#define UI_COLOR_BG          lv_color_hex(0x1A1A2E)   // deep navy background
#define UI_COLOR_PANEL       lv_color_hex(0x16213E)   // panel background
#define UI_COLOR_TEXT        lv_color_hex(0xEEEEEE)   // primary text
#define UI_COLOR_TEXT_DIM    lv_color_hex(0x888888)   // secondary text
#define UI_COLOR_ACCENT      lv_color_hex(0xFF6B35)   // orange accent (fire/heat)
#define UI_COLOR_ACCENT2     lv_color_hex(0x00D4AA)   // teal accent (cool/info)
#define UI_COLOR_RED         lv_color_hex(0xFF4444)   // alarm/hot
#define UI_COLOR_GREEN       lv_color_hex(0x44FF44)   // ok/active
#define UI_COLOR_YELLOW      lv_color_hex(0xFFCC00)   // warning
#define UI_COLOR_BAR_BG      lv_color_hex(0x333355)   // progress bar background

/**
 * Initialize styles used across all screens.
 */
void ui_styles_init(void);

// Shared styles — initialized by ui_styles_init()
extern lv_style_t style_screen;        // screen background
extern lv_style_t style_panel;         // card/panel background
extern lv_style_t style_title;         // screen titles
extern lv_style_t style_temp_large;    // large temperature display
extern lv_style_t style_temp_small;    // probe temperature text
extern lv_style_t style_label;         // general labels
extern lv_style_t style_label_dim;     // dimmed labels
extern lv_style_t style_status_bar;    // bottom status bar
extern lv_style_t style_icon_active;   // active status icon
extern lv_style_t style_icon_inactive; // inactive status icon

#endif
