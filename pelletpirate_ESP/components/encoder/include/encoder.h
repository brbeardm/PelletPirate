#ifndef ENCODER_H
#define ENCODER_H

#include <stdbool.h>
#include <stdint.h>
#include "boardpins.h"

// Encoder pins (PEC16-4215F-S0024) — per-board map in boardpins.h
#define ENCODER_PIN_A   BOARD_ENCODER_A
#define ENCODER_PIN_B   BOARD_ENCODER_B
#define ENCODER_PIN_BTN BOARD_ENCODER_BTN

typedef enum {
    ENCODER_BTN_NONE,
    ENCODER_BTN_SHORT,
    ENCODER_BTN_LONG,
} encoder_btn_event_t;

// Long press threshold in milliseconds
#define ENCODER_LONG_PRESS_MS 800

/**
 * Initialize encoder GPIOs and start the polling task.
 * Must be called after FreeRTOS scheduler is running.
 */
void encoder_init(void);

/**
 * Get accumulated rotation steps since last call.
 * Positive = clockwise, negative = counter-clockwise.
 * Resets the counter after reading.
 */
int encoder_get_diff(void);

/**
 * Get the current button state (pressed or not).
 */
bool encoder_button_pressed(void);

/**
 * Get and clear the last button event (short press, long press, or none).
 * Returns the event and clears it so it's only reported once.
 */
encoder_btn_event_t encoder_get_button_event(void);

/**
 * Microseconds since the last rotation or button press (boot counts as
 * activity). Drives the LCD idle-dimming timer.
 */
int64_t encoder_idle_us(void);

#endif
