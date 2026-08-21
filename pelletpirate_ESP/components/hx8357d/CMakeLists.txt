idf_component_register(
    SRCS "hx8357d.c" "boot_splash.c"
    INCLUDE_DIRS "include"
    REQUIRES boardpins driver esp_driver_spi esp_driver_gpio
)
