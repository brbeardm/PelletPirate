#include "webui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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

// Connected WebSocket clients (socket fds; -1 = free slot)
#define MAX_WS_CLIENTS 4
static int s_ws_fds[MAX_WS_CLIENTS] = { -1, -1, -1, -1 };

// Embedded dashboard page (see index.html in this component)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// --- Status JSON (shared by GET /api/status and the WS push) ---

static int build_status_json(char *buf, int len)
{
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

    return snprintf(buf, len,
                    "{\"gt\":%.1f,\"tgt\":%d,\"mode\":\"%s\",\"rssi\":%d}",
                    gt, tgt, mode, rssi);
}

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
    build_status_json(buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

// POST /api/grill-target — body is the plain target value, e.g. "230".
// Same clamps as the LCD digit editor; persists to NVS like the LCD does.
static esp_err_t target_post_handler(httpd_req_t *req)
{
    char body[16] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    if (r <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    int t = atoi(body);
    if (t < TARGET_TEMP_MIN || t > TARGET_TEMP_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "target out of range");
        return ESP_FAIL;
    }

    grill_state_lock();
    grill_state_get()->grill_target = t;
    grill_state_save_to_nvs();
    grill_state_unlock();
    ESP_LOGI(TAG, "web: grill target set to %d", t);

    return status_get_handler(req);
}

// --- WebSocket ---

static void ws_client_add(int fd)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) return;
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] < 0) {
            s_ws_fds[i] = fd;
            ESP_LOGI(TAG, "WS client connected (fd %d)", fd);
            return;
        }
    }
    ESP_LOGW(TAG, "WS client table full, fd %d not tracked", fd);
}

static void ws_client_remove(int fd)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) {
            s_ws_fds[i] = -1;
            ESP_LOGI(TAG, "WS client disconnected (fd %d)", fd);
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // Handshake complete — track the client for pushes
        ws_client_add(httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    // Drain any incoming frame (we don't expect client data, but the
    // frame must be consumed; close frames unregister the client)
    httpd_ws_frame_t frame = { 0 };
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK) return err;
    if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        ws_client_remove(httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    if (frame.len > 0 && frame.len < 126) {
        uint8_t payload[126];
        frame.payload = payload;
        httpd_ws_recv_frame(req, &frame, frame.len);
    }
    return ESP_OK;
}

// Push live status to all WS clients ~1/s; log heap every 60s
static void ws_push_task(void *arg)
{
    int tick = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!s_server) continue;

        char buf[160];
        int len = build_status_json(buf, sizeof(buf));

        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_fds[i] < 0) continue;
            httpd_ws_frame_t frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)buf,
                .len = len,
            };
            if (httpd_ws_send_frame_async(s_server, s_ws_fds[i], &frame) != ESP_OK) {
                ws_client_remove(s_ws_fds[i]);
            }
        }

        if (++tick >= 60) {
            tick = 0;
            ESP_LOGI(TAG, "free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        }
    }
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
    const httpd_uri_t target = {
        .uri = "/api/grill-target", .method = HTTP_POST, .handler = target_post_handler,
    };
    const httpd_uri_t ws = {
        .uri = "/ws", .method = HTTP_GET, .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &root);
    httpd_register_uri_handler(s_server, &status);
    httpd_register_uri_handler(s_server, &target);
    httpd_register_uri_handler(s_server, &ws);
    ESP_LOGI(TAG, "HTTP server started (REST + WS)");
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

    xTaskCreate(ws_push_task, "ws_push", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "WiFi station starting, SSID '%s'", CONFIG_PELLETPIRATE_WIFI_SSID);
}
