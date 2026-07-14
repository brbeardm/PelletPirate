#ifndef ACTUATOR_H
#define ACTUATOR_H

// Fan / Auger / Igniter control for the MOC3063 triac outputs.
//
// Control logic ported from PelletPirateC++_WB/src/main.cpp (DoMode /
// DoAugerControl / checkIgniter / SetMode) — see actuator.cpp header
// for the carryover/enhancement summary.
//
// GPIO map (V2 board): FAN=IO4, IGNITER=IO32, AUGER=IO33.
// All three MUST be parked LOW at the very start of app_main() (done in
// main.c) — floating triac-driver inputs are unacceptable on AC.

#ifdef __cplusplus
extern "C" {
#endif

#define ACTUATOR_FAN_GPIO     4
#define ACTUATOR_IGNITER_GPIO 32
#define ACTUATOR_AUGER_GPIO   33

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
