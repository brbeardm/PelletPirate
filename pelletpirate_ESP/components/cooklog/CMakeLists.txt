idf_component_register(
    SRCS "cooklog.c" "profiles.c"
    INCLUDE_DIRS "include"
    REQUIRES grill_state nvs_flash esp_timer
)

