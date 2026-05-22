idf_component_register(
    SRCS "ui.c" "ui_styles.c" "ui_splash.c" "ui_menu.c" "ui_home.c" "logo_img.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl hx8357d encoder grill_state esp_timer
)
