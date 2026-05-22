#ifndef HX8357D_H
#define HX8357D_H

#include "esp_err.h"
#include "driver/spi_master.h"

#define HX8357D_WIDTH  320
#define HX8357D_HEIGHT 480

// RGB565 color helpers
#define HX8357D_RGB565(r, g, b) \
    ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define HX8357D_BLACK   0x0000
#define HX8357D_RED     0xF800
#define HX8357D_GREEN   0x07E0
#define HX8357D_BLUE    0x001F
#define HX8357D_WHITE   0xFFFF
#define HX8357D_YELLOW  0xFFE0
#define HX8357D_CYAN    0x07FF
#define HX8357D_MAGENTA 0xF81F

typedef struct {
    spi_host_device_t spi_host;
    int pin_mosi;
    int pin_sclk;
    int pin_miso;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int spi_clock_speed_hz;
} hx8357d_config_t;

/**
 * Initialize the SPI bus and HX8357D display.
 * Call once at startup.
 */
esp_err_t hx8357d_init(const hx8357d_config_t *config);

/**
 * Fill the entire screen with a single RGB565 color.
 */
void hx8357d_fill_screen(uint16_t color);

/**
 * Set the active drawing window. Subsequent pixel data
 * writes will fill this rectangle left-to-right, top-to-bottom.
 */
void hx8357d_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * Send a buffer of RGB565 pixel data to the display.
 * Must call hx8357d_set_window() first to define the target region.
 * len is the number of pixels (not bytes).
 */
void hx8357d_send_pixels(const uint16_t *data, size_t len);

/**
 * Fill a rectangular region with a single color.
 */
void hx8357d_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * Send raw pixel data to a window region (no byte-swapping).
 * Used by LVGL flush callback — LVGL handles byte order via LV_COLOR_16_SWAP.
 * Sets window, then sends raw bytes over SPI.
 */
void hx8357d_flush_area(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                        const uint8_t *color_data, size_t len_bytes);

/**
 * Show boot splash screen with "PelletPirate V2" and "Loading..."
 * No dependencies — draws directly with the HX8357D driver.
 */
void hx8357d_boot_splash(void);

#endif
