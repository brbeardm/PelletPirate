#ifndef UI_HOME_H
#define UI_HOME_H

#include "lvgl.h"

/**
 * Create the home dashboard screen.
 * Returns the screen object.
 */
lv_obj_t *ui_home_create(void);

/**
 * Update the home screen with current grill state data.
 * Call periodically from the LVGL task.
 */
void ui_home_update(void);

#endif
