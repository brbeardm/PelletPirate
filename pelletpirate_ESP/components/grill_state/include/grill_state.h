#ifndef GRILL_STATE_H
#define GRILL_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GRILL_MODE_OFF,
    GRILL_MODE_START,
    GRILL_MODE_SMOKE,
    GRILL_MODE_COOKING,
    GRILL_MODE_WARMING,
    GRILL_MODE_IDLE,
    GRILL_MODE_SHUTDOWN,
} grill_mode_t;

typedef struct {
    bool enabled;
    float current_temp;
    float target_temp;
    char food_type[16];
    bool alarm_enabled;
    float calibration_offset;
} probe_state_t;

#define NUM_MEAT_PROBES 4

typedef struct {
    // Mode
    grill_mode_t mode;

    // Temperatures
    float grill_temp;
    int grill_target;
    probe_state_t probes[NUM_MEAT_PROBES];

    // Actuators
    bool fan_on;
    bool auger_on;
    bool igniter_on;

    // PID
    float pid_u;
    int pid_cycle;

    // Timers
    uint32_t cook_start_time;
    uint32_t shutdown_start_time;

    // WiFi
    bool wifi_connected;
    int wifi_rssi;

    // Diagnostics
    float auger_runtime_sec;
    float igniter_runtime_sec;
    uint32_t fan_cycles;
} grill_state_t;

/**
 * Initialize the grill state with defaults (Off mode, mock temps for testing).
 */
void grill_state_init(void);

/**
 * Get a pointer to the global grill state. Caller must use
 * grill_state_lock() / grill_state_unlock() for thread safety.
 */
grill_state_t *grill_state_get(void);

/**
 * Lock the grill state mutex. Call before reading/writing state
 * from any task other than the owner.
 */
void grill_state_lock(void);

/**
 * Unlock the grill state mutex.
 */
void grill_state_unlock(void);

/**
 * Get the mode name as a string (for display).
 */
const char *grill_mode_name(grill_mode_t mode);

#endif
