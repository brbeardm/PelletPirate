#ifndef ACTUATOR_H
#define ACTUATOR_H

// Fan / Auger / Igniter control for the MOC3063 triac outputs.
//
// Control logic ported from PelletPirateC++_WB/src/main.cpp (DoMode /
// DoAugerControl / checkIgniter / SetMode) — see actuator.cpp header
// for the carryover/enhancement summary.
//
// GPIO map lives in boardpins.h (V2 vs V4 per IDF target).
// All three MUST be parked LOW at the very start of app_main() (done in
// main.c) — floating triac-driver inputs are unacceptable on AC.

#include "boardpins.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACTUATOR_FAN_GPIO     BOARD_FAN_GPIO
#define ACTUATOR_IGNITER_GPIO BOARD_IGNITER_GPIO
#define ACTUATOR_AUGER_GPIO   BOARD_AUGER_GPIO

/**
 * Configure the three output GPIOs (LOW) and start the 1 Hz control task.
 * Call after grill_state_init(); reads mode/temps from grill_state and
 * writes actuator flags + diagnostics back.
 */
void actuator_init(void);

#ifdef __cplusplus
}
#endif

#endif
