// V5 fan AC phase control. Theory of operation in include/fanphase.h.
//
// Rung-2 measurements this engine is built on (V5 board 1, AC, 2026-11-07):
// qualified ZC rises at 120 Hz / 8331 us +/-9; HIGH pulse ~312 us centered
// on the zero crossing; raw edges chatter (first capture: ~80 runts/sec,
// bogus 300 us periods) so every edge is qualified by dead-time lockout.

#include "sdkconfig.h"
#include "fanphase.h"

#if CONFIG_PELLETPIRATE_BOARD_V5

#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "boardpins.h"

static const char *TAG = "fanphase";

// ---- BENCH TEST SWEEP ----------------------------------------------------
// When 1: any nonzero fan command runs a repeating staircase instead
// (100/80/60/45/35/30 percent, 20 s per step) for the duty-floor listening
// test. MUST be 0 for a cook build — init logs a loud warning when on.
#define FANPHASE_TEST_SWEEP 0
// ---------------------------------------------------------------------------

#define ZC_DEADTIME_US      6000    // lockout after an accepted rising edge
#define ZC_RISE_TO_ZERO_US  156     // rising edge leads true zero (312/2)
#define FIRE_WINDOW_US      7600    // latest fire point after true zero
#define ZC_STALE_US         100000  // no ZC this long -> failsafe full-on
#define FANPHASE_LEVEL_FLOOR 0.30f  // phase-starved AC fan motors stall

// Output plan for the ISRs, derived from the commanded level in task
// context only (Xtensa FreeRTOS forbids FPU use inside ISRs — keep ISR
// work integer-only).
enum { PLAN_OFF = 0, PLAN_PHASE = 1, PLAN_FULL = 2 };
static volatile int      s_plan = PLAN_OFF;
static volatile uint32_t s_fire_delay_us = 0;  // from accepted rise, PLAN_PHASE

static float s_level_req = 0.0f;   // last commanded level (task context)
static float s_level_eff = -1.0f;  // last applied level, for change logging
static bool  s_inited = false;
static bool  s_failsafe = false;

static gptimer_handle_t s_gpt = NULL;

typedef struct {
    // continuity state (never reset between report windows)
    int64_t  last_rise_us;  // most recent ACCEPTED rising edge
    int64_t  last_fall_us;  // most recent falling edge, any
    // per-window stats
    uint32_t rises;
    uint32_t raw_edges;
    uint32_t chatter;       // rising edges rejected by the lockout
    int64_t  period_sum_us;
    uint32_t period_n;
    uint32_t period_min_us, period_max_us;
    int64_t  width_sum_us;
    uint32_t width_n;
    uint32_t width_min_us, width_max_us;
} zc_stats_t;

static volatile zc_stats_t s_zc;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static void stats_reset(volatile zc_stats_t *s)
{
    s->rises = 0;
    s->raw_edges = 0;
    s->chatter = 0;
    s->period_sum_us = 0;
    s->period_n = 0;
    s->period_min_us = UINT32_MAX;
    s->period_max_us = 0;
    s->width_sum_us = 0;
    s->width_n = 0;
    s->width_min_us = UINT32_MAX;
    s->width_max_us = 0;
}

// Alarm ISR: the fire point of this half-cycle. The pin then holds HIGH
// until the next qualified ZC edge drops it.
static bool IRAM_ATTR fire_alarm_cb(gptimer_handle_t timer,
                                    const gptimer_alarm_event_data_t *edata,
                                    void *user_ctx)
{
    if (s_plan == PLAN_PHASE) {
        gpio_set_level(BOARD_FAN_GPIO, 1);
    }
    return false;
}

static void IRAM_ATTR zc_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    int level = gpio_get_level(BOARD_ZC_DET_GPIO);

    portENTER_CRITICAL_ISR(&s_lock);
    s_zc.raw_edges++;

    if (level) {
        if (s_zc.last_rise_us == 0 ||
            (now - s_zc.last_rise_us) > ZC_DEADTIME_US) {
            // Qualified rising edge = new half-cycle. Stats first.
            if (s_zc.last_rise_us != 0) {
                uint32_t period = (uint32_t)(now - s_zc.last_rise_us);
                s_zc.period_sum_us += period;
                s_zc.period_n++;
                if (period < s_zc.period_min_us) s_zc.period_min_us = period;
                if (period > s_zc.period_max_us) s_zc.period_max_us = period;

                if (s_zc.last_fall_us > s_zc.last_rise_us) {
                    uint32_t width = (uint32_t)(s_zc.last_fall_us - s_zc.last_rise_us);
                    s_zc.width_sum_us += width;
                    s_zc.width_n++;
                    if (width < s_zc.width_min_us) s_zc.width_min_us = width;
                    if (width > s_zc.width_max_us) s_zc.width_max_us = width;
                }
            }
            s_zc.last_rise_us = now;
            s_zc.rises++;

            // Drive plan for this half-cycle. The LED drops ~156 us before
            // the voltage zero; the latched triac conducts on to its
            // current zero, so there is no early cutout — and no immediate
            // retrigger into the next half-cycle.
            switch (s_plan) {
            case PLAN_FULL:
                gpio_set_level(BOARD_FAN_GPIO, 1);
                break;
            case PLAN_PHASE:
                gpio_set_level(BOARD_FAN_GPIO, 0);
                gptimer_set_raw_count(s_gpt, 0);
                {
                    gptimer_alarm_config_t al = {
                        .alarm_count = s_fire_delay_us,
                        .reload_count = 0,
                        .flags = { .auto_reload_on_alarm = false },
                    };
                    gptimer_set_alarm_action(s_gpt, &al);
                }
                break;
            default:   // PLAN_OFF
                gpio_set_level(BOARD_FAN_GPIO, 0);
                break;
            }
        } else {
            s_zc.chatter++;
        }
    } else {
        s_zc.last_fall_us = now;
    }

    portEXIT_CRITICAL_ISR(&s_lock);
}

// Task context only (float math). Converts a level into the ISR plan.
static void apply_level(float level)
{
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    if (level > 0.0f && level < FANPHASE_LEVEL_FLOOR) level = FANPHASE_LEVEL_FLOOR;

    int plan;
    uint32_t delay_us = 0;
    if (level <= 0.0f) {
        plan = PLAN_OFF;
    } else if (level >= 0.99f) {
        plan = PLAN_FULL;
    } else {
        // POWER-linear mapping (added after the 2026-11-07 listening test:
        // phase-linear steps clustered into ~3 audible speeds). Level =
        // fraction of delivered power; firing angle from the inverse of
        // P(a) = (pi - a + sin(2a)/2) / pi, tabulated per 10% of power.
        // Index i = power 1.0 - i/10; value = firing angle as a fraction
        // of the half-cycle (a/180deg).
        static const float ang_frac[11] = {
            0.000f,  // 100% power -> fire at true zero
            0.259f,  //  90% -> 46.6 deg
            0.336f,  //  80% -> 60.6 deg
            0.396f,  //  70% -> 71.3 deg
            0.449f,  //  60% -> 80.9 deg
            0.500f,  //  50% -> 90.0 deg (exact by symmetry)
            0.551f,  //  40% -> 99.1 deg
            0.604f,  //  30% -> 108.6 deg
            0.664f,  //  20% -> 119.5 deg
            0.741f,  //  10% -> 133.4 deg
            0.900f,  //   0% tail (true value 1.0; FIRE_WINDOW clamp + the
                     //   level floor keep this entry from ever firing)
        };
        float p = 1.0f - level;            // 0..1 across the table
        float fi = p * 10.0f;
        int i = (int)fi;
        if (i > 9) i = 9;
        float frac = ang_frac[i] + (ang_frac[i + 1] - ang_frac[i]) * (fi - (float)i);
        plan = PLAN_PHASE;
        delay_us = ZC_RISE_TO_ZERO_US + (uint32_t)(frac * 8333.0f);
        if (delay_us > ZC_RISE_TO_ZERO_US + FIRE_WINDOW_US) {
            delay_us = ZC_RISE_TO_ZERO_US + FIRE_WINDOW_US;
        }
    }

    portENTER_CRITICAL(&s_lock);
    s_fire_delay_us = delay_us;
    s_plan = plan;
    portEXIT_CRITICAL(&s_lock);

    // Snap the pin for the edge-free cases so off/full don't wait for
    // (or depend on) a ZC edge. PLAN_PHASE corrects itself within 8.3 ms.
    if (plan == PLAN_OFF) {
        gpio_set_level(BOARD_FAN_GPIO, 0);
    } else if (plan == PLAN_FULL || s_failsafe) {
        gpio_set_level(BOARD_FAN_GPIO, 1);
    }

    if (level != s_level_eff) {
        ESP_LOGI(TAG, "FAN level %.2f (%s, fire delay %lu us)",
                 level,
                 plan == PLAN_OFF ? "off" : plan == PLAN_FULL ? "full" : "phase",
                 (unsigned long)delay_us);
        s_level_eff = level;
    }
}

void fanphase_set_level(float level)
{
    s_level_req = level;
    if (!s_inited) return;   // applied at init
#if !FANPHASE_TEST_SWEEP
    apply_level(level);
#endif
    // With the sweep compiled in, the supervisor task owns the applied
    // level; the request only gates sweep-on vs off.
}

// 1 Hz supervisor: ZC diagnostics log + failsafe watchdog + test sweep.
static void supervisor_task(void *arg)
{
#if FANPHASE_TEST_SWEEP
    static const float sweep_steps[] = { 1.0f, 0.80f, 0.60f, 0.45f, 0.35f, 0.30f };
    const int n_steps = sizeof(sweep_steps) / sizeof(sweep_steps[0]);
    int sweep_idx = -1;
    int64_t step_since = 0;
#endif

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        int64_t now = esp_timer_get_time();

#if FANPHASE_TEST_SWEEP
        if (s_level_req > 0.0f) {
            if (sweep_idx < 0 || (now - step_since) > 20 * 1000000LL) {
                sweep_idx = (sweep_idx < 0) ? 0 : (sweep_idx + 1) % n_steps;
                step_since = now;
                ESP_LOGW(TAG, "TEST SWEEP step %d/%d: %.0f%%",
                         sweep_idx + 1, n_steps, sweep_steps[sweep_idx] * 100.0f);
                apply_level(sweep_steps[sweep_idx]);
            }
        } else if (sweep_idx >= 0) {
            sweep_idx = -1;
            apply_level(0.0f);
        }
#endif

        // Failsafe: fan commanded on but the ZC reference is gone (bench
        // power, H11AA1 fault, broken trace). Full-on = full combustion
        // air = the safe direction.
        portENTER_CRITICAL(&s_lock);
        int64_t last_rise = s_zc.last_rise_us;
        portEXIT_CRITICAL(&s_lock);
        bool zc_stale = (last_rise == 0) || (now - last_rise) > ZC_STALE_US;
        bool want_on = (s_level_eff > 0.0f);
        if (zc_stale && want_on && !s_failsafe) {
            s_failsafe = true;
            gpio_set_level(BOARD_FAN_GPIO, 1);
            ESP_LOGE(TAG, "ZC reference lost — failsafe: fan FULL ON");
        } else if (s_failsafe && (!zc_stale || !want_on)) {
            s_failsafe = false;
            if (!want_on) gpio_set_level(BOARD_FAN_GPIO, 0);
            ESP_LOGW(TAG, "failsafe cleared (%s)", zc_stale ? "fan off" : "ZC back");
        }

        // Diagnostics snapshot
        zc_stats_t snap;
        portENTER_CRITICAL(&s_lock);
        snap = *(zc_stats_t *)&s_zc;
        stats_reset(&s_zc);
        portEXIT_CRITICAL(&s_lock);

        if (snap.rises == 0) {
            ESP_LOGI(TAG, "ZC: no edges (bench power? H11AA1 only runs on mains)");
            continue;
        }
        ESP_LOGI(TAG, "ZC: %lu Hz | period us min/avg/max %lu/%lu/%lu | "
                      "HIGH width us min/avg/max %lu/%lu/%lu | raw %lu, chatter %lu",
                 (unsigned long)snap.rises,
                 (unsigned long)(snap.period_n ? snap.period_min_us : 0),
                 (unsigned long)(snap.period_n ? snap.period_sum_us / snap.period_n : 0),
                 (unsigned long)(snap.period_n ? snap.period_max_us : 0),
                 (unsigned long)(snap.width_n ? snap.width_min_us : 0),
                 (unsigned long)(snap.width_n ? snap.width_sum_us / snap.width_n : 0),
                 (unsigned long)(snap.width_n ? snap.width_max_us : 0),
                 (unsigned long)snap.raw_edges,
                 (unsigned long)snap.chatter);
    }
}

void fanphase_init(void)
{
    stats_reset(&s_zc);
    s_zc.last_rise_us = 0;
    s_zc.last_fall_us = 0;

    // Fan pin: take over from the boot-time LOW hold (idempotent re-config)
    gpio_config_t out_cfg = {
        .pin_bit_mask = 1ULL << BOARD_FAN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));
    gpio_set_level(BOARD_FAN_GPIO, 0);

    // 1 MHz free-running GPTimer; the ZC ISR re-arms its alarm each
    // half-cycle (set_raw_count / set_alarm_action are ISR-safe).
    gptimer_config_t gpt_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&gpt_cfg, &s_gpt));
    gptimer_event_callbacks_t cbs = { .on_alarm = fire_alarm_cb };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_gpt, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(s_gpt));
    ESP_ERROR_CHECK(gptimer_start(s_gpt));

    // ZC input: external 10k pullup (R43) + 1nF (C42) at the pin on V5.
    gpio_config_t in_cfg = {
        .pin_bit_mask = 1ULL << BOARD_ZC_DET_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(BOARD_ZC_DET_GPIO, zc_isr, NULL));

    s_inited = true;
    apply_level(s_level_req);   // honor any pre-init command

    xTaskCreate(supervisor_task, "fanphase", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "phase engine up: ZC=IO%d fan=IO%d, floor %.0f%%",
             BOARD_ZC_DET_GPIO, BOARD_FAN_GPIO, FANPHASE_LEVEL_FLOOR * 100.0f);
#if FANPHASE_TEST_SWEEP
    ESP_LOGW(TAG, "*** TEST SWEEP BUILD — fan staircases whenever commanded on; NOT a cook build ***");
#endif
}

#else  // !CONFIG_PELLETPIRATE_BOARD_V5

void fanphase_init(void) {}
void fanphase_set_level(float level) { (void)level; }

#endif
