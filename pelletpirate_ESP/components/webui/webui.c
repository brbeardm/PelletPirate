#include "webui.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "grill_state.h"
#include "sdkconfig.h"

static const char *TAG = "webui";

static httpd_handle_t s_server = NULL;
static int s_retry_count = 0;

// Embedded dashboard page (see index.html in this component)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// --- HTTP handlers ---

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start,
                           index_html_end - index_html_start);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char buf[160];

    // Refresh RSSI opportunistically while we're here
    wifi_ap_record_t ap;
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    gs->wifi_rssi = rssi;
    float gt = gs->grill_temp;
    int tgt = gs->grill_target;
    const char *mode = grill_mode_name(gs->mode);
    grill_state_unlock();

    snprintf(buf, sizeof(buf),
             "{\"gt\":%.1f,\"tgt\":%d,\"mode\":\"%s\",\"rssi\":%d}",
             gt, tgt, mode, rssi);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static void start_webserver(void)
{
    if (s_server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        s_server = NULL;
        return;
    }

    const httpd_uri_t root = {
        .uri = "/", .method = HTTP_GET, .handler = root_get_handler,
    };
    const httpd_uri_t status = {
        .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler,
    };
    httpd_register_uri_handler(s_server, &root);
    httpd_register_uri_handler(s_server, &status);
    ESP_LOGI(TAG, "HTTP server started");
}

// --- WiFi events ---

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        grill_state_lock();
        grill_state_get()->wifi_connected = false;
        grill_state_unlock();
        s_retry_count++;
        if (s_retry_count <= 5 || s_retry_count % 12 == 0) {
            ESP_LOGW(TAG, "WiFi disconnected, retrying (attempt %d)", s_retry_count);
        }
        // Keep retrying forever — the grill must never depend on WiFi,
        // but WiFi should recover on its own when the network returns.
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        s_retry_count = 0;
        ESP_LOGI(TAG, "WiFi connected: " IPSTR, IP2STR(&event->ip_info.ip));
        grill_state_lock();
        grill_state_get()->wifi_connected = true;
        grill_state_unlock();
        start_webserver();
    }
}

void webui_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "netif init failed"); return; }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop failed");
        return;
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "wifi init failed: %s", esp_err_to_name(err)); return; }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_PELLETPIRATE_WIFI_SSID,
            sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_PELLETPIRATE_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    err = esp_wifi_start();
    if (err != ESP_OK) { ESP_LOGE(TAG, "wifi start failed: %s", esp_err_to_name(err)); return; }

    ESP_LOGI(TAG, "WiFi station starting, SSID '%s'", CONFIG_PELLETPIRATE_WIFI_SSID);
}
