idf_component_register(
    SRCS "ui.c" "ui_styles.c" "ui_splash.c" "ui_main_menu.c"
         "ui_set_target.c" "ui_cook_mode.c" "ui_ignite.c"
         "ui_set_probes.c" "ui_dashboard.c" "ui_settings.c" "ui_profiles.c" "logo_img.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl hx8357d backlight encoder grill_state cooklog esp_timer
)


