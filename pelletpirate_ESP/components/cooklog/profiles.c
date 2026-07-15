#include "profiles.h"
#include "cooklog.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "grill_state.h"

static const char *TAG = "profiles";

#define BASE "/lfs"

static uint32_t s_rev = 0;

uint32_t profiles_revision(void)
{
    return s_rev;
}

// Build "prof_YYYYMMDD-HHMM_<Meat>.ppc" — date prefix keeps name-sort
// chronological; meat name spaces become dashes for a clean filename.
static void make_fname(char *buf, int len)
{
    char meat[16] = "Cook";
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        if (gs->probes[i].enabled && gs->probes[i].food_type[0]) {
            strlcpy(meat, gs->probes[i].food_type, sizeof(meat));
            break;
        }
    }
    grill_state_unlock();
    for (char *c = meat; *c; c++) {
        if (*c == ' ' || *c == '(' || *c == ')' || *c == '/') *c = '-';
    }

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 >= 2020) {
        char ts[20];
        strftime(ts, sizeof(ts), "%Y%m%d-%H%M", &tm);
        snprintf(buf, len, "prof_%.13s_%.12s.ppc", ts, meat);
    } else {
        snprintf(buf, len, "prof_boot%lu_%.8s.ppc",
                 (unsigned long)(esp_timer_get_time() / 1000000LL), meat);
    }
}

// "prof_20260714-2245_Brisket.ppc" -> "07/14 22:45 Brisket"
static void make_display(const char *fname, char *out, int len)
{
    unsigned y, mo, d, h, mi;
    char meat[20] = "";
    if (sscanf(fname, "prof_%4u%2u%2u-%2u%2u_%19[^.]", &y, &mo, &d, &h, &mi, meat) == 6) {
        snprintf(out, len, "%02u/%02u %02u:%02u %s", mo, d, h, mi, meat);
    } else {
        strlcpy(out, fname, len);
    }
}

int profiles_list(profile_info_t *out, int max)
{
    if (!cooklog_fs_ready()) return 0;
    int count = 0;
    DIR *dir = opendir(BASE);
    if (!dir) return 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL && count < max) {
        if (strncmp(de->d_name, "prof_", 5) != 0) continue;
        strlcpy(out[count].fname, de->d_name, PROFILE_NAME_MAX);
        make_display(de->d_name, out[count].display, PROFILE_NAME_MAX);
        count++;
    }
    closedir(dir);

    // Newest first: date-prefixed names sort descending
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(out[j].fname, out[i].fname) > 0) {
                profile_info_t tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return count;
}

bool profiles_save_current(char *name_out, int len)
{
    if (!cooklog_fs_ready()) return false;

    char fname[PROFILE_NAME_MAX];
    make_fname(fname, sizeof(fname));
    char path[64];
    snprintf(path, sizeof(path), BASE "/%s", fname);

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "save failed: %s", path);
        return false;
    }

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    fprintf(f, "tgt=%d\n", gs->grill_target);
    for (int i = 0; i < NUM_MEAT_PROBES; i++) {
        probe_state_t *p = &gs->probes[i];
        fprintf(f, "p%d=%d|%d|%s|%s\n", i + 1,
                p->enabled ? (int)p->target_temp : 0,
                (int)p->alarm_temp, p->food_type, p->alarm_type);
    }
    grill_state_unlock();
    fclose(f);

    if (name_out) make_display(fname, name_out, len);
    s_rev++;
    ESP_LOGI(TAG, "profile saved: %s", fname);
    return true;
}

bool profiles_delete(const char *fname)
{
    if (!cooklog_fs_ready()) return false;
    if (strncmp(fname, "prof_", 5) != 0 || strchr(fname, '/')) return false;

    char path[64];
    snprintf(path, sizeof(path), BASE "/%s", fname);
    if (unlink(path) != 0) {
        ESP_LOGE(TAG, "delete failed: %s", path);
        return false;
    }
    s_rev++;
    ESP_LOGI(TAG, "profile deleted: %s", fname);
    return true;
}

bool profiles_load(const char *fname)
{
    if (!cooklog_fs_ready()) return false;
    if (strncmp(fname, "prof_", 5) != 0 || strchr(fname, '/')) return false;

    char path[64];
    snprintf(path, sizeof(path), BASE "/%s", fname);
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "load failed: %s", path);
        return false;
    }

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        int v;
        if (sscanf(line, "tgt=%d", &v) == 1) {
            if (v >= TARGET_TEMP_MIN && v <= TARGET_TEMP_MAX) gs->grill_target = v;
            continue;
        }
        int idx, tg, am;
        char rest[64] = "";
        if (sscanf(line, "p%d=%d|%d|%63[^\n]", &idx, &tg, &am, rest) >= 3 &&
            idx >= 1 && idx <= NUM_MEAT_PROBES) {
            probe_state_t *p = &gs->probes[idx - 1];
            // rest = "food|alarmtype" (either may be empty)
            char *bar = strchr(rest, '|');
            if (bar) {
                *bar = '\0';
                strlcpy(p->alarm_type, bar + 1, sizeof(p->alarm_type));
            } else {
                p->alarm_type[0] = '\0';
            }
            strlcpy(p->food_type, rest, sizeof(p->food_type));
            p->target_temp = (float)tg;
            p->alarm_temp = (float)am;
            p->enabled = (tg > 0);
            if (!p->enabled) {
                p->food_type[0] = '\0';
                p->alarm_type[0] = '\0';
                p->alarm_temp = 0;
            }
        }
    }
    grill_state_save_to_nvs();
    grill_state_unlock();
    fclose(f);

    ESP_LOGI(TAG, "profile loaded: %s", fname);
    return true;
}
