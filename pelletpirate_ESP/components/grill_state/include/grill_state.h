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

typedef struct {
    bool enabled;
    float current_temp;
    float target_temp;
    char food_type[16];
    float alarm_temp;       // 0 = off, >0 = alarm threshold
    char alarm_type[16];    // "Wrap", "Beer Me", etc.
    float calibration_offset;
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

void grill_state_init(void);
grill_state_t *grill_state_get(void);
void grill_state_lock(void);
void grill_state_unlock(void);
const char *grill_mode_name(grill_mode_t mode);

#endif
