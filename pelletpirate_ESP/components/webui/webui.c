#include "webui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_sntp.h"
#include "cooklog.h"
#include "profiles.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "grill_state.h"
#include "nvs.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "webui";

static void start_webserver(void);

static httpd_handle_t s_server = NULL;
static int s_retry_count = 0;

// Connected WebSocket clients (socket fds; -1 = free slot). The table is
// mutated from httpd worker threads and read by ws_push_task — every
// access goes through the spinlock.
#define MAX_WS_CLIENTS 4
static int s_ws_fds[MAX_WS_CLIENTS] = { -1, -1, -1, -1 };
static portMUX_TYPE s_ws_mux = portMUX_INITIALIZER_UNLOCKED;

// Embedded dashboard page (see index.html in this component)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// --- Status JSON (shared by GET /api/status and the WS push) ---

static void json_escape(char *dst, int len, const char *src);

static int build_status_json(char *buf, int len)
{
    wifi_ap_record_t ap;
    int rssi = 0;
    char ssid_esc[40] = "";
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        rssi = ap.rssi;
        json_escape(ssid_esc, sizeof(ssid_esc), (const char *)ap.ssid);
    }

    char alarm_txt[72] = "";
    bool alarm = grill_state_alarm_active();
    if (alarm) grill_state_alarm_text(alarm_txt, sizeof(alarm_txt));

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    gs->wifi_rssi = rssi;

    int n = snprintf(buf, len,
                     "{\"gt\":%.1f,\"tgt\":%d,\"mode\":\"%s\",\"rssi\":%d,"
                     "\"et\":%d,\"f\":%d,\"a\":%d,\"ig\":%d,"
                     "\"al\":%d,\"alt\":\"%s\",\"pv\":%lu,"
                     "\"ssid\":\"%s\",\"p\":[",
                     gs->grill_temp, gs->grill_target,
                     grill_mode_name(gs->mode), rssi,
                     grill_state_get_elapsed_minutes(),
                     gs->fan_on ? 1 : 0, gs->auger_on ? 1 : 0,
                     gs->igniter_on ? 1 : 0,
                     alarm ? 1 : 0, alarm_txt,
                     (unsigned long)profiles_revision(), ssid_esc);
    for (int i = 0; i < NUM_MEAT_PROBES && n < len; i++) {
        probe_state_t *p = &gs->probes[i];
        n += snprintf(buf + n, len - n,
                      "%s{\"en\":%d,\"t\":%.1f,\"tg\":%d,\"es\":%d,"
                      "\"am\":%d,\"at\":\"%s\",\"fd\":\"%s\"}",
                      i ? "," : "", p->enabled ? 1 : 0, p->current_temp,
                      (int)p->target_temp, grill_state_get_est_minutes(i),
                      (int)p->alarm_temp, p->alarm_type, p->food_type);
    }
    if (n < len) n += snprintf(buf + n, len - n, "]}");
    grill_state_unlock();
    return n;
}

// --- HTTP handlers ---

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start,
                           index_html_end - index_html_start);
}

#define STATUS_JSON_MAX 896

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char buf[STATUS_JSON_MAX];
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
    cooklog_event("web", "TARGET %d", t);

    return status_get_handler(req);
}

// POST /api/probe-target — body is "<probe 1-4> <target>", e.g. "2 165".
// Target 0 clears/disables the probe (same semantics as the LCD wizard,
// including clearing food and alarm); a nonzero target on a disabled
// probe re-enables it with its previous food/alarm settings.
static esp_err_t probe_post_handler(httpd_req_t *req)
{
    char body[24] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    if (r <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    int idx = 0, t = -1;
    if (sscanf(body, "%d %d", &idx, &t) != 2 || idx < 1 || idx > NUM_MEAT_PROBES ||
        t < 0 || (t != 0 && (t < 100 || t > 499))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "want: <probe 1-4> <0|100-499>");
        return ESP_FAIL;
    }

    grill_state_lock();
    probe_state_t *p = &grill_state_get()->probes[idx - 1];
    if (t == 0) {
        p->enabled = false;
        p->target_temp = 0;
        p->alarm_temp = 0;
        p->food_type[0] = '\0';
        p->alarm_type[0] = '\0';
    } else {
        p->enabled = true;
        p->target_temp = (float)t;
    }
    grill_state_save_to_nvs();
    grill_state_unlock();
    ESP_LOGI(TAG, "web: probe %d target set to %d", idx, t);
    cooklog_event("web", "PROBE %d target %d", idx, t);

    return status_get_handler(req);
}

// POST /api/probe-config — full probe setup matching the LCD wizard.
// Body is pipe-delimited: idx|target|alarm|food|alarmtype
// e.g. 2|203|165|Brisket|Wrap. Target 0 disables and clears everything
// (LCD semantics); alarm 0 clears the alarm and its type.
static esp_err_t probe_config_post_handler(httpd_req_t *req)
{
    char body[96] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    if (r <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }

    // Split into 5 fields on '|', preserving empty fields
    char *f[5] = { body, NULL, NULL, NULL, NULL };
    int nf = 1;
    for (char *c = body; *c && nf < 5; c++) {
        if (*c == '|') { *c = '\0'; f[nf++] = c + 1; }
    }
    if (nf < 5) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "want: idx|target|alarm|food|alarmtype");
        return ESP_FAIL;
    }
    int idx = atoi(f[0]);
    int tg = atoi(f[1]);
    int am = atoi(f[2]);
    if (idx < 1 || idx > NUM_MEAT_PROBES ||
        (tg != 0 && (tg < 100 || tg > 499)) ||
        (am != 0 && (am < 100 || am > 499))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "value out of range");
        return ESP_FAIL;
    }

    grill_state_lock();
    probe_state_t *p = &grill_state_get()->probes[idx - 1];
    if (tg == 0) {
        p->enabled = false;
        p->target_temp = 0;
        p->alarm_temp = 0;
        p->food_type[0] = '\0';
        p->alarm_type[0] = '\0';
    } else {
        p->enabled = true;
        p->target_temp = (float)tg;
        strlcpy(p->food_type, f[3], sizeof(p->food_type));
        if (am == 0) {
            p->alarm_temp = 0;
            p->alarm_type[0] = '\0';
        } else {
            p->alarm_temp = (float)am;
            strlcpy(p->alarm_type, f[4], sizeof(p->alarm_type));
        }
    }
    grill_state_save_to_nvs();
    grill_state_unlock();
    ESP_LOGI(TAG, "web: probe %d configured (tg=%d am=%d food='%s' type='%s')",
             idx, tg, am, f[3], f[4]);
    cooklog_event("web", "PROBE %d cfg tg=%d al=%d %s/%s", idx, tg, am, f[3], f[4]);

    return status_get_handler(req);
}

// POST /api/mode — body is a mode token. Guards mirror the LCD exactly:
// start only from Off; cook modes and shutdown only when not Off/Ignite
// (the LCD grays out COOK MODE in those states); off always allowed.
static esp_err_t mode_post_handler(httpd_req_t *req)
{
    char body[16] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    if (r <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }

    static const struct { const char *tok; grill_mode_t m; } map[] = {
        { "off",        GRILL_MODE_OFF },
        { "start",      GRILL_MODE_START },
        { "smoke",      GRILL_MODE_SMOKE },
        { "supersmoke", GRILL_MODE_SUPER_SMOKE },
        { "cook",       GRILL_MODE_COOK },
        { "keepwarm",   GRILL_MODE_KEEP_WARM },
        { "shutdown",   GRILL_MODE_SHUTDOWN },
        { "reignite",   GRILL_MODE_REIGNITE },
    };
    int found = -1;
    for (int i = 0; i < (int)(sizeof(map) / sizeof(map[0])); i++) {
        if (strncmp(body, map[i].tok, sizeof(body)) == 0) { found = i; break; }
    }
    if (found < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown mode");
        return ESP_FAIL;
    }
    grill_mode_t want = map[found].m;

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    grill_mode_t cur = gs->mode;

    bool ok;
    if (want == GRILL_MODE_OFF) {
        ok = true;
    } else if (want == GRILL_MODE_START) {
        ok = (cur == GRILL_MODE_OFF);
    } else {
        // cook modes, shutdown, reignite: same gate as the LCD COOK MODE menu
        ok = (cur != GRILL_MODE_OFF && cur != GRILL_MODE_START);
    }

    if (ok) {
        gs->mode = want;
        if (want == GRILL_MODE_OFF) {
            gs->fan_on = false;
            gs->auger_on = false;
            gs->igniter_on = false;
        }
    }
    grill_state_unlock();

    if (!ok) {
        ESP_LOGW(TAG, "web: mode '%s' rejected (current %s)", body, grill_mode_name(cur));
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "not allowed in current mode");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "web: mode set to %s (was %s)", grill_mode_name(want), grill_mode_name(cur));
    cooklog_event("web", "MODE %s>%s", grill_mode_name(cur), grill_mode_name(want));
    return status_get_handler(req);
}

// POST /api/alarm-ack — acknowledge all active alarms (same as LCD hold)
static esp_err_t alarm_ack_post_handler(httpd_req_t *req)
{
    grill_state_alarm_ack();
    ESP_LOGI(TAG, "web: alarms acknowledged");
    cooklog_event("web", "ALARM ACK");
    return status_get_handler(req);
}

// GET /api/profiles — list saved cook profiles (newest first)
static esp_err_t profiles_get_handler(httpd_req_t *req)
{
    profile_info_t list[PROFILES_LIST_MAX];
    int count = profiles_list(list, PROFILES_LIST_MAX);
    char buf[640];
    int n = snprintf(buf, sizeof(buf), "[");
    for (int i = 0; i < count && n < (int)sizeof(buf) - 96; i++) {
        n += snprintf(buf + n, sizeof(buf) - n, "%s{\"f\":\"%s\",\"d\":\"%s\"}",
                      i ? "," : "", list[i].fname, list[i].display);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "]");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, n);
}

// POST /api/profile-save — snapshot current settings as a new profile
static esp_err_t profile_save_post_handler(httpd_req_t *req)
{
    char name[PROFILE_NAME_MAX];
    if (!profiles_save_current(name, sizeof(name))) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "web: profile saved: %s", name);
    cooklog_event("web", "PROFILE SAVE %s", name);
    return profiles_get_handler(req);
}

// POST /api/profile-load — body is the profile filename
static esp_err_t profile_load_post_handler(httpd_req_t *req)
{
    char body[PROFILE_NAME_MAX] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    if (r <= 0 || !profiles_load(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "load failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "web: profile loaded: %s", body);
    cooklog_event("web", "PROFILE LOAD %s", body);
    return status_get_handler(req);
}

// POST /api/profile-delete — body is the profile filename
static esp_err_t profile_delete_post_handler(httpd_req_t *req)
{
    char body[PROFILE_NAME_MAX] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    if (r <= 0 || !profiles_delete(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "delete failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "web: profile deleted: %s", body);
    cooklog_event("web", "PROFILE DELETE %s", body);
    return profiles_get_handler(req);
}

// GET /api/log — list cook files; GET /api/log?f=<name> — stream one CSV
static esp_err_t log_get_handler(httpd_req_t *req)
{
    char query[80], fname[48];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "f", fname, sizeof(fname)) == ESP_OK) {
        if (strncmp(fname, "cook_", 5) != 0 || strchr(fname, '/') || strstr(fname, "..")) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad filename");
            return ESP_FAIL;
        }
        char path[64];
        snprintf(path, sizeof(path), "/lfs/%s", fname);
        FILE *file = fopen(path, "r");
        if (!file) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no such log");
            return ESP_FAIL;
        }
        httpd_resp_set_type(req, "text/csv");
        char chunk[512];
        size_t n;
        while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0) {
            if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) {
                fclose(file);
                httpd_resp_send_chunk(req, NULL, 0);
                return ESP_FAIL;
            }
        }
        fclose(file);
        return httpd_resp_send_chunk(req, NULL, 0);
    }

    // No query: list the cook files as JSON
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[");
    DIR *dir = opendir("/lfs");
    if (dir) {
        struct dirent *de;
        bool first = true;
        while ((de = readdir(dir)) != NULL && n < (int)sizeof(buf) - 64) {
            if (strncmp(de->d_name, "cook_", 5) != 0) continue;
            char path[64];
            struct stat st = { 0 };
            snprintf(path, sizeof(path), "/lfs/%.40s", de->d_name);
            stat(path, &st);
            n += snprintf(buf + n, sizeof(buf) - n, "%s{\"n\":\"%.40s\",\"s\":%ld}",
                          first ? "" : ",", de->d_name, (long)st.st_size);
            first = false;
        }
        closedir(dir);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "]");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, n);
}

// --- SoftAP setup fallback ---
// If WiFi hasn't connected 25s after boot (wrong creds / new location),
// raise an open AP so a phone can configure WiFi at http://192.168.4.1.
// The AP drops a few seconds after the board joins a real network.

#define SETUP_AP_SSID "PelletPirate-Setup"

static bool s_ap_active = false;

static void set_ap_flag(bool on)
{
    grill_state_lock();
    grill_state_get()->wifi_ap_active = on;
    grill_state_unlock();
}

static void start_softap(void)
{
    if (s_ap_active) return;
    wifi_config_t ap = { 0 };
    strcpy((char *)ap.ap.ssid, SETUP_AP_SSID);
    ap.ap.ssid_len = strlen(SETUP_AP_SSID);
    ap.ap.channel = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 2;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK) {
        ESP_LOGE(TAG, "setup AP start failed");
        return;
    }
    s_ap_active = true;
    set_ap_flag(true);
    start_webserver();  // no-op if already up
    ESP_LOGI(TAG, "Setup AP up: '%s' -> http://192.168.4.1", SETUP_AP_SSID);
}

static void ap_stop_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(8000));  // let the AP client see the response
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_ap_active = false;
    set_ap_flag(false);
    ESP_LOGI(TAG, "Setup AP stopped (station connected)");
    vTaskDelete(NULL);
}

static void ap_check_cb(void *arg)
{
    grill_state_lock();
    bool up = grill_state_get()->wifi_connected;
    grill_state_unlock();
    if (!up) start_softap();
}

// Apply new credentials shortly after replying so the HTTP response gets
// out before the radio reconfigures (vital when serving via the setup AP).
static char s_pending_ssid[33];
static char s_pending_pass[65];

static void wifi_apply_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(750));
    webui_wifi_set_credentials(s_pending_ssid, s_pending_pass);
    vTaskDelete(NULL);
}

// Copy src into dst escaping JSON-special characters (SSIDs are wild)
static void json_escape(char *dst, int len, const char *src)
{
    int n = 0;
    for (; *src && n < len - 3; src++) {
        if (*src == '"' || *src == '\\') dst[n++] = '\\';
        if ((unsigned char)*src < 0x20) continue;
        dst[n++] = *src;
    }
    dst[n] = '\0';
}

// GET /api/wifi-scan — list nearby networks (blocking, ~2s)
static esp_err_t wifi_scan_get_handler(httpd_req_t *req)
{
    webui_ap_t aps[12];
    int count = webui_wifi_scan(aps, 12);
    if (count < 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ESP_FAIL;
    }
    char buf[768];
    int n = snprintf(buf, sizeof(buf), "[");
    for (int i = 0; i < count && n < (int)sizeof(buf) - 96; i++) {
        char esc[72];
        json_escape(esc, sizeof(esc), aps[i].ssid);
        n += snprintf(buf + n, sizeof(buf) - n, "%s{\"s\":\"%s\",\"r\":%d,\"x\":%d}",
                      i ? "," : "", esc, aps[i].rssi, aps[i].secure ? 1 : 0);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "]");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, n);
}

// POST /api/wifi-set — body is "ssid\npassword"; applied after the reply
static esp_err_t wifi_set_post_handler(httpd_req_t *req)
{
    char body[100] = { 0 };
    int recv_len = req->content_len < (int)sizeof(body) - 1
                       ? req->content_len : (int)sizeof(body) - 1;
    int r = httpd_req_recv(req, body, recv_len);
    char *nl = r > 0 ? strchr(body, '\n') : NULL;
    if (!nl || nl == body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "want ssid\\npass");
        return ESP_FAIL;
    }
    *nl = '\0';
    strlcpy(s_pending_ssid, body, sizeof(s_pending_ssid));
    strlcpy(s_pending_pass, nl + 1, sizeof(s_pending_pass));
    xTaskCreate(wifi_apply_task, "wifi_apply", 4096, NULL, 3, NULL);
    cooklog_event("web", "WIFI SET %s", s_pending_ssid);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":1}", HTTPD_RESP_USE_STRLEN);
}

// --- WebSocket ---

static void ws_client_add(int fd)
{
    bool added = false, present = false;
    portENTER_CRITICAL(&s_ws_mux);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) present = true;
    }
    if (!present) {
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_fds[i] < 0) {
                s_ws_fds[i] = fd;
                added = true;
                break;
            }
        }
    }
    portEXIT_CRITICAL(&s_ws_mux);
    if (added) ESP_LOGI(TAG, "WS client connected (fd %d)", fd);
    else if (!present) ESP_LOGW(TAG, "WS client table full, fd %d not tracked", fd);
}

static void ws_client_remove(int fd)
{
    bool removed = false;
    portENTER_CRITICAL(&s_ws_mux);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) {
            s_ws_fds[i] = -1;
            removed = true;
        }
    }
    portEXIT_CRITICAL(&s_ws_mux);
    if (removed) ESP_LOGI(TAG, "WS client disconnected (fd %d)", fd);
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

        char buf[STATUS_JSON_MAX];
        int len = build_status_json(buf, sizeof(buf));

        // Snapshot the table so sends happen outside the spinlock
        int fds[MAX_WS_CLIENTS];
        portENTER_CRITICAL(&s_ws_mux);
        for (int i = 0; i < MAX_WS_CLIENTS; i++) fds[i] = s_ws_fds[i];
        portEXIT_CRITICAL(&s_ws_mux);

        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (fds[i] < 0) continue;
            httpd_ws_frame_t frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)buf,
                .len = len,
            };
            if (httpd_ws_send_frame_async(s_server, fds[i], &frame) != ESP_OK) {
                ws_client_remove(fds[i]);
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
    config.max_uri_handlers = 16;  // default 8 silently drops extras; we register 15
    config.stack_size = 8192;      // default 4k overflows in the wifi-scan handler

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
    const httpd_uri_t probe = {
        .uri = "/api/probe-target", .method = HTTP_POST, .handler = probe_post_handler,
    };
    const httpd_uri_t probe_cfg = {
        .uri = "/api/probe-config", .method = HTTP_POST, .handler = probe_config_post_handler,
    };
    const httpd_uri_t mode = {
        .uri = "/api/mode", .method = HTTP_POST, .handler = mode_post_handler,
    };
    const httpd_uri_t ack = {
        .uri = "/api/alarm-ack", .method = HTTP_POST, .handler = alarm_ack_post_handler,
    };
    const httpd_uri_t logs = {
        .uri = "/api/log", .method = HTTP_GET, .handler = log_get_handler,
    };
    const httpd_uri_t prof_list = {
        .uri = "/api/profiles", .method = HTTP_GET, .handler = profiles_get_handler,
    };
    const httpd_uri_t prof_save = {
        .uri = "/api/profile-save", .method = HTTP_POST, .handler = profile_save_post_handler,
    };
    const httpd_uri_t prof_load = {
        .uri = "/api/profile-load", .method = HTTP_POST, .handler = profile_load_post_handler,
    };
    const httpd_uri_t prof_delete = {
        .uri = "/api/profile-delete", .method = HTTP_POST, .handler = profile_delete_post_handler,
    };
    const httpd_uri_t wifi_scan = {
        .uri = "/api/wifi-scan", .method = HTTP_GET, .handler = wifi_scan_get_handler,
    };
    const httpd_uri_t wifi_set = {
        .uri = "/api/wifi-set", .method = HTTP_POST, .handler = wifi_set_post_handler,
    };
    const httpd_uri_t ws = {
        .uri = "/ws", .method = HTTP_GET, .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &root);
    httpd_register_uri_handler(s_server, &status);
    httpd_register_uri_handler(s_server, &target);
    httpd_register_uri_handler(s_server, &probe);
    httpd_register_uri_handler(s_server, &probe_cfg);
    httpd_register_uri_handler(s_server, &mode);
    httpd_register_uri_handler(s_server, &ack);
    httpd_register_uri_handler(s_server, &logs);
    httpd_register_uri_handler(s_server, &prof_list);
    httpd_register_uri_handler(s_server, &prof_save);
    httpd_register_uri_handler(s_server, &prof_load);
    httpd_register_uri_handler(s_server, &prof_delete);
    httpd_register_uri_handler(s_server, &wifi_scan);
    httpd_register_uri_handler(s_server, &wifi_set);
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
        grill_state_get()->wifi_ip[0] = '\0';
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
        grill_state_t *gs = grill_state_get();
        gs->wifi_connected = true;
        snprintf(gs->wifi_ip, sizeof(gs->wifi_ip), IPSTR, IP2STR(&event->ip_info.ip));
        grill_state_unlock();

        // Wall-clock time for cook-log timestamps
        static bool sntp_started = false;
        if (!sntp_started) {
            sntp_started = true;
            esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_netif_sntp_init(&sntp_cfg);
        }

        // Setup AP no longer needed once we're on a real network
        if (s_ap_active) {
            xTaskCreate(ap_stop_task, "ap_stop", 2560, NULL, 3, NULL);
        }

        start_webserver();
    }
}

// --- WiFi setup support (LCD Settings > WI-FI SETUP) ---

#define WIFI_NVS_NS "wificfg"

int webui_wifi_scan(webui_ap_t *out, int max)
{
    wifi_scan_config_t sc = { 0 };  // active scan, all channels
    esp_err_t err = esp_wifi_scan_start(&sc, true);
    bool paused_connect = false;
    if (err != ESP_OK) {
        // Unprovisioned boards sit in a connect-retry loop which blocks
        // scanning — pause it, scan, and resume the retries afterwards.
        esp_wifi_disconnect();
        paused_connect = true;
        vTaskDelay(pdMS_TO_TICKS(200));
        err = esp_wifi_scan_start(&sc, true);
    }
    if (err != ESP_OK) {
        if (paused_connect) esp_wifi_connect();
        return -1;
    }

    // ~80B per record — heap, not stack (httpd workers have small stacks)
    uint16_t num = 20;
    wifi_ap_record_t *recs = malloc(num * sizeof(wifi_ap_record_t));
    if (!recs) {
        esp_wifi_clear_ap_list();
        if (paused_connect) esp_wifi_connect();
        return -1;
    }
    esp_err_t rec_err = esp_wifi_scan_get_ap_records(&num, recs);
    if (paused_connect) esp_wifi_connect();
    if (rec_err != ESP_OK) {
        free(recs);
        return -1;
    }

    int count = 0;
    for (int i = 0; i < num; i++) {
        if (recs[i].ssid[0] == '\0') continue;
        int j;
        for (j = 0; j < count; j++) {
            if (strcmp(out[j].ssid, (const char *)recs[i].ssid) == 0) break;
        }
        if (j < count) {
            if (recs[i].rssi > out[j].rssi) out[j].rssi = recs[i].rssi;
            continue;
        }
        if (count >= max) continue;
        strlcpy(out[count].ssid, (const char *)recs[i].ssid, sizeof(out[count].ssid));
        out[count].rssi = recs[i].rssi;
        out[count].secure = (recs[i].authmode != WIFI_AUTH_OPEN);
        count++;
    }
    free(recs);

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j].rssi > out[i].rssi) {
                webui_ap_t tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    ESP_LOGI(TAG, "WiFi scan: %d networks", count);
    return count;
}

void webui_wifi_set_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "pass", pass);
        nvs_commit(h);
        nvs_close(h);
    } else {
        ESP_LOGE(TAG, "wifi creds NVS open failed");
    }

    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    s_retry_count = 0;

    ESP_LOGI(TAG, "WiFi credentials set, connecting to '%s'", ssid);
    // If connected, disconnect fires the event handler which reconnects
    // with the new config; if idle/retrying, connect directly (the extra
    // call is harmless either way).
    esp_wifi_disconnect();
    esp_wifi_connect();
}

// Overwrite Kconfig defaults with NVS credentials if WiFi setup saved any
static void wifi_load_nvs_creds(wifi_config_t *wc)
{
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    char ssid[33], pass[65];
    size_t sl = sizeof(ssid), pl = sizeof(pass);
    if (nvs_get_str(h, "ssid", ssid, &sl) == ESP_OK && ssid[0]) {
        strlcpy((char *)wc->sta.ssid, ssid, sizeof(wc->sta.ssid));
        wc->sta.password[0] = '\0';
        if (nvs_get_str(h, "pass", pass, &pl) == ESP_OK) {
            strlcpy((char *)wc->sta.password, pass, sizeof(wc->sta.password));
        }
    }
    nvs_close(h);
}

void webui_init(void)
{
    // Local timezone for human-readable cook-log timestamps
    setenv("TZ", CONFIG_PELLETPIRATE_TZ, 1);
    tzset();

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "netif init failed"); return; }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop failed");
        return;
    }
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();  // for the SoftAP setup fallback

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
    wifi_load_nvs_creds(&wifi_config);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    err = esp_wifi_start();
    if (err != ESP_OK) { ESP_LOGE(TAG, "wifi start failed: %s", esp_err_to_name(err)); return; }

    // mDNS: reachable as pelletpirate.local (iOS/macOS reliably; Android
    // often can't resolve .local — the LCD Settings screen shows the IP)
    if (mdns_init() == ESP_OK) {
        mdns_hostname_set("pelletpirate");
        mdns_instance_name_set("PelletPirate Grill Controller");
        ESP_LOGI(TAG, "mDNS: pelletpirate.local");
    } else {
        ESP_LOGW(TAG, "mDNS init failed (IP access still works)");
    }

    xTaskCreate(ws_push_task, "ws_push", 4096, NULL, 4, NULL);

    // Raise the setup AP if we haven't connected within 25s of boot
    const esp_timer_create_args_t ap_timer_args = {
        .callback = ap_check_cb,
        .name = "ap_check",
    };
    esp_timer_handle_t ap_timer;
    if (esp_timer_create(&ap_timer_args, &ap_timer) == ESP_OK) {
        esp_timer_start_once(ap_timer, 25 * 1000000ULL);
    }

    ESP_LOGI(TAG, "WiFi station starting, SSID '%s'", (const char *)wifi_config.sta.ssid);
}
