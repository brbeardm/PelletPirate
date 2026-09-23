idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_adc esp_wifi esp_event esp_netif nvs_flash lwip esp_http_server)
