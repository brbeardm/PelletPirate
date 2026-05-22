#ifndef UI_MENU_H
#define UI_MENU_H

#include "lvgl.h"

/**
 * Create the main menu screen with 7 items.
 * Encoder scrolls, press selects.
 */
lv_obj_t *ui_menu_create(void);

#endif
