#ifndef UI_SPLASH_H
#define UI_SPLASH_H

#include "lvgl.h"

/**
 * Create and show the animated splash screen.
 * Logo fades in, text slides in, loading bar fills.
 * Calls on_complete callback when animation finishes.
 */
void ui_splash_create(void (*on_complete)(void));

#endif
