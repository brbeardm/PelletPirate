idf_component_register(
    SRCS "grill_state.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer nvs_flash
)
