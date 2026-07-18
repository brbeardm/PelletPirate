// Cook profile engine — see cookprog.h for the model. Sequencer only:
// every pit change goes through grill_state exactly as a human would
// make it, so the actuator's safety layers apply unchanged.

#include "cookprog.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "grill_state.h"
#include "cooklog.h"

static const char *TAG = "cookprog";

#define PROG_DIR       "/lfs"
#define PROG_PREFIX    "prog_"
#define PROG_EXT       ".ppg"
#define NVS_NS         "pelletpirate"
#define PROG_FILE_MAX  1024

typedef enum { STEP_SUPERSMOKE, STEP_SMOKE, STEP_COOK, STEP_KEEPWARM, STEP_GATE } step_mode_t;
typedef enum { EXIT_TEMP, EXIT_TIME, EXIT_ACK, EXIT_END } step_exit_t;

typedef struct {
    step_mode_t mode;
    int target;             // pit target for cook/keepwarm; band center for smoke
    step_exit_t exit;
    int value;              // temp F or minutes
    char note[32];
} prog_step_t;

// --- Factory templates (read-only, ship with the firmware, item 17) ---
// Each exercises a distinct engine capability; copy to a user file to tune.
typedef struct { const char *name; const char *text; } factory_t;

static const factory_t s_factory[] = {
    { "Brisket_Boss",
      "ver=1\nname=Brisket_Boss\n"
      "step=supersmoke,0,temp,140,\n"
      "step=cook,225,temp,160,\n"
      "step=gate,0,ack,0,WRAP NOW\n"
      "step=cook,250,temp,203,\n"
      "step=keepwarm,165,end,0,BRISKET DONE\n" },
    { "PorkButt_Solo",
      "ver=1\nname=PorkButt_Solo\n"
      "step=supersmoke,0,temp,150,\n"
      "step=cook,250,temp,165,\n"
      "step=gate,0,ack,0,WRAP NOW\n"
      "step=cook,275,temp,203,\n"
      "step=keepwarm,165,end,0,PORK DONE\n" },
    { "Ribs_Champ",
      "ver=1\nname=Ribs_Champ\n"
      "step=smoke,180,time,180,\n"
      "step=gate,0,ack,0,WRAP + LIQUID\n"
      "step=cook,225,time,120,\n"
      "step=gate,0,ack,0,UNWRAP + SAUCE\n"
      "step=cook,250,time,60,\n"
      "step=keepwarm,165,end,0,RIBS DONE - check bend\n" },
    { "Chicken_Crispy",
      "ver=1\nname=Chicken_Crispy\n"
      "step=smoke,180,time,45,\n"
      "step=cook,375,temp,165,\n"
      // Deliberately NO KeepWarm hold: holding steams the skin soft.
      "step=cook,375,end,0,CHICKEN DONE - PULL NOW\n" },
};
#define FACTORY_COUNT (sizeof(s_factory) / sizeof(s_factory[0]))

// --- Engine state (all access under s_mutex) ---
static SemaphoreHandle_t s_mutex;
static bool s_running = false;
static bool s_paused = false;
static bool s_boot_hold = false;        // waiting out the power-loss resume window
static bool s_igniting = false;         // waiting for START->COOK promotion
static char s_name[COOKPROG_NAME_MAX];
static prog_step_t s_steps[COOKPROG_MAX_STEPS];
static int s_count = 0;
static int s_idx = 0;
static int s_driver = 0;                // meat probe 0-3
static double s_step_start = 0;         // engine time at step entry
static double s_fault_since = 0;        // driver probe reading 0.0
static grill_mode_t s_cmd_mode = GRILL_MODE_OFF;   // last mode WE commanded

static double now_s(void) { return (double)esp_timer_get_time() / 1e6; }

static const char *step_mode_tok(step_mode_t m)
{
    switch (m) {
    case STEP_SUPERSMOKE: return "supersmoke";
    case STEP_SMOKE:      return "smoke";
    case STEP_COOK:       return "cook";
    case STEP_KEEPWARM:   return "keepwarm";
    default:              return "gate";
    }
}

static grill_mode_t step_grill_mode(step_mode_t m)
{
    switch (m) {
    case STEP_SUPERSMOKE: return GRILL_MODE_SUPER_SMOKE;
    case STEP_SMOKE:      return GRILL_MODE_SMOKE;
    case STEP_KEEPWARM:   return GRILL_MODE_KEEP_WARM;
    default:              return GRILL_MODE_COOK;
    }
}

// --- Program file parsing ---

static bool parse_step(const char *line, prog_step_t *st)
{
    char mode[16] = "", exi[8] = "", note[32] = "";
    int target = 0, value = 0;
    // step=<mode>,<target>,<exit>,<value>,<note...>
    int n = sscanf(line, "%15[^,],%d,%7[^,],%d,%31[^\n\r]", mode, &target, exi, &value, note);
    if (n < 4) return false;
    if      (strcmp(mode, "supersmoke") == 0) st->mode = STEP_SUPERSMOKE;
    else if (strcmp(mode, "smoke") == 0)      st->mode = STEP_SMOKE;
    else if (strcmp(mode, "cook") == 0)       st->mode = STEP_COOK;
    else if (strcmp(mode, "keepwarm") == 0)   st->mode = STEP_KEEPWARM;
    else if (strcmp(mode, "gate") == 0)       st->mode = STEP_GATE;
    else return false;
    if      (strcmp(exi, "temp") == 0) st->exit = EXIT_TEMP;
    else if (strcmp(exi, "time") == 0) st->exit = EXIT_TIME;
    else if (strcmp(exi, "ack") == 0)  st->exit = EXIT_ACK;
    else if (strcmp(exi, "end") == 0)  st->exit = EXIT_END;
    else return false;
    if (target < 0 || target > TARGET_TEMP_MAX) return false;
    if (st->exit == EXIT_TEMP && (value < 50 || value > 400)) return false;
    if (st->exit == EXIT_TIME && (value < 1 || value > 24 * 60)) return false;
    st->target = target;
    st->value = value;
    strlcpy(st->note, note, sizeof(st->note));
    // Gates must wait on ack, and only gates may
    if ((st->mode == STEP_GATE) != (st->exit == EXIT_ACK)) return false;
    return true;
}

static int parse_program(const char *text, prog_step_t *steps, int max)
{
    int count = 0;
    const char *p = text;
    while (*p && count < max) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > 7 && strncmp(p, "step=", 5) == 0) {
            char line[128];
            int c = len - 5 < (int)sizeof(line) - 1 ? len - 5 : (int)sizeof(line) - 1;
            memcpy(line, p + 5, c);
            line[c] = '\0';
            if (parse_step(line, &steps[count])) {
                count++;
            } else {
                ESP_LOGW(TAG, "bad step line ignored: %.*s", len, p);
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    return count;
}

static bool name_ok(const char *name)
{
    if (!name[0] || strlen(name) >= COOKPROG_NAME_MAX) return false;
    for (const char *c = name; *c; c++) {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '-' || *c == '_')) return false;
    }
    return true;
}

bool cookprog_read(const char *name, char *buf, int len)
{
    if (!name_ok(name)) return false;
    for (int i = 0; i < (int)FACTORY_COUNT; i++) {
        if (strcmp(s_factory[i].name, name) == 0) {
            strlcpy(buf, s_factory[i].text, len);
            return true;
        }
    }
    char path[80];
    snprintf(path, sizeof(path), PROG_DIR "/" PROG_PREFIX "%s" PROG_EXT, name);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    int n = fread(buf, 1, len - 1, f);
    fclose(f);
    if (n < 0) n = 0;
    buf[n] = '\0';
    return true;
}

bool cookprog_write(const char *name, const char *content)
{
    if (!name_ok(name)) return false;
    for (int i = 0; i < (int)FACTORY_COUNT; i++) {
        if (strcmp(s_factory[i].name, name) == 0) return false;  // read-only
    }
    char path[80];
    snprintf(path, sizeof(path), PROG_DIR "/" PROG_PREFIX "%s" PROG_EXT, name);
    if (!content || !content[0]) {
        return unlink(path) == 0;
    }
    // Must parse to at least one step before it's worth saving
    prog_step_t tmp[COOKPROG_MAX_STEPS];
    if (parse_program(content, tmp, COOKPROG_MAX_STEPS) < 1) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fputs(content, f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    ESP_LOGI(TAG, "program saved: %s", name);
    return true;
}

int cookprog_list(char names[][COOKPROG_NAME_MAX], int max)
{
    int count = 0;
    for (int i = 0; i < (int)FACTORY_COUNT && count < max; i++) {
        snprintf(names[count++], COOKPROG_NAME_MAX, "*%s", s_factory[i].name);
    }
    DIR *dir = opendir(PROG_DIR);
    if (dir) {
        struct dirent *de;
        while ((de = readdir(dir)) != NULL && count < max) {
            size_t nl = strlen(de->d_name);
            if (strncmp(de->d_name, PROG_PREFIX, 5) != 0) continue;
            if (nl <= 5 + 4 || strcmp(de->d_name + nl - 4, PROG_EXT) != 0) continue;
            int base = (int)nl - 5 - 4;
            if (base >= COOKPROG_NAME_MAX) continue;
            memcpy(names[count], de->d_name + 5, base);
            names[count][base] = '\0';
            count++;
        }
        closedir(dir);
    }
    return count;
}

// --- Run-state persistence (power-loss resume rides the item-7 rails) ---

static void persist_run_state(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (s_running) {
        nvs_set_str(h, "prog_name", s_name);
        nvs_set_u8(h, "prog_idx", (uint8_t)s_idx);
        nvs_set_u8(h, "prog_drv", (uint8_t)s_driver);
    } else {
        nvs_erase_key(h, "prog_name");
        nvs_erase_key(h, "prog_idx");
        nvs_erase_key(h, "prog_drv");
    }
    nvs_commit(h);
    nvs_close(h);
}

// --- Engine internals (call with s_mutex held) ---

static void set_prog_alarm(const char *text, bool green)
{
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    gs->prog_alarm = ALARM_ACTIVE;
    gs->prog_alarm_green = green;
    strlcpy(gs->prog_alarm_text, text, sizeof(gs->prog_alarm_text));
    grill_state_unlock();
}

static void clear_prog_alarm(void)
{
    grill_state_lock();
    grill_state_get()->prog_alarm = ALARM_IDLE;
    grill_state_unlock();
}

static void command_pit(grill_mode_t mode, int target)
{
    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    gs->mode = mode;
    if (target > 0) gs->grill_target = target;
    grill_state_unlock();
    s_cmd_mode = mode;
}

static void enter_step(void)
{
    prog_step_t *st = &s_steps[s_idx];
    s_step_start = now_s();
    s_fault_since = 0;

    if (st->mode == STEP_GATE) {
        // Pit keeps doing whatever the previous step set; the gate is a
        // human checkpoint
        set_prog_alarm(st->note[0] ? st->note : "PROFILE GATE - ack to continue", false);
        cooklog_event("auto", "PROFILE %s step %d/%d GATE: %s",
                      s_name, s_idx + 1, s_count, st->note);
        return;
    }

    command_pit(step_grill_mode(st->mode), st->target);
    if (st->exit == EXIT_END) {
        set_prog_alarm(st->note[0] ? st->note : "PROFILE COMPLETE", true);
    }
    cooklog_event("auto", "PROFILE %s step %d/%d: %s %d", s_name, s_idx + 1,
                  s_count, step_mode_tok(st->mode), st->target);
    persist_run_state();
}

static void advance_step(void)
{
    if (s_idx + 1 >= s_count) return;   // terminal steps never advance
    s_idx++;
    enter_step();
}

static void stop_program(const char *why, const char *source)
{
    ESP_LOGI(TAG, "program %s stopped (%s)", s_name, why);
    cooklog_event(source, "PROFILE %s stopped (%s)", s_name, why);
    s_running = false;
    s_paused = false;
    s_igniting = false;
    clear_prog_alarm();
    persist_run_state();
}

// --- Engine task ---

static void engine_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        // boot_hold: don't touch anything while the item-4 grill resume is
        // still deciding whether this cook comes back at all
        if (!s_running || s_boot_hold) {
            xSemaphoreGive(s_mutex);
            continue;
        }

        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        grill_mode_t mode = gs->mode;
        float drv_temp = gs->probes[s_driver].current_temp;
        alarm_state_t pa = gs->prog_alarm;
        grill_state_unlock();

        prog_step_t *st = &s_steps[s_idx];

        // User turned the grill Off or started Shutdown: the program is
        // over — checked even while manually paused, no stale programs
        if (mode == GRILL_MODE_OFF || mode == GRILL_MODE_SHUTDOWN) {
            stop_program(mode == GRILL_MODE_OFF ? "grill off" : "shutdown", "auto");
            xSemaphoreGive(s_mutex);
            continue;
        }

        if (s_paused) {
            xSemaphoreGive(s_mutex);
            continue;
        }

        // Ignite phase: waiting for the actuator to promote START->COOK
        if (s_igniting) {
            if (mode == GRILL_MODE_START) { xSemaphoreGive(s_mutex); continue; }
            s_igniting = false;
            enter_step();   // grill is lit — command the first step for real
            xSemaphoreGive(s_mutex);
            continue;
        }

        // Manual override: any mode we didn't command pauses the program.
        // The human always outranks the sequencer.
        if (st->mode != STEP_GATE && mode != s_cmd_mode) {
            s_paused = true;
            ESP_LOGW(TAG, "program paused: manual mode change (%s)", grill_mode_name(mode));
            cooklog_event("auto", "PROFILE %s PAUSED (manual control)", s_name);
            xSemaphoreGive(s_mutex);
            continue;
        }

        // Driver probe fault: hold the step, alarm after a grace period.
        // Never advance blind — pit control itself is still healthy.
        if (st->exit == EXIT_TEMP) {
            if (drv_temp <= 0) {
                if (s_fault_since == 0) s_fault_since = now_s();
                else if (now_s() - s_fault_since > 60 && pa == ALARM_IDLE) {
                    set_prog_alarm("PROFILE HOLD - driver probe fault", false);
                }
                xSemaphoreGive(s_mutex);
                continue;
            }
            if (s_fault_since != 0) {
                s_fault_since = 0;
                if (pa != ALARM_IDLE) clear_prog_alarm();   // signal back
            }
        }

        // Exit conditions (latch one-way: crossing once is crossing forever)
        switch (st->exit) {
        case EXIT_TEMP:
            if (drv_temp >= (float)st->value) advance_step();
            break;
        case EXIT_TIME:
            if (now_s() - s_step_start >= (double)st->value * 60.0) advance_step();
            break;
        case EXIT_ACK:
            if (pa == ALARM_ACKED || pa == ALARM_IDLE) {
                clear_prog_alarm();
                cooklog_event("auto", "PROFILE %s gate passed: %s", s_name, st->note);
                advance_step();
            }
            break;
        case EXIT_END:
            break;   // terminal hold: sit here until the human ends the cook
        }

        xSemaphoreGive(s_mutex);
    }
}

// One-shot: after the item-4 grill resume window has settled, either
// re-command the saved step (grill came back cooking) or clear the stale
// program (grill stayed Off — resume decided the fire was cold).
static void resume_unpause_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(50000));
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_running && s_boot_hold) {
        s_boot_hold = false;
        grill_state_lock();
        grill_mode_t m = grill_state_get()->mode;
        grill_state_unlock();
        if (m != GRILL_MODE_OFF && m != GRILL_MODE_SHUTDOWN) {
            ESP_LOGW(TAG, "program %s resuming at step %d/%d after power loss",
                     s_name, s_idx + 1, s_count);
            cooklog_event("auto", "PROFILE %s resumed at step %d after power loss",
                          s_name, s_idx + 1);
            enter_step();
        } else {
            stop_program("grill not resumed after power loss", "auto");
        }
    }
    xSemaphoreGive(s_mutex);
    vTaskDelete(NULL);
}

// --- Public API ---

bool cookprog_run(const char *name, int driver, const char *source)
{
    if (driver < 0 || driver >= NUM_MEAT_PROBES) return false;
    const char *clean = (name[0] == '*') ? name + 1 : name;

    char text[PROG_FILE_MAX];
    if (!cookprog_read(clean, text, sizeof(text))) return false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_running) { xSemaphoreGive(s_mutex); return false; }

    s_count = parse_program(text, s_steps, COOKPROG_MAX_STEPS);
    if (s_count < 1) { xSemaphoreGive(s_mutex); return false; }

    strlcpy(s_name, clean, sizeof(s_name));
    s_driver = driver;
    s_idx = 0;
    s_running = true;
    s_paused = false;

    grill_state_lock();
    grill_mode_t mode = grill_state_get()->mode;
    float temp = grill_state_get()->grill_temp;
    grill_state_unlock();

    ESP_LOGI(TAG, "running %s, driver P%d, %d steps (grill %s %.0fF)",
             s_name, driver + 1, s_count, grill_mode_name(mode), temp);
    cooklog_event(source, "PROFILE %s started, driver P%d", s_name, driver + 1);

    if (mode == GRILL_MODE_OFF) {
        // Cold grill: ignite first; step 1 is commanded once the actuator
        // promotes past START (its own 115F rule — no engine duplication)
        command_pit(GRILL_MODE_START, s_steps[0].target);
        s_igniting = true;
    } else {
        enter_step();
    }
    persist_run_state();
    xSemaphoreGive(s_mutex);
    return true;
}

void cookprog_stop(const char *source)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_running) stop_program("user stop", source);
    xSemaphoreGive(s_mutex);
}

bool cookprog_resume_manual(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ok = s_running && s_paused;
    if (ok) {
        s_paused = false;
        enter_step();   // re-command the current step's pit state
        cooklog_event("web", "PROFILE %s resumed (manual pause)", s_name);
    }
    xSemaphoreGive(s_mutex);
    return ok;
}

void cookprog_get_status(cookprog_status_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(out, 0, sizeof(*out));
    out->running = s_running;
    out->paused = s_paused;
    if (s_running) {
        strlcpy(out->name, s_name, sizeof(out->name));
        out->step = s_idx;
        out->step_count = s_count;
        out->driver = s_driver;
        prog_step_t *st = &s_steps[s_idx];
        switch (st->exit) {
        case EXIT_TEMP:
            snprintf(out->step_text, sizeof(out->step_text), "%s %d until P%d >= %d",
                     step_mode_tok(st->mode), st->target, s_driver + 1, st->value);
            break;
        case EXIT_TIME:
            snprintf(out->step_text, sizeof(out->step_text), "%s %d for %d min",
                     step_mode_tok(st->mode), st->target, st->value);
            break;
        case EXIT_ACK:
            snprintf(out->step_text, sizeof(out->step_text), "GATE: %s", st->note);
            break;
        default:
            snprintf(out->step_text, sizeof(out->step_text), "%s %d (hold) - %s",
                     step_mode_tok(st->mode), st->target, st->note);
            break;
        }
    }
    xSemaphoreGive(s_mutex);
}

void cookprog_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    // Power-loss resume: if a program was mid-run, rearm it PAUSED-like at
    // the saved step. The engine's ignite/manual logic then works off
    // whatever the item-4 grill resume decided: if the cook resumed hot,
    // re-command the step; if the grill stayed Off, the first engine tick
    // sees OFF and clears the stale program.
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        char name[COOKPROG_NAME_MAX];
        size_t len = sizeof(name);
        uint8_t idx = 0, drv = 0;
        if (nvs_get_str(h, "prog_name", name, &len) == ESP_OK &&
            nvs_get_u8(h, "prog_idx", &idx) == ESP_OK &&
            nvs_get_u8(h, "prog_drv", &drv) == ESP_OK) {
            char text[PROG_FILE_MAX];
            if (cookprog_read(name, text, sizeof(text))) {
                s_count = parse_program(text, s_steps, COOKPROG_MAX_STEPS);
                if (s_count > 0 && idx < s_count && drv < NUM_MEAT_PROBES) {
                    strlcpy(s_name, name, sizeof(s_name));
                    s_idx = idx;
                    s_driver = drv;
                    s_running = true;
                    s_step_start = now_s();   // time-based steps restart
                    s_igniting = false;
                    s_boot_hold = true;       // engine waits out the resume window
                    ESP_LOGW(TAG, "program %s found mid-run (step %d) — waiting for grill resume",
                             s_name, s_idx + 1);
                }
            }
        }
        nvs_close(h);
    }

    xTaskCreate(engine_task, "cookprog", 4096, NULL, 3, NULL);
    if (s_running) {
        xTaskCreate(resume_unpause_task, "prog_resume", 3072, NULL, 3, NULL);
    }
}
