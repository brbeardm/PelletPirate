#ifndef WEBUI_H
#define WEBUI_H

/**
 * Local WiFi dashboard (see wifi-dashboard-spec.md).
 *
 * Starts WiFi station mode — credentials from NVS if the LCD WiFi setup
 * has saved any, otherwise Kconfig (PELLETPIRATE_WIFI_SSID / _PASSWORD) —
 * and serves the dashboard over esp_http_server once an IP is obtained.
 * Fully asynchronous: WiFi or server failure never blocks boot or grill
 * operation. The web layer reads and writes grill_state only — it has no
 * access to actuators.
 */

#include <stdbool.h>

void webui_init(void);

// --- WiFi setup support (LCD Settings > WI-FI SETUP) ---

typedef struct {
    char ssid[33];
    int rssi;
    bool secure;   // any auth mode other than open
} webui_ap_t;

/**
 * Blocking scan (~2s — call from a worker task, not the LVGL task).
 * Results dedup'd by SSID keeping the strongest, sorted by RSSI desc.
 * Returns the count, or -1 on failure.
 */
int webui_wifi_scan(webui_ap_t *out, int max);

/**
 * Persist new credentials to NVS and reconnect with them. Connection
 * progress is visible in grill_state (wifi_connected / wifi_ip).
 */
void webui_wifi_set_credentials(const char *ssid, const char *pass);

#endif
