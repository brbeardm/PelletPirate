idf_component_register(
    SRCS "cookprog.c"
    INCLUDE_DIRS "include"
    REQUIRES grill_state cooklog nvs_flash esp_timer
)
