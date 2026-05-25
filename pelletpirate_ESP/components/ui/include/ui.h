#ifndef UI_H
#define UI_H

#include "lvgl.h"

void ui_init(void);
lv_indev_t *ui_get_encoder_indev(void);

/**
 * Disable/enable LVGL encoder input processing.
 * Screens that poll the encoder directly via timers must call
 * ui_encoder_set_direct(true) to prevent LVGL from consuming events.
 * Call ui_encoder_set_direct(false) when returning to group-based navigation.
 */
void ui_encoder_set_direct(bool direct);

/**
 * Load a new screen and delete the previous one to prevent memory leaks.
 */
void ui_load_screen(lv_obj_t *new_screen);

#endif
