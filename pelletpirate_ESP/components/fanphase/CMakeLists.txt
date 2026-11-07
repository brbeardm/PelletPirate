idf_component_register(
    SRCS "fanphase.c"
    INCLUDE_DIRS "include"
    REQUIRES boardpins esp_driver_gpio esp_driver_gptimer esp_timer
)
