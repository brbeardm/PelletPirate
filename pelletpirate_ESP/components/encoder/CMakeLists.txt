idf_component_register(
    SRCS "encoder.c"
    INCLUDE_DIRS "include"
    REQUIRES boardpins driver esp_driver_gpio esp_driver_pcnt esp_timer
)

