// MAX31865 RTD temperature sensor driver
// Ported from PelletPirateC++_WB/src/MAX31865.cpp (Particle Photon)
//
// Original used Particle SPI class with manual CS toggling.
// ESP-IDF SPI driver handles CS automatically per transaction.
//
// Temperature math unchanged: Callendar-Van Dusen linearization
// R(T) = R0(1 + aT + bT^2 + c(T-100)T^3)

#include "max31865.h"
#include <cmath>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "max31865";

// Callendar-Van Dusen coefficients (same as original)
static const float CVD_A = 0.00390830f;
static const float CVD_B = -0.0000005775f;

// Configuration register value: Vbias on, auto-conversion, 3-wire
// Original used 0xD0 = 0b11010000
static const uint8_t CONFIG_VALUE = 0xD0;

// Low-level SPI read/write using ESP-IDF transactions
static uint8_t spi_read_reg(spi_device_handle_t spi, uint8_t reg)
{
    uint8_t tx[2] = { reg, 0x00 };  // send register addr, then dummy byte for clock
    uint8_t rx[2] = { 0 };

    spi_transaction_t t = {};
    t.length = 16;          // 2 bytes
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    spi_device_polling_transmit(spi, &t);

    return rx[1];  // first byte is junk (sent during address phase)
}

static void spi_write_reg(spi_device_handle_t spi, uint8_t reg, uint8_t data)
{
    uint8_t tx[2] = { (uint8_t)(reg | 0x80), data };

    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = tx;
    spi_device_polling_transmit(spi, &t);
}

esp_err_t max31865_init(max31865_handle_t *handle, const max31865_config_t *config)
{
    handle->ref_resistor = config->ref_resistor;
    handle->rtd_nominal = config->rtd_nominal;
    handle->initialized = false;

    // Add this MAX31865 as a device on the shared SPI bus
    // MAX31865 uses SPI mode 1 or 3; mode 3 matches the original Particle code
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 1 * 1000 * 1000;   // 1 MHz (MAX31865 max is 5 MHz)
    devcfg.mode = 3;                            // SPI mode 3 (CPOL=1, CPHA=1)
    devcfg.spics_io_num = config->pin_cs;
    devcfg.queue_size = 1;

    esp_err_t ret = spi_bus_add_device(config->spi_host, &devcfg, &handle->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add MAX31865 (CS=%d): %s", config->pin_cs, esp_err_to_name(ret));
        return ret;
    }

    // Write configuration: Vbias on, auto-conversion, 3-wire RTD
    spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_CONFIG), CONFIG_VALUE);

    // Verify config was written correctly
    uint8_t readback = spi_read_reg(handle->spi, MAX31865_REG_CONFIG);
    if (readback == CONFIG_VALUE) {
        // Set fault thresholds (same as original: high=0xFFFF, low=0x0000)
        spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_HIGH_FAULT_MSB), 0xFF);
        spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_HIGH_FAULT_LSB), 0xFF);
        spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_LOW_FAULT_MSB), 0x00);
        spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_LOW_FAULT_LSB), 0x00);
        handle->initialized = true;
        ESP_LOGI(TAG, "MAX31865 (CS=%d) initialized, config=0x%02X", config->pin_cs, readback);
    } else {
        ESP_LOGE(TAG, "MAX31865 (CS=%d) config verify failed: expected 0x%02X, got 0x%02X",
                 config->pin_cs, CONFIG_VALUE, readback);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

float max31865_get_temp_f(max31865_handle_t *handle)
{
    if (!handle->initialized) {
        ESP_LOGE(TAG, "MAX31865 not initialized");
        return 0.0f;
    }

    // Check for fault first
    uint8_t fault = spi_read_reg(handle->spi, MAX31865_REG_FAULT_STATUS);
    if (fault != 0) {
        ESP_LOGE(TAG, "Fault detected: 0x%02X", fault);
        max31865_check_fault(handle);

        // Clear fault and re-enable
        spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_CONFIG), 0x82);  // fault clear
        vTaskDelay(pdMS_TO_TICKS(700));
        spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_CONFIG), CONFIG_VALUE);
        vTaskDelay(pdMS_TO_TICKS(700));
        return 0.0f;
    }

    // Trigger a one-shot conversion
    spi_write_reg(handle->spi, MAX31865_WR(MAX31865_REG_CONFIG), CONFIG_VALUE);

    // Read RTD registers
    uint8_t lsb = spi_read_reg(handle->spi, MAX31865_REG_RTD_LSB);
    uint8_t fault_bit = lsb & 0x01;

    if (fault_bit != 0) {
        ESP_LOGW(TAG, "RTD LSB fault bit set");
        return 0.0f;
    }

    uint8_t msb = spi_read_reg(handle->spi, MAX31865_REG_RTD_MSB);

    // Combine MSB and LSB (strip fault bit from LSB)
    // Same math as original: RTD = (msb << 7) + ((lsb & 0xFE) >> 1)
    float RTD = (float)((msb << 7) + ((lsb & 0xFE) >> 1));

    // Convert ADC code to resistance
    float R = (RTD * handle->ref_resistor) / 32768.0f;

    // Callendar-Van Dusen: solve for temperature
    // Same formula as original MAX31865.cpp
    float R0 = handle->rtd_nominal;
    float Temp = -R0 * CVD_A + sqrtf(R0 * R0 * CVD_A * CVD_A - 4.0f * R0 * CVD_B * (R0 - R));
    Temp = Temp / (2.0f * R0 * CVD_B);

    // Convert Celsius to Fahrenheit (same as original)
    float Tempf = Temp * 9.0f / 5.0f + 32.0f;

    return Tempf;
}

void max31865_check_fault(max31865_handle_t *handle)
{
    uint8_t fault = spi_read_reg(handle->spi, MAX31865_REG_FAULT_STATUS);

    // Same fault bit checks as original MAX31865::Fault()
    if (fault & 0x80) {
        ESP_LOGE(TAG, "D7: RTD disconnected from RTD+ or RTD- (High Fault Threshold)");
    }
    if (fault & 0x40) {
        ESP_LOGE(TAG, "D6: RTD+ and RTD- shorted (Low Fault Threshold)");
    }
    if (fault & 0x20) {
        ESP_LOGE(TAG, "D5: Vref- > 0.85 * Vbias");
    }
    if (fault & 0x10) {
        ESP_LOGE(TAG, "D4: Vref- < 0.85 * Vbias (FORCE- open)");
    }
    if (fault & 0x08) {
        ESP_LOGE(TAG, "D3: RTDIN- < 0.85 * Vbias (FORCE- open)");
    }
    if (fault & 0x04) {
        ESP_LOGE(TAG, "D2: Overvoltage/undervoltage fault");
    }
}
