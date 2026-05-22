#ifndef UI_H
#define UI_H

#include "lvgl.h"

/**
 * Initialize LVGL, register display driver and encoder input device,
 * create the default theme, and load the home screen.
 * Must be called after hx8357d_init() and encoder_init().
 */
void ui_init(void);

/**
 * Get the LVGL encoder input device (for assigning groups).
 */
lv_indev_t *ui_get_encoder_indev(void);

#endif
