idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES hx8357d backlight encoder grill_state max31865 ui driver esp_timer
)
