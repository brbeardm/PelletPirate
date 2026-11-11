// V5 pellet hopper gauge — Sharp GP2Y0E02A on J9 -> R47 100R -> IO14
// (ADC2_CH3, R40 pull-up DNP, C41 100nF filter on board).
//
// Ported from the esp_breadboard_test_fw rig that proved ADC2/WiFi
// coexistence (4400+ samples, zero timeouts, 2026-09-22). Sampling is a
// lazy 3 s burst (median-of-9 + EMA) — a hopper drains over hours, not
// milliseconds — with the rig's retry armor kept in case the documented
// ADC2-vs-WiFi arbitration ever shows up in the field.

#include "hopper.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char *TAG = "hopper";

#if CONFIG_PELLETPIRATE_BOARD_V5

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "nvs.h"
#include "grill_state.h"

#define HOPPER_ADC_UNIT   ADC_UNIT_2
#define HOPPER_ADC_CH     ADC_CHANNEL_3   // IO14 (BOARD_HOPPER_GPIO)

#define SAMPLE_PERIOD_MS  3000            // one smoothed update every 3 s
#define BURST_N           9               // median-of-9 inside each update
#define BURST_GAP_MS      10
#define MAX_TRIES         10              // ADC2 retry armor (WiFi arbiter)
#define RETRY_DELAY_MS    10

// Sensor absent: with R40 DNP an unplugged J9 floats near 0 V through the
// RC. The Sharp's live output never goes below ~550 mV (50 cm limit).
#define ABSENT_MV         250

// GP2Y0E02A linear transfer (datasheet nominal, bench-confirmed 09-22):
// 2200 mV @ 4 cm .. 550 mV @ 50 cm  ->  ~35.9 mV/cm falling with distance.
#define CURVE_MV_NEAR     2200.0f
#define CURVE_NEAR_CM     4.0f
#define CURVE_MV_PER_CM   35.87f
// Full-hopper distance: spec mounts the lens ~2.5" above max fill.
#define FULL_DIST_IN      2.5f

// Alert ladder with hysteresis (re-arm 5% above each trigger)
#define TIER_LOW          20
#define TIER_CRIT         10
#define TIER_REARM        5

#define NVS_NS            "pelletpirate"
#define NVS_KEY_DEPTH     "hop_depth"     // tenths of an inch, 0 = unset
#define NVS_KEY_EMPTY     "hop_emv"       // captured endpoints, -1 = unset
#define NVS_KEY_FULL      "hop_fmv"

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_have_cali;

// Written by the sampler task, read by API calls; int writes are atomic.
static volatile int s_mv = -1;            // smoothed; -1 = no sample yet
static volatile int s_depth_tenths = 0;
static volatile int s_empty_mv = -1;
static volatile int s_full_mv = -1;

static int s_tier = 0;                    // current alert tier (0/20/10)
static bool s_absent_logged = false;

static void cal_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    int32_t v;
    if (nvs_get_i32(h, NVS_KEY_DEPTH, &v) == ESP_OK) s_depth_tenths = v;
    if (nvs_get_i32(h, NVS_KEY_EMPTY, &v) == ESP_OK) s_empty_mv = v;
    if (nvs_get_i32(h, NVS_KEY_FULL, &v) == ESP_OK) s_full_mv = v;
    nvs_close(h);
}

static void cal_save(const char *key, int32_t val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, key, val);
        nvs_commit(h);
        nvs_close(h);
    }
}

// Nominal sensor output at a given distance (inches from the lens)
static int curve_mv_at(float dist_in)
{
    float mv = CURVE_MV_NEAR - (dist_in * 2.54f - CURVE_NEAR_CM) * CURVE_MV_PER_CM;
    return (int)mv;
}

// -1 when no usable calibration; else 0-100
static int compute_pct(int mv)
{
    int empty = s_empty_mv, full = s_full_mv;
    if (empty < 0 || full < 0 || (full - empty) < 200) {
        // No valid capture cal — fall back to depth math
        int d = s_depth_tenths;
        if (d < 60 || d > 180) return -1;
        empty = curve_mv_at(d / 10.0f);
        full = curve_mv_at(FULL_DIST_IN);
    }
    int pct = (mv - empty) * 100 / (full - empty);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

// One retried ADC2 read -> calibrated mV, or -1 on hard failure
static int sample_once(void)
{
    for (int tries = 0; tries < MAX_TRIES; tries++) {
        int raw;
        esp_err_t err = adc_oneshot_read(s_adc, HOPPER_ADC_CH, &raw);
        if (err == ESP_OK) {
            int mv = raw;
            if (s_have_cali) adc_cali_raw_to_voltage(s_cali, raw, &mv);
            return mv;
        }
        if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "adc read: %s", esp_err_to_name(err));
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));  // WiFi owned the ADC
    }
    return -1;
}

static void hopper_task(void *arg)
{
    while (1) {
        // Median-of-BURST_N rejects single-sample spikes
        int burst[BURST_N];
        int got = 0;
        for (int i = 0; i < BURST_N; i++) {
            int mv = sample_once();
            if (mv >= 0) burst[got++] = mv;
            vTaskDelay(pdMS_TO_TICKS(BURST_GAP_MS));
        }

        if (got > 0) {
            for (int i = 1; i < got; i++) {        // insertion sort
                int v = burst[i], j = i - 1;
                while (j >= 0 && burst[j] > v) { burst[j + 1] = burst[j]; j--; }
                burst[j + 1] = v;
            }
            int median = burst[got / 2];
            // EMA: ~4 updates (~12 s) to settle a step — calm display,
            // still fast enough to watch pellets pour in during cal
            s_mv = (s_mv < 0) ? median : (3 * s_mv + median) / 4;
        }

        int mv = s_mv;
        int pct = -1;
        if (mv >= ABSENT_MV) {
            pct = compute_pct(mv);
            if (s_absent_logged) {
                ESP_LOGI(TAG, "sensor detected (%d mV)", mv);
                s_absent_logged = false;
            }
        } else if (mv >= 0 && !s_absent_logged) {
            ESP_LOGW(TAG, "no sensor on J9 (%d mV) — gauge shows '--'", mv);
            s_absent_logged = true;
        }

        // Alert ladder (worsen immediately, recover with hysteresis)
        if (pct < 0) {
            s_tier = 0;                            // never alert on a fault
        } else if (pct <= TIER_CRIT) {
            s_tier = TIER_CRIT;
        } else if (pct <= TIER_LOW && (s_tier == 0 || s_tier == TIER_LOW)) {
            s_tier = TIER_LOW;
        } else if ((s_tier == TIER_CRIT && pct > TIER_CRIT + TIER_REARM) ||
                   (s_tier == TIER_LOW && pct > TIER_LOW + TIER_REARM)) {
            s_tier = (s_tier == TIER_CRIT && pct <= TIER_LOW) ? TIER_LOW : 0;
        }

        grill_state_lock();
        grill_state_t *gs = grill_state_get();
        gs->hopper_pct = pct;
        gs->hopper_mv = mv;
        gs->hopper_alert = s_tier;
        grill_state_unlock();

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

void hopper_init(void)
{
    cal_load();

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = HOPPER_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &s_adc) != ESP_OK) {
        ESP_LOGE(TAG, "ADC2 init failed — hopper gauge disabled");
        return;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,          // Sharp tops out at 2.2 V
    };
    adc_oneshot_config_channel(s_adc, HOPPER_ADC_CH, &chan_cfg);

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = HOPPER_ADC_UNIT,
        .chan = HOPPER_ADC_CH,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_have_cali =
        adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) == ESP_OK;

    xTaskCreate(hopper_task, "hopper", 3072, NULL, 2, NULL);
    ESP_LOGI(TAG, "gauge up: IO14/ADC2_CH3, depth=%.1f\", cal %s",
             s_depth_tenths / 10.0,
             (s_empty_mv >= 0 && s_full_mv >= 0) ? "captured" : "curve");
}

void hopper_get_info(int *mv, int *pct, int *depth_tenths,
                     int *empty_mv, int *full_mv)
{
    if (mv) *mv = s_mv;
    if (pct) *pct = (s_mv >= ABSENT_MV) ? compute_pct(s_mv) : -1;
    if (depth_tenths) *depth_tenths = s_depth_tenths;
    if (empty_mv) *empty_mv = s_empty_mv;
    if (full_mv) *full_mv = s_full_mv;
}

esp_err_t hopper_set_depth(int tenths)
{
    if (tenths != 0 && (tenths < 60 || tenths > 180)) return ESP_ERR_INVALID_ARG;
    s_depth_tenths = tenths;
    cal_save(NVS_KEY_DEPTH, tenths);
    ESP_LOGI(TAG, "depth set: %.1f\"", tenths / 10.0);
    return ESP_OK;
}

esp_err_t hopper_capture(int which)
{
    int mv = s_mv;
    if (mv < ABSENT_MV) return ESP_ERR_INVALID_STATE;   // no sensor talking
    if (which == 0) {
        s_empty_mv = mv;
        cal_save(NVS_KEY_EMPTY, mv);
    } else {
        s_full_mv = mv;
        cal_save(NVS_KEY_FULL, mv);
    }
    ESP_LOGI(TAG, "captured %s = %d mV", which ? "FULL" : "EMPTY", mv);
    return ESP_OK;
}

esp_err_t hopper_clear_capture(void)
{
    s_empty_mv = -1;
    s_full_mv = -1;
    cal_save(NVS_KEY_EMPTY, -1);
    cal_save(NVS_KEY_FULL, -1);
    ESP_LOGI(TAG, "capture cal cleared (back to depth curve)");
    return ESP_OK;
}

#else // !CONFIG_PELLETPIRATE_BOARD_V5 — V2/V4 have no analog hopper input

void hopper_init(void) {}
void hopper_get_info(int *mv, int *pct, int *depth_tenths,
                     int *empty_mv, int *full_mv)
{
    if (mv) *mv = -1;
    if (pct) *pct = -1;
    if (depth_tenths) *depth_tenths = 0;
    if (empty_mv) *empty_mv = -1;
    if (full_mv) *full_mv = -1;
}
esp_err_t hopper_set_depth(int tenths) { (void)tenths; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t hopper_capture(int which) { (void)which; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t hopper_clear_capture(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif
