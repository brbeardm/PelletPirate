#ifndef WEBUI_H
#define WEBUI_H

/**
 * Local WiFi dashboard (see wifi-dashboard-spec.md).
 *
 * Starts WiFi station mode with credentials from Kconfig
 * (PELLETPIRATE_WIFI_SSID / _PASSWORD) and serves the dashboard over
 * esp_http_server once an IP is obtained. Fully asynchronous: WiFi or
 * server failure never blocks boot or grill operation. The web layer
 * reads and writes grill_state only — it has no access to actuators.
 */
void webui_init(void);

#endif
