idf_component_register(
    SRCS "max31865.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_spi esp_driver_gpio
)
