#include "grill_state.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "grill_state";
#define NVS_NAMESPACE "pelletpirate"

static grill_state_t s_state;
static SemaphoreHandle_t s_mutex;

static const char *mode_names[] = {
    [GRILL_MODE_OFF]         = "Off",
    [GRILL_MODE_START]       = "Ignite",
    [GRILL_MODE_SMOKE]       = "Smoke",
    [GRILL_MODE_SUPER_SMOKE] = "Super Smoke",
    [GRILL_MODE_COOK]        = "Cook",
    [GRILL_MODE_KEEP_WARM]   = "Keep Warm",
    [GRILL_MODE_SHUTDOWN]    = "Shutdown",
    [GRILL_MODE_REIGNITE]    = "Re-Ignite",
};

// Get current time in seconds (monotonic)
static uint32_t now_sec(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

void grill_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Defaults
    s_state.mode = GRILL_MODE_OFF;
    s_state.grill_temp = 87.0f;
    s_state.grill_target = 225;

    // Try to load saved settings from NVS
    grill_state_load_from_nvs();

    // cook_start_time stays 0 until the actuator sees the mode leave OFF
    // (grill_state_cook_started); ET/EST and history are per-cook.
    s_state.last_history_time = now_sec();
    ESP_LOGI(TAG, "Grill state initialized, target=%d", s_state.grill_target);
}

grill_state_t *grill_state_get(void)
{
    return &s_state;
}

void grill_state_lock(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
}

void grill_state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

const char *grill_mode_name(grill_mode_t mode)
{
    if (mode >= 0 && mode < GRILL_MODE_COUNT) {
        return mode_names[mode];
    }
    return "Unknown";
}

void grill_state_cook_started(void)
{
    // Caller must hold the lock. Marks the cook start and resets per-cook
    // probe baselines so ET/EST reflect this cook, not the last one.
    s_state.cook_start_time = now_sec();
    s_state.last_history_time = now_sec();
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        s_state.probes[i].start_temp = s_state.probes[i].current_temp;
        s_state.probes[i].history_count = 0;
        s_state.probes[i].history_index = 0;
    }
}

void grill_state_cook_ended(void)
{
    // Caller must hold the lock.
    s_state.cook_start_time = 0;
}

void grill_state_record_history(void)
{
    // Caller must hold the lock (ui_dashboard calls this inside its
    // locked update pass).
    uint32_t t = now_sec();
    if (s_state.cook_start_time == 0) return;
    if (t - s_state.last_history_time < TEMP_HISTORY_INTERVAL_SEC) return;

    s_state.last_history_time = t;

    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (!s_state.probes[i].enabled) continue;

        s_state.probes[i].temp_history[s_state.probes[i].history_index] =
            s_state.probes[i].current_temp;
        s_state.probes[i].history_index =
            (s_state.probes[i].history_index + 1) % TEMP_HISTORY_SIZE;
        if (s_state.probes[i].history_count < TEMP_HISTORY_SIZE)
            s_state.probes[i].history_count++;
    }
}

void grill_state_graph_record(void)
{
    // Caller must hold the lock. Runs continuously (preheat is data too).
    uint32_t t = now_sec();
    if (s_state.graph_count > 0 &&
        t - s_state.last_graph_time < GRAPH_INTERVAL_SEC) return;
    s_state.last_graph_time = t;

    int idx = s_state.graph_index;
    s_state.graph_grill[idx] = s_state.grill_temp;
    // Target is recorded even when Off — its trend over the cook shows how
    // the pitmaster managed the fire, and it must always be on the chart.
    s_state.graph_target[idx] = (int16_t)s_state.grill_target;
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        s_state.graph_probe[i][idx] =
            s_state.probes[i].enabled ? s_state.probes[i].current_temp : 0.0f;
    }
    s_state.graph_index = (idx + 1) % GRAPH_HISTORY_SIZE;
    if (s_state.graph_count < GRAPH_HISTORY_SIZE) s_state.graph_count++;
}

int grill_state_get_est_minutes(int probe_idx)
{
    if (probe_idx < 0 || probe_idx >= NUM_MEAT_PROBES) return -1;

    probe_state_t *p = &s_state.probes[probe_idx];
    if (!p->enabled || p->target_temp <= 0) return -1;
    if (p->history_count < 3) return -1;  // need at least 15 minutes of data
    if (p->current_temp >= p->target_temp) return 0;  // already done

    // Rolling average: calculate rate from oldest to newest sample in history
    // Oldest sample is at (history_index) if buffer is full, or at 0 if not
    int oldest_idx;
    if (p->history_count >= TEMP_HISTORY_SIZE) {
        oldest_idx = p->history_index;  // circular: next write pos = oldest
    } else {
        oldest_idx = 0;
    }
    int newest_idx = (p->history_index - 1 + TEMP_HISTORY_SIZE) % TEMP_HISTORY_SIZE;

    float oldest_temp = p->temp_history[oldest_idx];
    float newest_temp = p->temp_history[newest_idx];
    float temp_change = newest_temp - oldest_temp;

    // Time span of the history window
    float minutes_span = (float)(p->history_count - 1) * (TEMP_HISTORY_INTERVAL_SEC / 60.0f);
    if (minutes_span <= 0) return -1;

    // Rate in degrees per minute
    float rate = temp_change / minutes_span;

    // If rate is zero or negative (stall or cooling), can't estimate
    if (rate <= 0.05f) return -1;  // less than 3°F/hour = stalled

    // Remaining degrees
    float remaining = p->target_temp - p->current_temp;

    // Estimated minutes
    int est = (int)(remaining / rate);
    if (est < 0) est = 0;
    if (est > 5999) est = 5999;  // cap at 99:59

    return est;
}

int grill_state_get_elapsed_minutes(void)
{
    if (s_state.cook_start_time == 0) return 0;
    uint32_t elapsed = now_sec() - s_state.cook_start_time;
    return (int)(elapsed / 60);
}

// --- Alarm engine ---

static bool mode_is_cooking(grill_mode_t m)
{
    // Modes where the grill is expected to hold temperature — the only
    // modes where a temp-drop alarm is meaningful. START/REIGNITE are
    // still climbing; OFF/SHUTDOWN are supposed to fall.
    return m == GRILL_MODE_SMOKE || m == GRILL_MODE_SUPER_SMOKE ||
           m == GRILL_MODE_COOK || m == GRILL_MODE_KEEP_WARM;
}

void grill_state_alarms_update(void)
{
    grill_state_lock();

    // Probe alarms: fire at/above alarm_temp; re-arm after acknowledge
    // once the probe cools ALARM_PROBE_HYST_F below the threshold.
    // current_temp == 0 means fault/unplugged — never fire on that.
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        probe_state_t *p = &s_state.probes[i];
        bool armed = p->enabled && p->alarm_temp > 0 && p->current_temp > 0;

        if (!armed) {
            s_state.probe_alarm[i] = ALARM_IDLE;
            continue;
        }
        switch (s_state.probe_alarm[i]) {
        case ALARM_IDLE:
            if (p->current_temp >= p->alarm_temp) {
                s_state.probe_alarm[i] = ALARM_ACTIVE;
                ESP_LOGW(TAG, "ALARM: probe %d reached %.0fF (alarm %.0fF, type '%s')",
                         i + 1, p->current_temp, p->alarm_temp, p->alarm_type);
            }
            break;
        case ALARM_ACKED:
            if (p->current_temp < p->alarm_temp - ALARM_PROBE_HYST_F) {
                s_state.probe_alarm[i] = ALARM_IDLE;  // re-armed
            }
            break;
        default:
            break;
        }
    }

    // Grill temp-drop alarm: only meaningful in holding modes, and only
    // after the grill has actually reached the target band once (so it
    // never fires during warm-up). Mirrors the Photon tempMonitorOn logic.
    if (!mode_is_cooking(s_state.mode)) {
        s_state.grill_alarm = ALARM_IDLE;
        s_state.grill_reached_band = false;
    } else {
        float t = s_state.grill_temp;
        float target = (float)s_state.grill_target;

        if (t > 0 && t >= target - GRILL_INBAND_F) {
            s_state.grill_reached_band = true;
            if (s_state.grill_alarm == ALARM_ACKED) {
                s_state.grill_alarm = ALARM_IDLE;  // recovered — re-arm
            }
        }
        if (s_state.grill_reached_band && t > 0 &&
            t < target - GRILL_DROP_BAND_F &&
            s_state.grill_alarm == ALARM_IDLE) {
            s_state.grill_alarm = ALARM_ACTIVE;
            ESP_LOGE(TAG, "ALARM: grill temp dropped to %.0fF (target %d) — fire out?",
                     t, s_state.grill_target);
        }
    }

    grill_state_unlock();
}

bool grill_state_alarm_active(void)
{
    bool active = false;
    grill_state_lock();
    if (s_state.grill_alarm == ALARM_ACTIVE) active = true;
    if (s_state.sensor_alarm == ALARM_ACTIVE) active = true;
    for (int i = 0; i < NUM_MEAT_PROBES && !active; i++) {
        if (s_state.probe_alarm[i] == ALARM_ACTIVE) active = true;
    }
    grill_state_unlock();
    return active;
}

bool grill_state_alarm_text(char *buf, int len)
{
    bool found = false;
    grill_state_lock();

    // Sensor fault outranks everything — the controller is flying blind
    // and has forced a shutdown burn-off
    if (s_state.sensor_alarm == ALARM_ACTIVE) {
        snprintf(buf, len, "GRILL SENSOR FAULT - SHUTTING DOWN");
        found = true;
    } else
    // Grill drop outranks probe alarms — it means the cook is at risk
    if (s_state.grill_alarm == ALARM_ACTIVE) {
        snprintf(buf, len, "GRILL TEMP DROP: %.0f\xC2\xB0""F (target %d\xC2\xB0""F)",
                 s_state.grill_temp, s_state.grill_target);
        found = true;
    } else {
        for (int i = 0; i < NUM_MEAT_PROBES; i++) {
            if (s_state.probe_alarm[i] == ALARM_ACTIVE) {
                probe_state_t *p = &s_state.probes[i];
                if (p->alarm_type[0] != '\0') {
                    snprintf(buf, len, "PROBE %d: %.0f\xC2\xB0""F - %s!",
                             i + 1, p->current_temp, p->alarm_type);
                } else {
                    snprintf(buf, len, "PROBE %d ALARM: %.0f\xC2\xB0""F",
                             i + 1, p->current_temp);
                }
                found = true;
                break;
            }
        }
    }

    grill_state_unlock();
    return found;
}

void grill_state_alarm_ack(void)
{
    grill_state_lock();
    if (s_state.grill_alarm == ALARM_ACTIVE) s_state.grill_alarm = ALARM_ACKED;
    if (s_state.sensor_alarm == ALARM_ACTIVE) s_state.sensor_alarm = ALARM_ACKED;
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (s_state.probe_alarm[i] == ALARM_ACTIVE) {
            s_state.probe_alarm[i] = ALARM_ACKED;
        }
    }
    grill_state_unlock();
    ESP_LOGI(TAG, "Alarms acknowledged");
}

esp_err_t grill_state_save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    // Accumulate the first failure — a partial save must not report success
    esp_err_t worst = ESP_OK;
#define NVS_CHECK(call) do { \
        esp_err_t e_ = (call); \
        if (e_ != ESP_OK && worst == ESP_OK) worst = e_; \
    } while (0)

    NVS_CHECK(nvs_set_i32(handle, "grill_target", s_state.grill_target));

    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        char key[16];
        probe_state_t *p = &s_state.probes[i];

        snprintf(key, sizeof(key), "p%d_en", i);
        NVS_CHECK(nvs_set_u8(handle, key, p->enabled ? 1 : 0));

        snprintf(key, sizeof(key), "p%d_tgt", i);
        NVS_CHECK(nvs_set_i32(handle, key, (int32_t)p->target_temp));

        snprintf(key, sizeof(key), "p%d_alm", i);
        NVS_CHECK(nvs_set_i32(handle, key, (int32_t)p->alarm_temp));

        snprintf(key, sizeof(key), "p%d_food", i);
        NVS_CHECK(nvs_set_str(handle, key, p->food_type));

        snprintf(key, sizeof(key), "p%d_atyp", i);
        NVS_CHECK(nvs_set_str(handle, key, p->alarm_type));
    }
#undef NVS_CHECK

    esp_err_t cerr = nvs_commit(handle);
    if (worst == ESP_OK) worst = cerr;
    nvs_close(handle);

    if (worst == ESP_OK) {
        ESP_LOGI(TAG, "Settings saved to NVS");
    } else {
        ESP_LOGE(TAG, "NVS save FAILED (%s) — settings will not survive reboot",
                 esp_err_to_name(worst));
    }
    return worst;
}

void grill_state_load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved settings found, using defaults");

        // First boot defaults
        s_state.grill_target = 225;
        for (int i = 0; i < NUM_MEAT_PROBES; i++) {
            s_state.probes[i].enabled = false;
            s_state.probes[i].target_temp = 0;
            s_state.probes[i].alarm_temp = 0;
            strncpy(s_state.probes[i].food_type, "", sizeof(s_state.probes[i].food_type));
            strncpy(s_state.probes[i].alarm_type, "", sizeof(s_state.probes[i].alarm_type));
        }
        return;
    }

    // Load grill target
    int32_t target = 225;
    nvs_get_i32(handle, "grill_target", &target);
    s_state.grill_target = (int)target;

    // Load each probe
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        char key[16];
        probe_state_t *p = &s_state.probes[i];

        snprintf(key, sizeof(key), "p%d_en", i);
        uint8_t en = 0;
        nvs_get_u8(handle, key, &en);
        p->enabled = (en != 0);

        snprintf(key, sizeof(key), "p%d_tgt", i);
        int32_t tgt = 0;
        nvs_get_i32(handle, key, &tgt);
        p->target_temp = (float)tgt;

        snprintf(key, sizeof(key), "p%d_alm", i);
        int32_t alm = 0;
        nvs_get_i32(handle, key, &alm);
        p->alarm_temp = (float)alm;

        snprintf(key, sizeof(key), "p%d_food", i);
        size_t len = sizeof(p->food_type);
        if (nvs_get_str(handle, key, p->food_type, &len) != ESP_OK)
            p->food_type[0] = '\0';

        snprintf(key, sizeof(key), "p%d_atyp", i);
        len = sizeof(p->alarm_type);
        if (nvs_get_str(handle, key, p->alarm_type, &len) != ESP_OK)
            p->alarm_type[0] = '\0';
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Settings loaded from NVS, target=%d", s_state.grill_target);
}

// --- Power-loss cook resume ---
// The actuator persists the running mode+target on every transition (and on
// mid-cook target changes); after an unexpected reboot main.c consults this
// to resume a cook in progress. A clean OFF overwrites it, so nothing stale
// survives a normal end-of-cook.

void grill_state_persist_run(grill_mode_t mode, int target)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "run_mode", (uint8_t)mode);
    nvs_set_i32(h, "run_tgt", (int32_t)target);
    esp_err_t err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "persist run mode failed: %s", esp_err_to_name(err));
    }
}

bool grill_state_load_run(grill_mode_t *mode, int *target)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t m = 0;
    int32_t t = 0;
    bool ok = (nvs_get_u8(h, "run_mode", &m) == ESP_OK) &&
              (nvs_get_i32(h, "run_tgt", &t) == ESP_OK);
    nvs_close(h);
    if (!ok || m >= GRILL_MODE_COUNT) return false;
    *mode = (grill_mode_t)m;
    *target = (int)t;
    return true;
}
