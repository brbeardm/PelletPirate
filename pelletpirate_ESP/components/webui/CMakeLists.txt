idf_component_register(
    SRCS "webui.c"
    INCLUDE_DIRS "include"
    EMBED_FILES "index.html"
    REQUIRES esp_wifi esp_netif esp_event esp_http_server nvs_flash grill_state cooklog lwip
)
