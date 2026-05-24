#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "hx8357d.h"

static const char *TAG = "hx8357d";

// HX8357D command definitions
#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_PASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A
#define CMD_TEON    0x35
#define CMD_TEARLINE 0x44
#define CMD_SETC    0xB9
#define CMD_SETRGB  0xB3
#define CMD_SETCOM  0xB6
#define CMD_SETOSC  0xB0
#define CMD_SETPWR1 0xB1
#define CMD_SETCYC  0xB4
#define CMD_SETSTBA 0xC0
#define CMD_SETPANEL 0xCC
#define CMD_SETGAMMA 0xE0

// Init command table entry
typedef struct {
    uint8_t cmd;
    uint8_t data[34];
    uint8_t databytes;      // bit 7 set = delay after; 0xFF = end of table
} lcd_init_cmd_t;

// HX8357D init sequence from TFT_eSPI / Adafruit references
DRAM_ATTR static const lcd_init_cmd_t hx8357d_init_cmds[] = {
    {CMD_SWRESET, {0}, 0x80},
    {CMD_SETC, {0xFF, 0x83, 0x57}, 0x83},
    {CMD_SETRGB, {0x80, 0x00, 0x06, 0x06}, 4},
    {CMD_SETCOM, {0x25}, 1},
    {CMD_SETOSC, {0x68}, 1},
    {CMD_SETPANEL, {0x05}, 1},
    {CMD_SETPWR1, {0x00, 0x15, 0x1C, 0x1C, 0x83, 0xAA}, 6},
    {CMD_SETSTBA, {0x50, 0x50, 0x01, 0x3C, 0x1E, 0x08}, 6},
    {CMD_SETCYC, {0x02, 0x40, 0x00, 0x2A, 0x2A, 0x0D, 0x78}, 7},
    {CMD_SETGAMMA, {
        0x02, 0x0A, 0x11, 0x1D, 0x23, 0x35, 0x41, 0x4B,
        0x4B, 0x42, 0x3A, 0x27, 0x1B, 0x08, 0x09, 0x03,
        0x02, 0x0A, 0x11, 0x1D, 0x23, 0x35, 0x41, 0x4B,
        0x4B, 0x42, 0x3A, 0x27, 0x1B, 0x08, 0x09, 0x03,
        0x00, 0x01
    }, 34},
    {CMD_COLMOD, {0x55}, 1},
    {CMD_MADCTL, {0xC0}, 1},  // MY+MX (panel is natively BGR, no BGR bit needed)

    // Display inversion on (some panels need this for correct polarity)
    {0x21, {0}, 0},
    {CMD_TEON, {0x00}, 1},
    {CMD_TEARLINE, {0x00, 0x02}, 2},
    {CMD_SLPOUT, {0}, 0x80},
    {CMD_DISPON, {0}, 0x80},
    {0, {0}, 0xFF},
};

// Module state
static spi_device_handle_t s_spi;
static int s_pin_dc;
static int s_pin_rst;

// Number of lines per DMA chunk
#define PARALLEL_LINES 16
#define CHUNK_PIXELS (PARALLEL_LINES * HX8357D_WIDTH)

// Static DMA buffer — allocated once at init, never freed
static uint16_t *s_dma_buf = NULL;

// Pre-transfer callback: set DC pin for command (0) or data (1)
static void IRAM_ATTR spi_pre_transfer_cb(spi_transaction_t *t)
{
    gpio_set_level(s_pin_dc, (int)t->user);
}

static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = (void *)0,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void lcd_data(const uint8_t *data, int len)
{
    if (len == 0) return;
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .user = (void *)1,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void lcd_reset(void)
{
    gpio_set_level(s_pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(s_pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
}

esp_err_t hx8357d_init(const hx8357d_config_t *config)
{
    s_pin_dc = config->pin_dc;
    s_pin_rst = config->pin_rst;

    // Configure DC and RST as GPIO outputs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->pin_dc) | (1ULL << config->pin_rst),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->pin_mosi,
        .miso_io_num = config->pin_miso,
        .sclk_io_num = config->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CHUNK_PIXELS * 2 + 8,
    };
    esp_err_t ret = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Attach LCD device to SPI bus
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = config->spi_clock_speed_hz,
        .mode = 0,
        .spics_io_num = config->pin_cs,
        .queue_size = 7,
        .pre_cb = spi_pre_transfer_cb,
    };
    ret = spi_bus_add_device(config->spi_host, &devcfg, &s_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Allocate DMA buffer ONCE — reused for all fill/pixel operations
    s_dma_buf = heap_caps_malloc(CHUNK_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!s_dma_buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        return ESP_ERR_NO_MEM;
    }

    // Hardware reset
    lcd_reset();

    // Send init command sequence
    int i = 0;
    while (hx8357d_init_cmds[i].databytes != 0xFF) {
        lcd_cmd(hx8357d_init_cmds[i].cmd);
        lcd_data(hx8357d_init_cmds[i].data, hx8357d_init_cmds[i].databytes & 0x3F);
        if (hx8357d_init_cmds[i].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        i++;
    }

    ESP_LOGI(TAG, "HX8357D initialized (%dx%d)", HX8357D_WIDTH, HX8357D_HEIGHT);
    return ESP_OK;
}

void hx8357d_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_cmd(CMD_CASET);
    uint8_t ca_data[] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    lcd_data(ca_data, 4);

    lcd_cmd(CMD_PASET);
    uint8_t pa_data[] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    lcd_data(pa_data, 4);

    lcd_cmd(CMD_RAMWR);
}

void hx8357d_send_pixels(const uint16_t *data, size_t len)
{
    size_t remaining = len;
    const uint16_t *src = data;
    while (remaining > 0) {
        size_t n = (remaining > CHUNK_PIXELS) ? CHUNK_PIXELS : remaining;
        for (size_t i = 0; i < n; i++) {
            s_dma_buf[i] = (src[i] >> 8) | (src[i] << 8);
        }
        spi_transaction_t t = {
            .length = n * 16,
            .tx_buffer = s_dma_buf,
            .user = (void *)1,
        };
        spi_device_polling_transmit(s_spi, &t);
        src += n;
        remaining -= n;
    }
}

void hx8357d_fill_screen(uint16_t color)
{
    hx8357d_fill_rect(0, 0, HX8357D_WIDTH, HX8357D_HEIGHT, color);
}

void hx8357d_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    hx8357d_set_window(x, y, x + w - 1, y + h - 1);

    uint16_t swapped = (color >> 8) | (color << 8);
    for (size_t i = 0; i < CHUNK_PIXELS; i++) {
        s_dma_buf[i] = swapped;
    }

    size_t total_pixels = (size_t)w * h;
    while (total_pixels > 0) {
        size_t n = (total_pixels > CHUNK_PIXELS) ? CHUNK_PIXELS : total_pixels;
        spi_transaction_t t = {
            .length = n * 16,
            .tx_buffer = s_dma_buf,
            .user = (void *)1,
        };
        spi_device_polling_transmit(s_spi, &t);
        total_pixels -= n;
    }
}

void hx8357d_flush_area(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                        const uint8_t *color_data, size_t len_bytes)
{
    hx8357d_set_window(x1, y1, x2, y2);

    const size_t max_chunk = CHUNK_PIXELS * 2;
    size_t remaining = len_bytes;
    const uint8_t *ptr = color_data;

    while (remaining > 0) {
        size_t chunk = (remaining > max_chunk) ? max_chunk : remaining;
        spi_transaction_t t = {
            .length = chunk * 8,
            .tx_buffer = ptr,
            .user = (void *)1,
        };
        spi_device_polling_transmit(s_spi, &t);
        ptr += chunk;
        remaining -= chunk;
    }
}
