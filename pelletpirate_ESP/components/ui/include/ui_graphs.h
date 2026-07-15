#ifndef UI_GRAPHS_H
#define UI_GRAPHS_H

#include "lvgl.h"

/** Rolling 1-hour temperature graph: grill, target, enabled probes. */
lv_obj_t *ui_graphs_create(void);

#endif
