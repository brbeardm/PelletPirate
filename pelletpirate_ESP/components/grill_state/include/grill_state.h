#ifndef GRILL_STATE_H
#define GRILL_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

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

// Rolling graph ring (grill + target + all probes) for the LCD Graphs
// screen and GET /api/graph. Runs continuously, cooking or not.
#define GRAPH_HISTORY_SIZE 120
#define GRAPH_INTERVAL_SEC 30          // 120 x 30s = 1 hour window

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

// --- Alarm engine ---
// Alarms are evaluated by grill_state_alarms_update() (called after each
// temperature refresh) and mirrored by every interface (LCD banner, web).

typedef enum {
    ALARM_IDLE = 0,   // not armed, or armed and below threshold
    ALARM_ACTIVE,     // fired, unacknowledged
    ALARM_ACKED,      // acknowledged; re-arms when condition clears
    ALARM_DONE,       // one-shot alarms (probe action, goal): fired, acked,
                      // and spent — never re-arms until the next cook
} alarm_state_t;

// Alarm class for banner styling: goal alerts are good news
#define ALARM_CLASS_NONE  0
#define ALARM_CLASS_GREEN 1   // only goal-reached alerts active
#define ALARM_CLASS_RED   2   // any action/safety alarm active

#define ALARM_PROBE_HYST_F  5.0f   // probe re-arms this far below its alarm temp
#define GRILL_DROP_BAND_F   30.0f  // grill alarm: temp below target by this much
#define GRILL_INBAND_F      15.0f  // "at temp" band that arms drop detection
#define GRILL_DROP_DWELL_S  60     // sustained below-band time before drop fires
                                   // (lid-opens recover well inside this)

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

    // Alarms
    alarm_state_t probe_alarm[NUM_MEAT_PROBES];   // action alarms (Wrap etc.) - RED, one-shot
    alarm_state_t goal_alarm[NUM_MEAT_PROBES];    // goal reached (target_temp) - GREEN, one-shot
    alarm_state_t grill_alarm;      // grill temp-drop alarm - RED, re-arming
    alarm_state_t sensor_alarm;     // grill RTD fault forced a shutdown - RED
    alarm_state_t pellet_alarm;     // fuel starvation suspected (auger pegged, temp diving) - RED
    // Profile engine notices: gates ("WRAP NOW", RED, ack = continue) and
    // completion ("BRISKET_BOSS COMPLETE", GREEN). Text/color owned by the
    // cookprog engine; ack flows through the normal alarm ack.
    alarm_state_t prog_alarm;
    bool prog_alarm_green;
    char prog_alarm_text[48];
    bool grill_reached_band;        // grill got within GRILL_INBAND_F of target this cook

    // Cook start wall-clock (epoch seconds; 0 = unknown/not cooking).
    // Unlike cook_start_time (monotonic, for ET math) this survives
    // reboots via NVS and is for display ("Start: 7/17 11:19 AM").
    int64_t cook_start_wall;

    // Graph ring (chronology: oldest at graph_index when full, else 0)
    float graph_grill[GRAPH_HISTORY_SIZE];
    float graph_probe[NUM_MEAT_PROBES][GRAPH_HISTORY_SIZE];
    int16_t graph_target[GRAPH_HISTORY_SIZE];
    int graph_count;
    int graph_index;
    uint32_t last_graph_time;

    // WiFi
    bool wifi_connected;
    bool wifi_ap_active;    // SoftAP setup mode ("PelletPirate-Setup") is up
    int wifi_rssi;
    char wifi_ip[16];       // dotted-quad string, empty when disconnected

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
 * Call this every TEMP_HISTORY_INTERVAL_SEC. Caller must hold the lock.
 */
void grill_state_record_history(void);

/**
 * Record grill/target/probe temps into the graph ring (self-throttled to
 * GRAPH_INTERVAL_SEC). Caller must hold the lock.
 */
void grill_state_graph_record(void);

/**
 * Mark cook start/end (mode leaving/entering OFF). Resets per-cook probe
 * baselines and ET. Caller must hold the lock.
 */
void grill_state_cook_started(void);
void grill_state_cook_ended(void);

/**
 * Save current probe configs and grill target to NVS flash.
 * Call this when the user presses Save on the probe or target screens.
 * Returns the first error hit — a partial save never reports ESP_OK.
 */
esp_err_t grill_state_save_to_nvs(void);

/**
 * Load probe configs and grill target from NVS flash.
 * Called automatically by grill_state_init(). If no saved data exists,
 * defaults are used.
 */
void grill_state_load_from_nvs(void);

/**
 * Persist / recall the running mode+target for power-loss resume.
 * The actuator writes on every mode transition and mid-cook target change;
 * main.c reads once at boot to decide whether to resume a cook.
 * load returns false if nothing was ever saved.
 */
void grill_state_persist_run(grill_mode_t mode, int target);
bool grill_state_load_run(grill_mode_t *mode, int *target, int64_t *cook_start_wall);

/**
 * Get estimated minutes remaining for a probe based on rolling average.
 * Returns -1 if not enough data, -2 if the probe is STALLED (climbing
 * slower than ~3°F/hour — a linear estimate would be absurd).
 */
int grill_state_get_est_minutes(int probe_idx);

/**
 * Soonest upcoming goal across enabled probes (minutes), or -1 if none
 * estimable. probe_idx (optional) receives which probe. Caller must hold
 * the state lock.
 */
int grill_state_next_goal_minutes(int *probe_idx);

/**
 * Which panel jack (0-4 = P1-P5) carries the GRILL RTD. Stored in NVS;
 * change only while the grill is Off — the grill channel drives PID,
 * igniter inhibit and the fault shutdown.
 */
int grill_state_get_grill_jack(void);
bool grill_state_set_grill_jack(int jack);   // false if refused (not Off)

/**
 * Get elapsed cook time in minutes.
 * Returns 0 if not cooking.
 */
int grill_state_get_elapsed_minutes(void);

/**
 * Evaluate all alarm conditions against current temps/mode.
 * Call after each temperature refresh. Takes the state lock itself.
 */
void grill_state_alarms_update(void);

/**
 * True if any alarm is ACTIVE (fired and unacknowledged).
 */
bool grill_state_alarm_active(void);

/**
 * ALARM_CLASS_RED if any action/safety alarm is active, ALARM_CLASS_GREEN
 * if only goal-reached alerts are active, else ALARM_CLASS_NONE.
 * Drives banner color on both UIs (green = good news).
 */
int grill_state_alarm_class(void);

/**
 * Write a human-readable description of the highest-priority active alarm
 * into buf. Returns false (buf untouched) if no alarm is active.
 */
bool grill_state_alarm_text(char *buf, int len);

/**
 * Acknowledge all active alarms. Each re-arms automatically once its
 * condition clears (probe cools below threshold, grill returns to band).
 */
void grill_state_alarm_ack(void);

#endif
