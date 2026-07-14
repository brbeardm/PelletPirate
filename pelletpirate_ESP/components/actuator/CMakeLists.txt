idf_component_register(
    SRCS "actuator.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio esp_timer pid grill_state
)
