#ifndef BACKLIGHT_H
#define BACKLIGHT_H

/**
 * TPS61165 backlight driver control via LEDC PWM on the CTRL pin (IO17).
 *
 * Hardware constraints (see pelletpirate-context.md):
 * - PWM dimming is only valid in the 5 kHz - 100 kHz band
 * - CTRL held low > 2.5 ms puts the device into shutdown
 * - After an OVP latch-off (open LED / display unplugged), the device
 *   stays off until CTRL is toggled
 */

/**
 * Configure LEDC on the CTRL pin with the backlight OFF (duty 0),
 * and load the saved brightness level from NVS (default 100%).
 * Call after the early boot GPIO hold in app_main().
 */
void backlight_init(void);

/**
 * Turn the backlight on at the stored brightness level.
 */
void backlight_on(void);

/**
 * Set brightness. 0 = off; otherwise clamped to 10-100%.
 * Turning on from off (or after an OVP latch) toggles CTRL low for
 * >2.5 ms first so a latched TPS61165 restarts cleanly.
 */
void backlight_set_percent(int pct);

int backlight_get_percent(void);

/**
 * Persist the current brightness level to NVS.
 */
void backlight_save_to_nvs(void);

#endif
