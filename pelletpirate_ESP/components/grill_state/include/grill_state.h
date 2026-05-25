#ifndef GRILL_STATE_H
#define GRILL_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GRILL_MODE_OFF,
    GRILL_MODE_START,
    GRILL_MODE_SMOKE,
    GRILL_MODE_SUPER_SMOKE,
    GRILL_MODE_COOK,
    GRILL_MODE_KEEP_WARM,
    GRILL_MODE_SHUTDOWN,
    GRILL_MODE_REIGNITE,
} grill_mode_t;

#define GRILL_MODE_COUNT 8

// Rolling temp history for EST calculation
// Sample every 5 minutes, keep 12 samples = 60 minutes of history
#define TEMP_HISTORY_SIZE 12
#define TEMP_HISTORY_INTERVAL_SEC 300  // 5 minutes

typedef struct {
    bool enabled;
    float current_temp;
    float target_temp;
    char food_type[16];
    float alarm_temp;       // 0 = off, >0 = alarm threshold
    char alarm_type[16];    // "Wrap", "Beer Me", etc.
    float calibration_offset;

    // Temp history for EST rolling average
    float temp_history[TEMP_HISTORY_SIZE];
    int history_count;      // how many valid entries (0 to TEMP_HISTORY_SIZE)
    int history_index;      // next write position (circular)
    float start_temp;       // temp when probe was first enabled in this cook
} probe_state_t;

#define NUM_MEAT_PROBES 4

// Target temp limits
#define TARGET_TEMP_MIN 100
#define TARGET_TEMP_MAX 499

// Ignite auto-disable threshold
#define IGNITE_DISABLE_TEMP 115.0f

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
    uint32_t cook_start_time;       // epoch seconds when cook started (0 = not cooking)
    uint32_t shutdown_start_time;
    uint32_t last_history_time;     // last time temp history was sampled

    // WiFi
    bool wifi_connected;
    int wifi_rssi;

    // Diagnostics
    float auger_runtime_sec;
    float igniter_runtime_sec;
    uint32_t fan_cycles;
} grill_state_t;

void grill_state_init(void);
grill_state_t *grill_state_get(void);
void grill_state_lock(void);
void grill_state_unlock(void);
const char *grill_mode_name(grill_mode_t mode);

/**
 * Record current probe temps into rolling history.
 * Call this every TEMP_HISTORY_INTERVAL_SEC from the main loop.
 */
void grill_state_record_history(void);

/**
 * Save current probe configs and grill target to NVS flash.
 * Call this when the user presses Save on the probe or target screens.
 */
void grill_state_save_to_nvs(void);

/**
 * Load probe configs and grill target from NVS flash.
 * Called automatically by grill_state_init(). If no saved data exists,
 * defaults are used.
 */
void grill_state_load_from_nvs(void);

/**
 * Get estimated minutes remaining for a probe based on rolling average.
 * Returns -1 if not enough data to estimate.
 */
int grill_state_get_est_minutes(int probe_idx);

/**
 * Get elapsed cook time in minutes.
 * Returns 0 if not cooking.
 */
int grill_state_get_elapsed_minutes(void);

#endif
