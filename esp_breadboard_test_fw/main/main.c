/*
 * PelletPirate Sharp GP2Y0E02A / ADC2-vs-WiFi coexistence test + web test rig
 * Board:  PelletPirate_ESP breakout, ESP32-S3-WROOM-1U-N16R8
 * Sensor: Sharp Vout -> 100R -> IO14 = ADC2_CH3 (mirrors V5 J9 front end)
 *
 * Phase A: 20 s of ADC2_CH3 sampling with WiFi OFF  -> baseline, expect 0 timeouts
 * Phase B: WiFi STA + UDP heartbeat + HTTP dashboard, retry-loop sampling forever.
 *          Browse to the printed IP: live %-full gauge, Set EMPTY / Set FULL
 *          calibration, and the running timeout stats = the V5 go/no-go number.
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#define LED_GPIO          12              /* module pin 20, external test LED */
#define SHARP_ADC_UNIT    ADC_UNIT_2
#define SHARP_ADC_CH      ADC_CHANNEL_3   /* IO14 */
#define SAMPLE_PERIOD_MS  100             /* 10 Hz — far faster than a hopper gauge needs */
#define MAX_TRIES         10              /* attempts per sample before declaring hard failure */
#define RETRY_DELAY_MS    10
#define BASELINE_SAMPLES  200             /* 20 s @ 10 Hz */
#define STATS_WINDOW      50              /* log every 5 s */

static const char *TAG = "SHARP_ADC2";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_have_cali;

static EventGroupHandle_t s_wifi_evt;
#define WIFI_CONNECTED_BIT BIT0

typedef struct {
    uint32_t samples;
    uint32_t ok_first;      /* succeeded on try 1 */
    uint32_t ok_retry;      /* succeeded on try 2..MAX_TRIES */
    uint32_t hard_fail;     /* all tries exhausted */
    uint32_t timeouts;      /* total ESP_ERR_TIMEOUT hits across all tries */
    uint32_t max_tries;     /* worst tries-needed for any successful sample */
    int      mv_min, mv_max;
    int64_t  mv_sum;
    uint32_t mv_count;
} stats_t;

/* Shared with the web handlers; single writer (sampler), racy reads are benign. */
static stats_t s_total;
static volatile int s_mv_latest = 0;     /* median+EMA smoothed display value */
static volatile int s_mv_empty = -1;     /* calibration points, RAM only */
static volatile int s_mv_full = -1;

/* Median-of-9 (rejects single-sample spikes) into a slow EMA (~2 s settle at 10 Hz). */
#define FILT_N 9
static int s_filt_buf[FILT_N];
static int s_filt_idx, s_filt_count;

static void display_update(int mv)
{
    s_filt_buf[s_filt_idx] = mv;
    s_filt_idx = (s_filt_idx + 1) % FILT_N;
    if (s_filt_count < FILT_N) s_filt_count++;

    int tmp[FILT_N];
    memcpy(tmp, s_filt_buf, s_filt_count * sizeof(int));
    for (int i = 1; i < s_filt_count; i++) {
        int v = tmp[i], j = i - 1;
        while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = v;
    }
    int median = tmp[s_filt_count / 2];

    if (s_filt_count < FILT_N) {
        s_mv_latest = median;                          /* fast seed while filling */
    } else {
        s_mv_latest = (s_mv_latest * 15 + median) / 16;
    }
}

static void stats_reset(stats_t *s)
{
    memset(s, 0, sizeof(*s));
    s->mv_min = INT32_MAX;
    s->mv_max = INT32_MIN;
}

/* One gauge sample with the same retry policy V5 firmware would use. */
static bool sharp_sample(stats_t *s, int *out_mv)
{
    s->samples++;
    for (uint32_t tries = 1; tries <= MAX_TRIES; tries++) {
        int raw;
        esp_err_t err = adc_oneshot_read(s_adc, SHARP_ADC_CH, &raw);
        if (err == ESP_OK) {
            int mv = raw;
            if (s_have_cali) {
                adc_cali_raw_to_voltage(s_cali, raw, &mv);
            }
            if (tries == 1) s->ok_first++; else s->ok_retry++;
            if (tries > s->max_tries) s->max_tries = tries;
            if (mv < s->mv_min) s->mv_min = mv;
            if (mv > s->mv_max) s->mv_max = mv;
            s->mv_sum += mv;
            s->mv_count++;
            *out_mv = mv;
            return true;
        }
        if (err == ESP_ERR_TIMEOUT) {
            s->timeouts++;              /* WiFi owned the ADC — the failure mode under test */
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }
        ESP_LOGE(TAG, "adc_oneshot_read unexpected error: %s", esp_err_to_name(err));
        break;
    }
    s->hard_fail++;
    return false;
}

static void stats_print(const char *phase, const stats_t *s)
{
    int avg = s->mv_count ? (int)(s->mv_sum / s->mv_count) : 0;
    ESP_LOGI(TAG,
        "[%s] %" PRIu32 " samples | first-try %" PRIu32 ", retried %" PRIu32
        " (worst %" PRIu32 " tries), HARD-FAIL %" PRIu32 " | timeouts %" PRIu32
        " | mV min/avg/max %d/%d/%d",
        phase, s->samples, s->ok_first, s->ok_retry, s->max_tries,
        s->hard_fail, s->timeouts,
        s->mv_count ? s->mv_min : 0, avg, s->mv_count ? s->mv_max : 0);
}

/* ------------------------------ web test rig ------------------------------ */

static const char INDEX_HTML[] =
"<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>PelletPirate Hopper Sensor</title>"
"<style>body{font-family:sans-serif;background:#1b1b1f;color:#eee;text-align:center;margin-top:2em}"
"#pct{font-size:5em;font-weight:bold}#mv{font-size:1.5em;color:#8bc34a}"
"button{font-size:1.1em;margin:.4em;padding:.5em 1.2em;border-radius:8px;border:0;background:#444;color:#eee}"
".bar{width:80%;max-width:420px;height:24px;background:#333;border-radius:12px;margin:1em auto}"
"#fill{height:100%;background:#8bc34a;border-radius:12px;width:0}"
"#cals,#stats{color:#888;font-size:.9em;margin-top:1em}</style></head><body>"
"<h2>Hopper Sensor Test</h2><div id=pct>--</div>"
"<div class=bar><div id=fill></div></div>"
"<div id=mv>-- mV</div>"
"<p><button onclick=cal('empty')>Set EMPTY (now)</button>"
"<button onclick=cal('full')>Set FULL (now)</button></p>"
"<div id=cals></div><div id=stats></div>"
"<script>"
"async function cal(w){await fetch('/cal?set='+w);tick();}"
"async function tick(){try{let d=await(await fetch('/data')).json();"
"document.getElementById('mv').textContent=d.mv+' mV';"
"document.getElementById('pct').textContent=(d.pct<0?'--':d.pct+'%');"
"document.getElementById('fill').style.width=(d.pct<0?0:d.pct)+'%';"
"document.getElementById('cals').textContent='cal: empty='+d.mv_empty+' mV, full='+d.mv_full+' mV';"
"document.getElementById('stats').textContent=d.samples+' samples | timeouts '+d.timeouts"
"+' | retried '+d.ok_retry+' | hard-fail '+d.hard_fail;"
"}catch(e){}}"
"setInterval(tick,500);tick();"
"</script></body></html>";

static esp_err_t index_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t data_get(httpd_req_t *req)
{
    int mv = s_mv_latest, empty = s_mv_empty, full = s_mv_full;
    int pct = -1;
    if (empty >= 0 && full >= 0 && full != empty) {
        pct = (mv - empty) * 100 / (full - empty);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
    }
    char buf[224];
    int len = snprintf(buf, sizeof(buf),
        "{\"mv\":%d,\"pct\":%d,\"mv_empty\":%d,\"mv_full\":%d,"
        "\"samples\":%" PRIu32 ",\"timeouts\":%" PRIu32
        ",\"ok_retry\":%" PRIu32 ",\"hard_fail\":%" PRIu32 "}",
        mv, pct, empty, full,
        s_total.samples, s_total.timeouts, s_total.ok_retry, s_total.hard_fail);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static esp_err_t cal_get(httpd_req_t *req)
{
    char query[32], val[8] = "";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "set", val, sizeof(val));
    }
    if (strcmp(val, "empty") == 0) {
        s_mv_empty = s_mv_latest;
        ESP_LOGI(TAG, "calibrated EMPTY = %d mV", s_mv_empty);
    } else if (strcmp(val, "full") == 0) {
        s_mv_full = s_mv_latest;
        ESP_LOGI(TAG, "calibrated FULL = %d mV", s_mv_full);
    }
    return httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
}

static void web_start(void)
{
    httpd_handle_t srv = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&srv, &cfg));
    httpd_uri_t u_index = { .uri = "/",     .method = HTTP_GET, .handler = index_get };
    httpd_uri_t u_data  = { .uri = "/data", .method = HTTP_GET, .handler = data_get };
    httpd_uri_t u_cal   = { .uri = "/cal",  .method = HTTP_GET, .handler = cal_get };
    httpd_register_uri_handler(srv, &u_index);
    httpd_register_uri_handler(srv, &u_data);
    httpd_register_uri_handler(srv, &u_cal);
}

/* --------------------------------- WiFi ---------------------------------- */

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *dis = data;
        xEventGroupClearBits(s_wifi_evt, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi disconnected (reason %d), reconnecting...", dis->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR "  — open http://" IPSTR "/ for the dashboard",
                 IP2STR(&evt->ip_info.ip), IP2STR(&evt->ip_info.ip));
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);
    }
}

/* Constant TX traffic so the radio stays busy like it would with the web UI live. */
static void heartbeat_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(3333),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST),
    };
    uint32_t n = 0;
    char payload[48];
    while (1) {
        xEventGroupWaitBits(s_wifi_evt, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        int len = snprintf(payload, sizeof(payload), "pelletpirate-hb %" PRIu32, n++);
        sendto(sock, payload, len, 0, (struct sockaddr *)&dest, sizeof(dest));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    wifi_config_t sta = { 0 };
    strlcpy((char *)sta.sta.ssid, CONFIG_PELLETPIRATE_WIFI_SSID, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, CONFIG_PELLETPIRATE_WIFI_PASSWORD, sizeof(sta.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* No modem sleep: radio maximally active = worst case for the ADC2 arbiter. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Sharp GP2Y0E02A on IO14 (ADC2_CH3) vs WiFi test");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led);

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = SHARP_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,        /* full 0..~3.1 V range, Sharp tops out at 2.2 V */
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, SHARP_ADC_CH, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = SHARP_ADC_UNIT,
        .chan = SHARP_ADC_CH,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_have_cali = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) == ESP_OK;
    ESP_LOGI(TAG, "ADC calibration: %s", s_have_cali ? "curve-fitting" : "NONE (raw counts)");

    /* ---- Phase A: baseline, WiFi off ---- */
    ESP_LOGI(TAG, "Phase A: %d samples @ %d ms, WiFi OFF", BASELINE_SAMPLES, SAMPLE_PERIOD_MS);
    stats_t base;
    stats_reset(&base);
    int led_on = 0;
    for (int i = 0; i < BASELINE_SAMPLES; i++) {
        int mv;
        if (sharp_sample(&base, &mv)) {
            display_update(mv);
        }
        if (i % 10 == 0) {
            gpio_set_level(LED_GPIO, led_on = !led_on);
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
    stats_print("BASELINE", &base);

    /* ---- Phase B: WiFi + web dashboard, sample forever ---- */
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);
    s_wifi_evt = xEventGroupCreate();
    wifi_start();
    web_start();
    xTaskCreate(heartbeat_task, "heartbeat", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "Phase B: WiFi STA '%s', web dashboard on port 80, sampling forever",
             CONFIG_PELLETPIRATE_WIFI_SSID);

    stats_t window;
    stats_reset(&s_total);
    stats_reset(&window);
    uint32_t n = 0;
    while (1) {
        int mv = 0;
        stats_t probe;      /* sample once, fold into both accumulators */
        stats_reset(&probe);
        bool ok = sharp_sample(&probe, &mv);
        if (ok) {
            display_update(mv);
        }

        stats_t *dst[2] = { &window, &s_total };
        for (int d = 0; d < 2; d++) {
            dst[d]->samples   += probe.samples;
            dst[d]->ok_first  += probe.ok_first;
            dst[d]->ok_retry  += probe.ok_retry;
            dst[d]->hard_fail += probe.hard_fail;
            dst[d]->timeouts  += probe.timeouts;
            if (probe.max_tries > dst[d]->max_tries) dst[d]->max_tries = probe.max_tries;
            if (ok) {
                if (mv < dst[d]->mv_min) dst[d]->mv_min = mv;
                if (mv > dst[d]->mv_max) dst[d]->mv_max = mv;
                dst[d]->mv_sum += mv;
                dst[d]->mv_count++;
            }
        }

        if (++n % 10 == 0) {
            gpio_set_level(LED_GPIO, led_on = !led_on);
        }
        if (n % STATS_WINDOW == 0) {
            stats_print("WIFI 5s", &window);
            stats_reset(&window);
            if (n % (STATS_WINDOW * 12) == 0) {   /* cumulative once a minute */
                stats_print("WIFI TOTAL", &s_total);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
