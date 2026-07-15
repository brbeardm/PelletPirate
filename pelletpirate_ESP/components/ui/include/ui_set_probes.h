#ifndef UI_SET_PROBES_H
#define UI_SET_PROBES_H

#include "lvgl.h"

lv_obj_t *ui_set_probes_create(void);

/**
 * Open Set Probes directly at Level 2 for one probe (0-3).
 * Used by the dashboard probe rows; exits return to the dashboard.
 */
lv_obj_t *ui_set_probes_create_for(int probe_idx);

#endif
