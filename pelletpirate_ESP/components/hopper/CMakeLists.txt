idf_component_register(
    SRCS "hopper.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc grill_state nvs_flash boardpins
)
