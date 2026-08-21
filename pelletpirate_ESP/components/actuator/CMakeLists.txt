idf_component_register(
    SRCS "actuator.cpp"
    INCLUDE_DIRS "include"
    REQUIRES boardpins driver esp_driver_gpio esp_timer pid grill_state cooklog esp_system
)
