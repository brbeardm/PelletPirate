#include "cooklog.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_littlefs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "grill_state.h"

static const char *TAG = "cooklog";

#define LOG_BASE_PATH   "/lfs"
#define NVS_NAMESPACE   "pelletpirate"
#define NVS_KEY_IV      "log_iv"
#define NVS_KEY_ACTIVE  "log_actv"   // filename of the open cook log (for
                                     // power-loss continuation)
// Sized for the OTA partition table's ~896KB storage partition (was 300KB
// against the old 2.4MB one) — keeps ~6-7 long cooks before rotation.
#define ROTATE_MIN_FREE (150 * 1024)
#define PENDING_MAX     8
#define ROW_MAX         160

static FILE *s_file = NULL;
static char s_fname[48];
static int s_interval = 30;             // 0 = off
static bool s_mounted = false;
static SemaphoreHandle_t s_mutex;

// Pre-cook events buffered until the file opens
static char s_pending[PENDING_MAX][ROW_MAX];
static int s_pending_count = 0;

// Power-loss continuation: set by cooklog_mark_resume() (main.c resume
// path) before the resumed mode goes active — the next file open appends
// to the interrupted cook's CSV instead of starting a new one.
static bool s_resume_pending = false;

static void nvs_set_active(const char *fname)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    if (fname) nvs_set_str(h, NVS_KEY_ACTIVE, fname);
    else nvs_erase_key(h, NVS_KEY_ACTIVE);
    nvs_commit(h);
    nvs_close(h);
}

void cooklog_mark_resume(void)
{
    s_resume_pending = true;
}

// Suppress the task's auto MODE event briefly after an attributed one
static int64_t s_mode_evt_suppress_until = 0;

// fflush alone only reaches the VFS layer; LittleFS commits nothing durable
// until fsync. Without this every row is lost on power-cut (the 0-byte /
// missing cook files from the first real cook, 2026-07-16/17).
// Call with s_mutex held.
static void flush_and_sync(void)
{
    if (!s_file) return;
    if (fflush(s_file) != 0 || fsync(fileno(s_file)) != 0) {
        ESP_LOGE(TAG, "log write/sync FAILED for %s (errno %d) — data at risk",
                 s_fname, errno);
    }
}

static void fmt_time(char *buf, int len)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 >= 2020) {
        // Full ISO date-time so spreadsheets parse it directly and cooks
        // crossing midnight stay unambiguous
        strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm);
    } else {
        // No wall clock (SNTP not synced) — seconds since boot
        snprintf(buf, len, "+%lld", esp_timer_get_time() / 1000000LL);
    }
}

// Build one CSV row with the full state snapshot. Takes the state lock.
static void build_row(char *buf, int len, char ev, const char *note)
{
    char ts[24];
    fmt_time(ts, sizeof(ts));

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    char pcols[64] = "";
    int n = 0;
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (gs->probes[i].enabled && gs->probes[i].current_temp > 0) {
            n += snprintf(pcols + n, sizeof(pcols) - n, "%.1f,", gs->probes[i].current_temp);
        } else {
            n += snprintf(pcols + n, sizeof(pcols) - n, ",");
        }
    }
    snprintf(buf, len, "%s,%c,%s,%.1f,%d,%.2f,%d,%d,%d,%s%d,%s\n",
             ts, ev, grill_mode_name(gs->mode), gs->grill_temp, gs->grill_target,
             gs->pid_u, gs->fan_on ? 1 : 0, gs->auger_on ? 1 : 0,
             gs->igniter_on ? 1 : 0, pcols, gs->wifi_rssi, note ? note : "");
    grill_state_unlock();
}

// Delete oldest cook files until enough space is free. Called before open.
static void rotate_if_needed(void)
{
    size_t total = 0, used = 0;
    if (esp_littlefs_info("storage", &total, &used) != ESP_OK) return;

    while (total - used < ROTATE_MIN_FREE) {
        DIR *dir = opendir(LOG_BASE_PATH);
        if (!dir) return;
        char oldest[48] = "";
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            if (strncmp(de->d_name, "cook_", 5) != 0) continue;
            // Never rotate out the file an interrupted cook may resume into
            // (s_fname keeps the full path; compare basenames)
            const char *active = strrchr(s_fname, '/');
            if (active && strcmp(de->d_name, active + 1) == 0) continue;
            if (oldest[0] == '\0' || strcmp(de->d_name, oldest) < 0) {
                strlcpy(oldest, de->d_name, sizeof(oldest));
            }
        }
        closedir(dir);
        if (oldest[0] == '\0') return;  // nothing left to delete

        char path[64];
        snprintf(path, sizeof(path), LOG_BASE_PATH "/%s", oldest);
        ESP_LOGW(TAG, "rotating out %s", oldest);
        unlink(path);
        if (esp_littlefs_info("storage", &total, &used) != ESP_OK) return;
    }
}

static void open_cook_file(void)
{
    // Power-loss continuation: reopen the interrupted cook's file in
    // append mode so one cook stays one CSV. Every row was fsync'd, so
    // the file holds everything up to the outage.
    if (s_resume_pending) {
        s_resume_pending = false;
        char prev[48] = "";
        size_t len = sizeof(prev);
        nvs_handle_t h;
        if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
            nvs_get_str(h, NVS_KEY_ACTIVE, prev, &len);
            nvs_close(h);
        }
        if (prev[0] != '\0') {
            FILE *f = fopen(prev, "a");
            if (f) {
                s_file = f;
                strlcpy(s_fname, prev, sizeof(s_fname));
                char ts[24];
                fmt_time(ts, sizeof(ts));
                fprintf(s_file, "%s,E,Off,,,,,,,,,,,,POWER LOSS - resumed\n", ts);
                flush_and_sync();
                ESP_LOGW(TAG, "cook log CONTINUED after power loss: %s", s_fname);
                return;
            }
            ESP_LOGW(TAG, "resume log %s missing — starting a new file", prev);
        }
    }

    rotate_if_needed();

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 >= 2020) {
        snprintf(s_fname, sizeof(s_fname), LOG_BASE_PATH "/cook_%04d%02d%02d_%02d%02d.csv",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    } else {
        snprintf(s_fname, sizeof(s_fname), LOG_BASE_PATH "/cook_boot_%lld.csv",
                 esp_timer_get_time() / 1000000LL);
    }

    s_file = fopen(s_fname, "w");
    if (!s_file) {
        ESP_LOGE(TAG, "failed to open %s", s_fname);
        return;
    }
    fprintf(s_file, "time,ev,mode,grill,tgt,u,fan,aug,ign,p1,p2,p3,p4,rssi,note\n");

    // Flush pre-cook setup events (target/probe changes made while Off)
    for (int i = 0; i < s_pending_count; i++) {
        fputs(s_pending[i], s_file);
    }
    s_pending_count = 0;
    flush_and_sync();
    nvs_set_active(s_fname);
    ESP_LOGI(TAG, "cook log started: %s", s_fname);
}

static void close_cook_file(void)
{
    if (!s_file) return;
    fclose(s_file);
    s_file = NULL;
    nvs_set_active(NULL);   // clean end — nothing to continue
    ESP_LOGI(TAG, "cook log closed: %s", s_fname);
}

void cooklog_event(const char *source, const char *fmt, ...)
{
    if (!s_mounted || s_interval == 0) return;

    char note[96];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(note, sizeof(note) - 8, fmt, ap);
    va_end(ap);
    snprintf(note + n, sizeof(note) - n, " (%s)", source);

    if (strncmp(note, "MODE", 4) == 0 && strcmp(source, "auto") != 0) {
        s_mode_evt_suppress_until = esp_timer_get_time() + 2500000;
    }

    char row[ROW_MAX];
    build_row(row, sizeof(row), 'E', note);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_file) {
        fputs(row, s_file);
        flush_and_sync();
    } else if (s_pending_count < PENDING_MAX) {
        strlcpy(s_pending[s_pending_count++], row, ROW_MAX);
    } else {
        // Ring: drop the oldest pre-cook event
        memmove(s_pending[0], s_pending[1], (PENDING_MAX - 1) * ROW_MAX);
        strlcpy(s_pending[PENDING_MAX - 1], row, ROW_MAX);
    }
    xSemaphoreGive(s_mutex);
}

static void cooklog_task(void *arg)
{
    grill_mode_t prev_mode = GRILL_MODE_OFF;
    bool prev_ign = false, prev_fan = false, prev_alarm = false;
    alarm_state_t prev_goal[NUM_MEAT_PROBES] = { ALARM_IDLE };
    int64_t last_sample = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!s_mounted) continue;

        alarm_state_t goal[NUM_MEAT_PROBES];
        float gtemp[NUM_MEAT_PROBES];
        char gfood[NUM_MEAT_PROBES][16];
        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        grill_mode_t mode = gs->mode;
        bool ign = gs->igniter_on;
        bool fan = gs->fan_on;
        for (int i = 0; i < NUM_MEAT_PROBES; i++) {
            goal[i] = gs->goal_alarm[i];
            gtemp[i] = gs->probes[i].current_temp;
            strlcpy(gfood[i], gs->probes[i].food_type, sizeof(gfood[i]));
        }
        grill_state_unlock();
        bool alarm = grill_state_alarm_active();
        bool active = (mode != GRILL_MODE_OFF);
        int64_t now = esp_timer_get_time();

        // Cook file lifecycle
        if (s_interval > 0 && active && !s_file) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            open_cook_file();
            xSemaphoreGive(s_mutex);
            last_sample = 0;
        }

        // Auto-detected events (suppressed if the source already logged it)
        if (mode != prev_mode && s_file) {
            if (now > s_mode_evt_suppress_until) {
                cooklog_event("auto", "MODE %s>%s",
                              grill_mode_name(prev_mode), grill_mode_name(mode));
            }
        }
        if (ign != prev_ign && s_file) {
            cooklog_event("auto", "IGNITER %s", ign ? "ON" : "OFF");
        }
        // Fan events only outside PID modes (burst pulsing would be noise;
        // the sample rows carry fan state during COOK/KEEP_WARM)
        if (fan != prev_fan && s_file &&
            mode != GRILL_MODE_COOK && mode != GRILL_MODE_KEEP_WARM) {
            cooklog_event("auto", "FAN %s", fan ? "ON" : "OFF");
        }
        if (alarm && !prev_alarm && s_file) {
            char txt[64] = "";
            grill_state_alarm_text(txt, sizeof(txt));
            cooklog_event("auto", "ALARM: %s", txt);
        }
        // Goal crossings get their own attributed rows — the aggregate
        // ALARM transition above can be masked by an alarm already active
        for (int i = 0; i < NUM_MEAT_PROBES; i++) {
            if (goal[i] == ALARM_ACTIVE && prev_goal[i] != ALARM_ACTIVE && s_file) {
                cooklog_event("auto", "GOAL P%d %s reached %.0fF", i + 1,
                              gfood[i][0] ? gfood[i] : "probe", gtemp[i]);
            }
            prev_goal[i] = goal[i];
        }
        prev_ign = ign;
        prev_fan = fan;
        prev_alarm = alarm;

        // Periodic sample
        if (s_file && active && s_interval > 0 &&
            (now - last_sample) >= (int64_t)s_interval * 1000000LL) {
            last_sample = now;
            char row[ROW_MAX];
            build_row(row, sizeof(row), 'S', "");
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            if (s_file) { fputs(row, s_file); flush_and_sync(); }
            xSemaphoreGive(s_mutex);
        }

        // Close at end of cook (after the final mode event was written)
        if (!active && s_file && prev_mode != GRILL_MODE_OFF) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            close_cook_file();
            xSemaphoreGive(s_mutex);
        }
        prev_mode = mode;
    }
}

void cooklog_set_interval(int seconds)
{
    if (seconds != 0 && seconds != 10 && seconds != 30) return;
    s_interval = seconds;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, NVS_KEY_IV, seconds);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "log interval set to %ds", seconds);
}

int cooklog_get_interval(void)
{
    return s_interval;
}

bool cooklog_fs_ready(void)
{
    return s_mounted;
}

void cooklog_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LOG_BASE_PATH,
        .partition_label = "storage",
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s — logging disabled", esp_err_to_name(err));
        return;
    }
    s_mounted = true;

    nvs_handle_t h;
    int32_t iv = 30;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, NVS_KEY_IV, &iv);
        nvs_close(h);
    }
    s_interval = (int)iv;

    size_t total = 0, used = 0;
    esp_littlefs_info("storage", &total, &used);
    ESP_LOGI(TAG, "LittleFS mounted: %u KB used of %u KB, interval %ds",
             (unsigned)(used / 1024), (unsigned)(total / 1024), s_interval);

    xTaskCreate(cooklog_task, "cooklog", 4096, NULL, 3, NULL);
}
