idf_component_register(
    SRCS "backlight.c"
    INCLUDE_DIRS "include"
    REQUIRES boardpins driver esp_driver_ledc nvs_flash
)
