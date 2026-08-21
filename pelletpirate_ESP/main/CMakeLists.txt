idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES boardpins hx8357d backlight encoder grill_state max31865 actuator webui cooklog cookprog ui driver esp_timer app_update
)

