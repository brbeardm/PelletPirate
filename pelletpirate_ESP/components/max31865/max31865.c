// MAX31865 RTD-to-digital converter driver
// Ported from PelletPirateC++_WB/src/MAX31865.cpp (Particle Photon) —
// register logic and Callendar-Van Dusen temperature math unchanged.
//
// V2 board: 5x MAX31865 on the shared SPI2 bus (with the LCD), 400 ohm
// reference resistor (R21 etc.), PT100 probes on TRS jacks wired 3-wire
// (Tip=RTDIN+, Ring=FORCE, Sleeve=return).
//
// ESP32's SPI driver allows only 3 attached devices per bus and the LCD
// takes one, so all five MAX31865s share ONE CS-less SPI device handle
// (mode 3, 2 MHz) and their chip selects are driven manually as GPIOs.
// The bus is acquired around each manual-CS transaction so LCD/LVGL
// transfers can never interleave inside a CS window.

#include "max31865.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "max31865";

// Callendar-Van Dusen coefficients for PT100 (same source as Photon code)
static const float CVD_A = 0.00390830f;
static const float CVD_B = -0.0000005775f;

// Config register values (Photon code used the same)
#define CONFIG_RUN    0xD0   // Vbias on, auto conversion, 3-wire RTD
#define CONFIG_CLRFLT 0x82   // Vbias on, clear fault status

// One shared CS-less device for all MAX31865s on the bus
static spi_device_handle_t s_spi = NULL;

static esp_err_t max_xfer(max31865_handle_t *h, const uint8_t *tx, uint8_t *rx, int len)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_acquire_bus(h->spi, portMAX_DELAY);
    if (err != ESP_OK) return err;
    gpio_set_level(h->pin_cs, 0);
    err = spi_device_polling_transmit(h->spi, &t);
    gpio_set_level(h->pin_cs, 1);
    spi_device_release_bus(h->spi);
    return err;
}

static void write_reg(max31865_handle_t *h, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { MAX31865_WR(reg), val };
    max_xfer(h, tx, NULL, 2);
}

static uint8_t read_reg(max31865_handle_t *h, uint8_t reg)
{
    uint8_t tx[2] = { reg, 0x00 };
    uint8_t rx[2] = { 0 };
    max_xfer(h, tx, rx, 2);
    return rx[1];
}

esp_err_t max31865_init(max31865_handle_t *handle, const max31865_config_t *config)
{
    handle->ref_resistor = config->ref_resistor;
    handle->rtd_nominal = config->rtd_nominal;
    handle->pin_cs = config->pin_cs;
    handle->initialized = false;

    // CS as plain GPIO output, idle high
    gpio_config_t cs_conf = {
        .pin_bit_mask = 1ULL << config->pin_cs,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_conf);
    gpio_set_level(config->pin_cs, 1);

    // Add the shared CS-less device on first init
    if (s_spi == NULL) {
        spi_device_interface_config_t devcfg = {
            .clock_speed_hz = 2 * 1000 * 1000,   // MAX31865 max is 5 MHz
            .mode = 3,                            // CPOL=1 CPHA=1 per datasheet
            .spics_io_num = -1,                   // CS driven manually
            .queue_size = 1,
        };
        esp_err_t err = spi_bus_add_device(config->spi_host, &devcfg, &s_spi);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
            return err;
        }
    }
    handle->spi = s_spi;

    // Same bring-up sequence as the Photon code: write run config, read it
    // back to verify communication, then set fault thresholds wide open.
    write_reg(handle, MAX31865_REG_CONFIG, CONFIG_RUN);
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t cfg = read_reg(handle, MAX31865_REG_CONFIG);
    if (cfg != CONFIG_RUN) {
        ESP_LOGE(TAG, "CS %d: config readback 0x%02X (expected 0x%02X) — no comms",
                 handle->pin_cs, cfg, CONFIG_RUN);
        return ESP_FAIL;
    }

    write_reg(handle, MAX31865_REG_HIGH_FAULT_MSB, 0xFF);
    write_reg(handle, MAX31865_REG_HIGH_FAULT_LSB, 0xFF);
    write_reg(handle, MAX31865_REG_LOW_FAULT_MSB, 0x00);
    write_reg(handle, MAX31865_REG_LOW_FAULT_LSB, 0x00);

    handle->initialized = true;
    ESP_LOGI(TAG, "CS %d: MAX31865 initialized (config 0x%02X)", handle->pin_cs, cfg);
    return ESP_OK;
}

float max31865_get_temp_f(max31865_handle_t *handle)
{
    if (!handle->initialized) return 0.0f;

    uint8_t fault = read_reg(handle, MAX31865_REG_FAULT_STATUS);
    if (fault != 0) {
        max31865_check_fault(handle);
        // Clear the fault and restore run config (register doesn't
        // self-clear; unplugged probe re-faults on the next cycle)
        write_reg(handle, MAX31865_REG_CONFIG, CONFIG_CLRFLT);
        vTaskDelay(pdMS_TO_TICKS(10));
        write_reg(handle, MAX31865_REG_CONFIG, CONFIG_RUN);
        return 0.0f;
    }

    uint8_t lsb = read_reg(handle, MAX31865_REG_RTD_LSB);
    if (lsb & 0x01) {
        // LSB fault bit set — treat as fault, detail comes next cycle
        return 0.0f;
    }
    uint8_t msb = read_reg(handle, MAX31865_REG_RTD_MSB);

    // 15-bit RTD code -> resistance -> temperature (math identical to Photon)
    float rtd_code = (float)(((uint16_t)msb << 7) | (lsb >> 1));
    float r = (rtd_code * handle->ref_resistor) / 32768.0f;
    float r0 = handle->rtd_nominal;
    float temp_c = (-r0 * CVD_A +
                    sqrtf(r0 * r0 * CVD_A * CVD_A - 4.0f * r0 * CVD_B * (r0 - r))) /
                   (2.0f * r0 * CVD_B);
    float temp_f = temp_c * 9.0f / 5.0f + 32.0f;

    // An unplugged probe can occasionally read a raw code of 0 without the
    // fault bit set, computing to ~-410F. Clamp anything implausible to the
    // 0.0 fault convention (Photon code did the same with a 0..500 window).
    if (temp_f < -40.0f || temp_f > 600.0f) return 0.0f;
    return temp_f;
}

void max31865_check_fault(max31865_handle_t *handle)
{
    uint8_t fault = read_reg(handle, MAX31865_REG_FAULT_STATUS);
    if (fault == 0) return;

    if (fault & 0x80) ESP_LOGW(TAG, "CS %d: D7 — RTD high threshold (probe unplugged?)", handle->pin_cs);
    if (fault & 0x40) ESP_LOGW(TAG, "CS %d: D6 — RTD low threshold (probe shorted?)", handle->pin_cs);
    if (fault & 0x20) ESP_LOGW(TAG, "CS %d: D5 — REFIN- > 0.85*Vbias", handle->pin_cs);
    if (fault & 0x10) ESP_LOGW(TAG, "CS %d: D4 — REFIN- < 0.85*Vbias (FORCE- open)", handle->pin_cs);
    if (fault & 0x08) ESP_LOGW(TAG, "CS %d: D3 — RTDIN- < 0.85*Vbias (FORCE- open)", handle->pin_cs);
    if (fault & 0x04) ESP_LOGW(TAG, "CS %d: D2 — over/undervoltage", handle->pin_cs);
}
