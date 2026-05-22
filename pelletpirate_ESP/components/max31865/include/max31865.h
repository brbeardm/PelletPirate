#ifndef MAX31865_H
#define MAX31865_H

#include "esp_err.h"
#include "driver/spi_master.h"

// SPI timeout between reads (ms) - was 70 in original Particle code
#define MAX31865_TIMEOUT_MS 70

// Register addresses (read)
#define MAX31865_REG_CONFIG          0x00
#define MAX31865_REG_RTD_MSB         0x01
#define MAX31865_REG_RTD_LSB         0x02
#define MAX31865_REG_HIGH_FAULT_MSB  0x03
#define MAX31865_REG_HIGH_FAULT_LSB  0x04
#define MAX31865_REG_LOW_FAULT_MSB   0x05
#define MAX31865_REG_LOW_FAULT_LSB   0x06
#define MAX31865_REG_FAULT_STATUS    0x07

// Write address = read address | 0x80
#define MAX31865_WR(reg) ((reg) | 0x80)

// Ported from PelletPirateC++_WB/src/MAX31865.cpp (Particle Photon)
// Changes:
//   - Particle SPI -> ESP-IDF spi_device_transmit()
//   - Particle.publish() -> ESP_LOGI/ESP_LOGE
//   - Constructor takes config struct instead of raw CS pin
//   - CS managed by ESP-IDF SPI driver (not manual GPIO)
//   - Temperature math and register logic unchanged

typedef struct {
    spi_host_device_t spi_host;     // Must match the host used by LCD (SPI2_HOST)
    int pin_cs;                     // Chip select GPIO for this MAX31865
    float ref_resistor;             // Reference resistor on the board (typically 400 ohm)
    float rtd_nominal;              // RTD resistance at 0C (typically 100 ohm for PT100)
} max31865_config_t;

typedef struct {
    spi_device_handle_t spi;
    float ref_resistor;
    float rtd_nominal;
    bool initialized;
} max31865_handle_t;

/**
 * Initialize a MAX31865 device on an already-initialized SPI bus.
 * The SPI bus must be initialized before calling this (hx8357d_init does this).
 */
esp_err_t max31865_init(max31865_handle_t *handle, const max31865_config_t *config);

/**
 * Read temperature in Fahrenheit from the RTD probe.
 * Returns 0.0 on fault (fault is logged).
 */
float max31865_get_temp_f(max31865_handle_t *handle);

/**
 * Read and report any fault conditions.
 */
void max31865_check_fault(max31865_handle_t *handle);

#endif
