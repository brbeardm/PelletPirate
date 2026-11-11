#ifndef HOPPER_H
#define HOPPER_H

#include "esp_err.h"

// V5 pellet hopper gauge — Sharp GP2Y0E02A analog IR ranger on J9/IO14
// (ADC2_CH3). Publishes smoothed %-full + health into grill_state for the
// LCD, web UI and push alerts. Strictly read-only: it never touches the
// grill state machine or actuators (v1 design — alerts, not actions).
//
// On V2/V4 builds every call is a no-op and grill_state.hopper_pct stays
// -1 (sensor absent), so shared UI code needs no target guards.

void hopper_init(void);

// Snapshot for the cal/settings UI. Any out pointer may be NULL.
// mv = smoothed sensor millivolts (live even when uncalibrated),
// pct = 0-100 or -1, depth_tenths = configured hopper depth in 0.1",
// empty_mv/full_mv = two-point capture cal (-1 = not captured).
void hopper_get_info(int *mv, int *pct, int *depth_tenths,
                     int *empty_mv, int *full_mv);

// Primary cal: hopper depth (sensor lens to hopper bottom), 60-180 tenths
// of an inch (6.0"-18.0"). 0 clears. Persists to NVS.
esp_err_t hopper_set_depth(int tenths);

// Advanced cal: capture the live reading as the EMPTY (0) or FULL (1)
// endpoint. When both are captured they override the depth math.
esp_err_t hopper_capture(int which);
esp_err_t hopper_clear_capture(void);

#endif
