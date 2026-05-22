// PelletPirate logo — converted by LCD Image Converter
// RGB565, 300x448

#include "lvgl.h"
#include <stdint.h>

// Pull in just the pixel data array (not the tImage struct at the end)
// The file defines: static const uint32_t image_data_pellet_piratelogo320_480[67200]
typedef struct { const uint32_t *data; uint16_t width; uint16_t height; uint8_t dataSize; } tImage;
#include "../../images/pellet_pirate-logo-320_480.c"

const lv_image_dsc_t logo_pelletpirate = {
    .header = {
        .w = 300,
        .h = 448,
        .cf = LV_COLOR_FORMAT_RGB565,
    },
    .data_size = 300 * 448 * 2,
    .data = (const uint8_t *)image_data_pellet_piratelogo320_480,
};
