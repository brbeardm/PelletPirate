// Fan / Auger / Igniter control task
//
// Ported from PelletPirateC++_WB/src/main.cpp (Particle Photon), keeping
// the field-proven control behavior and adding fan burst modulation.
//
// CARRYOVERS from the Photon code (proven over years of cooks):
//   - Auger burst cycling: ON for cycle*u, OFF for cycle*(1-u)
//   - START: 60s cycle at u=0.25 (15s on / 45s off), igniter on,
//     auto-transition to COOK above IGNITE_DISABLE_TEMP
//   - SMOKE: 15s on / (45 + P*10)s off — Traeger-style P-setting
//   - COOK (was "Hold"): PID (PB=60 Ti=180 Td=45) every 20s,
//     u clamped to [0.15, 1.0]
//   - Igniter auto-assist whenever grill temp is below threshold
//     (flame-out recovery), and the 20-minute igniter safety timeout
//     that forces SHUTDOWN ("**SAFETY FIRST**" in the original)
//   - SHUTDOWN: auger off, fan on for full pellet burn-off, then OFF
//     (both behaviors were hard-won fixes in the original: 4/1/2018
//     auger-off and 12/21/2020 fan-stays-on)
//
// ENHANCEMENTS:
//   - Fan burst modulation: when grill temp is within FAN_BAND_F of
//     target in COOK/KEEP_WARM, the fan cycles at FAN_CYCLE_SEC with a
//     duty tied to the PID output instead of running flat-out. The
//     MOC3063 is a zero-cross driver, so modulation is slow burst
//     on/off (seconds), never kHz PWM.
//   - Hard interlock: fan is forced ON whenever auger or igniter is on
//     (combustion air, prevents hopper burn-back)
//   - SUPER_SMOKE mode: P-setting 4 (15s on / 85s off)
//   - Transition-only logging with mode/temp/duty context
//   - Runtime diagnostics into grill_state (igniter/auger runtime, fan cycles)

#include "actuator.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "grill_state.h"
#include "cooklog.h"
}
#include "pid.h"

static const char *TAG = "actuator";

// --- Carryover constants (Photon main.cpp values) ---
static const double PID_PB = 60.0, PID_TI = 180.0, PID_TD = 45.0;
static const double PID_CYCLE_SEC = 20.0;   // PIDCycleTime
static const double U_MIN = 0.15, U_MAX = 1.0;
static const double START_CYCLE_SEC = 60.0, START_U = 0.25;
static const double SMOKE_ON_SEC = 15.0;
static const int    P_SETTING_SMOKE = 2;    // off = 45 + P*10
static const int    P_SETTING_SUPER = 4;
static const double IGNITER_MAX_ON_SEC = 1200.0;  // 20 min safety timeout
static const double SHUTDOWN_BURNOFF_SEC = 900.0; // 15 min (V2 spec; Photon used 600)
static const double RTD_FAULT_SHUTDOWN_SEC = 30.0; // grill sensor dead this long -> burn-off

// Flame-out: sensor healthy but the fire died. Conservative on purpose —
// wind gusts and lid-opens cause 30-40F dips that recover well inside 10
// minutes, and the igniter auto-assist (below 115F) gets its chance first.
static const double FLAMEOUT_DROP_F = 60.0;  // this far below target...
static const double FLAMEOUT_SEC = 600.0;    // ...for this long -> burn-off

// --- Enhancement constants ---
// 2s burst period: at 50% duty that's 1s on / 1s off — the impeller never
// spins down, giving quasi-continuous reduced airflow (integral-cycle
// control; the MOC3063 zero-cross driver forbids real PWM/phase-angle).
// Was 30s, which audibly stopped the fan for 15s stretches. LISTENING
// TEST REQUIRED on the real fan: if it hums/surges, fall back to 30s or
// set FAN_MIN_DUTY=1.0.
static const double FAN_CYCLE_SEC = 2.0;
static const double FAN_BAND_F = 10.0;      // pulse fan when within +/- this of target
static const double FAN_MIN_DUTY = 0.5;

// Fuel starvation detector (COOK/KEEP_WARM): auger pegged while temp
// dives = fuel not reaching the fire (empty hopper, bridging, jam).
// Signature from the 2026-07-18 pellet-out: u=1.00, 260->215F in 4 min.
static const double STARVE_U_MIN = 0.95;      // auger effectively pegged
static const double STARVE_DROP_F = 20.0;     // temp fell this much...
static const double STARVE_WINDOW_SEC = 240;  // ...within this window
// Bounded-P hybrid smoke: keep the feast/famine smolder engine but nudge
// the famine length so temp stays near the target band. Target acts as a
// band center, NOT a setpoint (set it low, ~170, for classic smoke temps).
static const int SMOKE_P_MIN = 1, SMOKE_P_MAX = 6;
static const double SMOKE_BAND_F = 15.0;      // deadband around target
static const double SMOKE_ADJ_SEC = 90.0;     // one P step per this interval

// Anti-surge stagger on cold start (leaving OFF): fan first, then auger,
// then igniter. Motor inrush lasts ~0.5-1s, so 2s stages let each load's
// inrush finish before the next switches on. Zero-cross triacs handle
// sub-cycle timing already; this only spreads the big steps.
static const double AUGER_STAGGER_SEC = 2.0;
static const double IGNITER_STAGGER_SEC = 4.0;

static pid *s_pid = nullptr;
static double s_u = U_MIN;                  // current auger duty

// Burst-cycler state (auger and fan each get one)
struct burst_state {
    bool on;
    double toggle_time;
};
static burst_state s_auger = {};
static burst_state s_fan = {};

static double s_igniter_on_since = 0;       // 0 = igniter off
static double s_active_since = 0;           // when we last left OFF (0 = in OFF)
static double s_fault_since = 0;            // grill RTD reading 0.0 in an active mode
static double s_flameout_since = 0;         // temp sustained far below target
static grill_mode_t s_prev_mode = GRILL_MODE_OFF;
static int s_prev_target = -1;              // for mid-cook target persist

// Starvation detector: temp ring sampled every 30s, oldest-vs-now
#define STARVE_RING 8                       // 8 x 30s = 4 min window
static float s_starve_ring[STARVE_RING] = {0};
static int s_starve_idx = 0, s_starve_count = 0;
static double s_starve_last_sample = 0;

// Bounded-P smoke state
static int s_smoke_p_offset = 0;
static double s_smoke_last_adj = 0;

// Physical pin states, for transition-only logging
static bool s_pin_fan = false, s_pin_aug = false, s_pin_ign = false;

static double now_sec(void)
{
    return (double)esp_timer_get_time() / 1000000.0;
}

// Same toggle logic as the Photon DoAugerControl()
static bool burst_cycle(burst_state *b, double now, double cycle, double duty)
{
    if (duty >= 0.97) { b->on = true; b->toggle_time = now; return true; }
    if (duty <= 0.03) { b->on = false; b->toggle_time = now; return false; }
    if (b->on && (now - b->toggle_time) > cycle * duty) {
        b->on = false;
        b->toggle_time = now;
    } else if (!b->on && (now - b->toggle_time) > cycle * (1.0 - duty)) {
        b->on = true;
        b->toggle_time = now;
    }
    return b->on;
}

static void burst_start_on(burst_state *b, double now)
{
    b->on = true;
    b->toggle_time = now;
}

static void apply_pin(int gpio, bool want, bool *cur, const char *name,
                      grill_mode_t mode, float temp)
{
    if (want != *cur) {
        gpio_set_level((gpio_num_t)gpio, want ? 1 : 0);
        *cur = want;
        ESP_LOGI(TAG, "%s %s (mode=%s temp=%.0fF u=%.2f)",
                 name, want ? "ON" : "off", grill_mode_name(mode), temp, s_u);
    }
}

static void actuator_task(void *arg)
{
    // A hung control loop with fire burning must reboot the board — the
    // boot-time GPIO holds drive every output LOW again (safe state).
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();
        double now = now_sec();

        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        grill_mode_t mode = gs->mode;
        float temp = gs->grill_temp;
        int target = gs->grill_target;
        uint32_t shutdown_start = gs->shutdown_start_time;
        bool reached_band = gs->grill_reached_band;
        grill_state_unlock();

        // Mode-entry housekeeping
        if (mode != s_prev_mode) {
            ESP_LOGI(TAG, "mode %s -> %s (temp=%.0fF target=%d)",
                     grill_mode_name(s_prev_mode), grill_mode_name(mode), temp, target);
            s_fault_since = 0;  // each mode gets a fresh sensor-fault grace period
            s_flameout_since = 0;
            s_starve_count = 0; s_starve_idx = 0;   // fresh starvation window
            s_smoke_p_offset = 0; s_smoke_last_adj = now;
            if (mode == GRILL_MODE_COOK || mode == GRILL_MODE_KEEP_WARM) {
                s_pid->setTarget(target, 0);  // reset integrator (Photon SetMode=Hold)
                s_u = U_MIN;                  // start at maintenance level
            }
            if (mode != GRILL_MODE_OFF && s_prev_mode == GRILL_MODE_OFF) {
                burst_start_on(&s_auger, now);  // cycles begin in the ON phase
                burst_start_on(&s_fan, now);
                s_active_since = now;
                grill_state_lock();
                grill_state_cook_started();     // ET zero, per-cook probe baselines
                grill_state_unlock();
                ESP_LOGI(TAG, "cold start — anti-surge stagger: fan t+0, auger t+%.0fs, igniter t+%.0fs",
                         AUGER_STAGGER_SEC, IGNITER_STAGGER_SEC);
            } else if (mode == GRILL_MODE_OFF) {
                s_active_since = 0;
                grill_state_lock();
                grill_state_cook_ended();
                grill_state_unlock();
            }
            if (mode == GRILL_MODE_SHUTDOWN) {
                grill_state_lock();
                gs->shutdown_start_time = (uint32_t)now;
                grill_state_unlock();
                shutdown_start = (uint32_t)now;
            }
            // Power-loss resume record: every transition, including auto ones
            // and clean OFF (which overwrites any stale cook state). Written
            // AFTER cook_started so the wall-clock start rides along.
            grill_state_persist_run(mode, target);
            s_prev_mode = mode;
        }

        // Mid-cook target changes must reach the resume record too, or a
        // power loss would resume at a stale target
        if (mode != GRILL_MODE_OFF && s_prev_target != -1 && target != s_prev_target) {
            grill_state_persist_run(mode, target);
        }
        s_prev_target = target;

        bool fan = false, aug = false, ign = false;
        grill_mode_t new_mode = mode;
        bool fault_shutdown = false;

        // Grill RTD fault (0.0F convention) while running: the controller
        // is blind. Grace period rides out transients (the median filter
        // upstream already ate single glitches); a persistent fault forces
        // a shutdown burn-off — never keep feeding pellets on a dead sensor.
        bool sensor_fault = (temp <= 0.0f);
        if (sensor_fault && mode != GRILL_MODE_OFF && mode != GRILL_MODE_SHUTDOWN) {
            if (s_fault_since == 0) {
                s_fault_since = now;
                ESP_LOGW(TAG, "grill RTD fault — holding output, igniter inhibited");
            } else if ((now - s_fault_since) > RTD_FAULT_SHUTDOWN_SEC) {
                ESP_LOGE(TAG, "SAFETY: grill RTD dead for %.0f s — forcing SHUTDOWN",
                         now - s_fault_since);
                cooklog_event("auto", "RTD FAULT grill %.0fs - forced SHUTDOWN",
                              now - s_fault_since);
                new_mode = GRILL_MODE_SHUTDOWN;
                fault_shutdown = true;
            }
        } else if (!sensor_fault) {
            if (s_fault_since != 0) {
                ESP_LOGI(TAG, "grill RTD recovered (%.0fF)", temp);
            }
            s_fault_since = 0;
        }

        switch (mode) {
        case GRILL_MODE_OFF:
            break;

        case GRILL_MODE_START:
        case GRILL_MODE_REIGNITE:
            aug = burst_cycle(&s_auger, now, START_CYCLE_SEC, START_U);
            ign = (temp > 0 && temp < IGNITE_DISABLE_TEMP);
            fan = true;
            if (temp >= IGNITE_DISABLE_TEMP) {
                ESP_LOGI(TAG, "grill lit (%.0fF >= %.0fF) — auto %s -> COOK",
                         temp, IGNITE_DISABLE_TEMP, grill_mode_name(mode));
                new_mode = GRILL_MODE_COOK;
            }
            break;

        case GRILL_MODE_SMOKE:
        case GRILL_MODE_SUPER_SMOKE: {
            // Bounded-P hybrid: the feast/famine cycle IS the smoke engine
            // (fresh pellets smoldering on dying embers), so never PID this —
            // instead nudge the famine length one step at a time when temp
            // drifts outside target±band. Base P preserves each mode's
            // character; offset is bounded so it stays a smoke mode.
            int base = (mode == GRILL_MODE_SMOKE) ? P_SETTING_SMOKE : P_SETTING_SUPER;
            if (temp > 0 && target > 0 && (now - s_smoke_last_adj) >= SMOKE_ADJ_SEC) {
                s_smoke_last_adj = now;
                if (temp < target - SMOKE_BAND_F && base + s_smoke_p_offset > SMOKE_P_MIN) {
                    s_smoke_p_offset--;   // too cold: shorter famine, more fuel
                } else if (temp > target + SMOKE_BAND_F && base + s_smoke_p_offset < SMOKE_P_MAX) {
                    s_smoke_p_offset++;   // too hot: longer famine
                }
            }
            int p = base + s_smoke_p_offset;
            if (p < SMOKE_P_MIN) p = SMOKE_P_MIN;
            if (p > SMOKE_P_MAX) p = SMOKE_P_MAX;
            double off = 45.0 + p * 10.0;
            double cyc = SMOKE_ON_SEC + off;
            aug = burst_cycle(&s_auger, now, cyc, SMOKE_ON_SEC / cyc);
            ign = (temp > 0 && temp < IGNITE_DISABLE_TEMP);  // cold-smoke flame assist
            fan = true;
            break;
        }

        case GRILL_MODE_COOK:
        case GRILL_MODE_KEEP_WARM: {
            // On sensor fault: skip the PID (a 0.0F reading looks like
            // "225F too cold" and winds the output to max) and hold the
            // last duty until recovery or the fault shutdown fires.
            if (!sensor_fault && (now - s_pid->LastUpdate) > PID_CYCLE_SEC) {
                double u = s_pid->update(temp, target, 0);
                s_u = u < U_MIN ? U_MIN : (u > U_MAX ? U_MAX : u);
                ESP_LOGI(TAG, "PID: temp=%.1fF target=%d u=%.2f", temp, target, s_u);
            }
            aug = burst_cycle(&s_auger, now, PID_CYCLE_SEC, s_u);
            ign = (temp > 0 && temp < IGNITE_DISABLE_TEMP);  // flame-out recovery

            // Fan burst modulation once temp settles into the band;
            // air scales with fuel (duty tied to PID output)
            if (temp > 0 && (temp > target - FAN_BAND_F) && (temp < target + FAN_BAND_F)) {
                double fduty = 2.0 * s_u;
                if (fduty < FAN_MIN_DUTY) fduty = FAN_MIN_DUTY;
                if (fduty > 1.0) fduty = 1.0;
                fan = burst_cycle(&s_fan, now, FAN_CYCLE_SEC, fduty);
            } else {
                fan = true;
            }
            break;
        }

        case GRILL_MODE_SHUTDOWN:
            fan = true;   // pellet burn-off — fan MUST run (Photon 12/21/2020 fix)
            aug = false;  // no new fuel (Photon 4/1/2018 fix)
            ign = false;
            if (shutdown_start != 0 && (now - shutdown_start) > SHUTDOWN_BURNOFF_SEC) {
                ESP_LOGI(TAG, "burn-off complete (%.0f s) — SHUTDOWN -> OFF",
                         now - (double)shutdown_start);
                new_mode = GRILL_MODE_OFF;
                fan = false;
            }
            break;

        default:
            break;
        }

        // Flame-out: the cook reached the band, the sensor is healthy, yet
        // temp has fallen and stayed far below target. The igniter assist
        // (below 115F) already had its chance to relight; stop feeding
        // pellets into a dead pot and burn off. The temp-drop alarm banner
        // fired at -30F on the way down, so the user was already warned.
        if ((mode == GRILL_MODE_COOK || mode == GRILL_MODE_KEEP_WARM) &&
            !sensor_fault && reached_band &&
            temp < (float)target - FLAMEOUT_DROP_F) {
            if (s_flameout_since == 0) {
                s_flameout_since = now;
                ESP_LOGW(TAG, "possible flame-out: %.0fF vs target %d — watching %.0fs",
                         temp, target, FLAMEOUT_SEC);
            } else if ((now - s_flameout_since) > FLAMEOUT_SEC) {
                ESP_LOGE(TAG, "SAFETY: flame-out suspected (%.0fF, %.0fF below target "
                         "for %.0f s) — forcing SHUTDOWN",
                         temp, (float)target - temp, now - s_flameout_since);
                cooklog_event("auto", "FLAME-OUT suspected (%.0fF) - forced SHUTDOWN", temp);
                new_mode = GRILL_MODE_SHUTDOWN;
            }
        } else {
            s_flameout_since = 0;
        }

        // Fuel starvation early warning — fires ~10 min before the flame-out
        // shutdown would. Auger pegged + temp diving = fuel isn't reaching
        // the fire (empty hopper, bridged pellets, jammed auger). Alarm
        // only; the flame-out shutdown remains the enforcement layer.
        if ((mode == GRILL_MODE_COOK || mode == GRILL_MODE_KEEP_WARM) && !sensor_fault) {
            if (now - s_starve_last_sample >= 30.0) {
                s_starve_last_sample = now;
                s_starve_ring[s_starve_idx] = temp;
                s_starve_idx = (s_starve_idx + 1) % STARVE_RING;
                if (s_starve_count < STARVE_RING) s_starve_count++;
            }
            if (s_starve_count >= STARVE_RING) {
                float oldest = s_starve_ring[s_starve_idx];  // next write slot = oldest
                bool starving = (s_u >= STARVE_U_MIN) &&
                                (temp <= oldest - (float)STARVE_DROP_F);
                grill_state_lock();
                alarm_state_t pa = gs->pellet_alarm;
                if (starving && pa == ALARM_IDLE) {
                    gs->pellet_alarm = ALARM_ACTIVE;
                } else if (!starving && pa == ALARM_ACKED) {
                    gs->pellet_alarm = ALARM_IDLE;   // refilled/recovered — re-arm
                }
                grill_state_unlock();
                if (starving && pa == ALARM_IDLE) {
                    ESP_LOGE(TAG, "PELLET STARVATION suspected: u=%.2f, %.0fF -> %.0fF over %.0fs",
                             s_u, oldest, temp, STARVE_WINDOW_SEC);
                    cooklog_event("auto", "CHECK PELLETS: auger max, temp %.0fF falling", temp);
                }
            }
        } else {
            s_starve_count = 0;
            s_starve_idx = 0;
        }

        // Anti-surge stagger: on a cold start, hold auger and igniter back
        // until their stage times so the three inrush events never stack
        if (s_active_since != 0) {
            double active_for = now - s_active_since;
            if (active_for < AUGER_STAGGER_SEC) aug = false;
            if (active_for < IGNITER_STAGGER_SEC) ign = false;
        }

        // Interlock: never feed pellets or ignite without combustion air
        if (aug || ign) fan = true;

        // Igniter safety timeout (carryover: "**SAFETY FIRST**")
        if (ign) {
            if (s_igniter_on_since == 0) s_igniter_on_since = now;
            if ((now - s_igniter_on_since) > IGNITER_MAX_ON_SEC) {
                ESP_LOGE(TAG, "SAFETY: igniter on %.0f s without reaching %.0fF — forcing SHUTDOWN",
                         now - s_igniter_on_since, IGNITE_DISABLE_TEMP);
                ign = false;
                new_mode = GRILL_MODE_SHUTDOWN;
            }
        } else {
            s_igniter_on_since = 0;
        }

        apply_pin(ACTUATOR_FAN_GPIO, fan, &s_pin_fan, "FAN", mode, temp);
        apply_pin(ACTUATOR_AUGER_GPIO, aug, &s_pin_aug, "AUGER", mode, temp);
        apply_pin(ACTUATOR_IGNITER_GPIO, ign, &s_pin_ign, "IGNITER", mode, temp);

        // Publish states + diagnostics
        grill_state_lock();
        bool fan_was_on = gs->fan_on;
        gs->fan_on = fan;
        gs->auger_on = aug;
        gs->igniter_on = ign;
        gs->pid_u = (float)s_u;
        if (ign) gs->igniter_runtime_sec += 1.0f;
        if (aug) gs->auger_runtime_sec += 1.0f;
        if (fan && !fan_was_on) gs->fan_cycles++;
        if (new_mode != mode) gs->mode = new_mode;
        if (fault_shutdown) gs->sensor_alarm = ALARM_ACTIVE;  // banner + web alarm
        grill_state_unlock();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void actuator_init(void)
{
    const int pins[3] = { ACTUATOR_FAN_GPIO, ACTUATOR_IGNITER_GPIO, ACTUATOR_AUGER_GPIO };
    for (int i = 0; i < 3; i++) {
        gpio_set_level((gpio_num_t)pins[i], 0);
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << pins[i];
        cfg.mode = GPIO_MODE_OUTPUT;
        cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&cfg);
        gpio_set_level((gpio_num_t)pins[i], 0);
    }

    s_pid = new pid(PID_PB, PID_TI, PID_TD, 0);

    xTaskCreatePinnedToCore(actuator_task, "actuator", 4096, NULL, 6, NULL, 1);
    ESP_LOGI(TAG, "Actuator control started (FAN=%d IGNITER=%d AUGER=%d, all LOW)",
             ACTUATOR_FAN_GPIO, ACTUATOR_IGNITER_GPIO, ACTUATOR_AUGER_GPIO);
}
