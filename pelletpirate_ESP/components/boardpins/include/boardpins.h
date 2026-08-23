#ifndef BOARDPINS_H
#define BOARDPINS_H

// Single source of truth for GPIO assignments on both PelletPirate boards.
//
//   esp32   -> V2 board: ESP32-DevKitC-32E socketed on two 19-pin rails
//   esp32s3 -> V4 board: ESP32-S3-WROOM-1U-N16R8 soldered on the PCB
//
// V4 map extracted from the PelletPirate_V4 netlist 2026-08-21 and verified
// identical to the archived PCBWay fab zip (2026-07-28) the boards were
// built from. Module pad -> GPIO per the ESP32-S3-WROOM-1 datasheet.
//
// Anything that drives a triac opto or the TPS61165 backlight CTRL must be
// parked LOW at the top of app_main() (see boot_low_pins there).

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3

// ---- V4 board ----
// Shared SPI bus (LCD + 5x MAX31865). Net names are peripheral-relative:
// /ALL_SDI = data INTO peripherals = ESP MOSI; /ALL_SDO = ESP MISO.
#define BOARD_SPI_MOSI      39  // /ALL_SDI
#define BOARD_SPI_SCLK      40  // /ALL_SCLK
#define BOARD_SPI_MISO      41  // /ALL_SDO

#define BOARD_LCD_CS        16
#define BOARD_LCD_DC        18
#define BOARD_LCD_RST       15

// The enclosure mounts the LCD upside down relative to the original V2
// case: MY+MX cleared rotates the output 180 degrees (panel is natively
// BGR on both boards). V2 boards adopted this same orientation 2026-08-23.
#define BOARD_LCD_MADCTL    0x00

// MAX31865 chip selects indexed by FRONT-PANEL jack label (P1-P5, the
// product language everywhere in firmware). The V4 enclosure mounts the
// board flipped, so panel order is the REVERSE of PCB refs: P1=J5 ... P5=J1.
#define BOARD_JACK_P1_CS    48  // J5 / MAX5
#define BOARD_JACK_P2_CS    47  // J4 / MAX4
#define BOARD_JACK_P3_CS    21  // J3 / MAX3
#define BOARD_JACK_P4_CS    12  // J2 / MAX2
#define BOARD_JACK_P5_CS     4  // J1 / MAX1

#define BOARD_ENCODER_A      5  // 10k pullups on board (R32-R34)
#define BOARD_ENCODER_B      6
#define BOARD_ENCODER_BTN    7

// IO3 is an S3 strapping pin (JTAG select). Floating at boot it cannot
// source the ~9mA the MOC LED needs, so no spurious fire — but it still
// goes in boot_low_pins like everything else.
#define BOARD_IGNITER_GPIO   3
#define BOARD_AUGER_GPIO     9
#define BOARD_FAN_GPIO      10  // MOC3053M random-phase (V2 was MOC3063M)

#define BOARD_BACKLIGHT_GPIO 17 // TPS61165 CTRL — same GPIO as V2; R16 is
                                // now a 10k pulldown (was 100k)

// IO2 is an unconnected module pad on V4 — heartbeat toggles harmlessly.
#define BOARD_HEARTBEAT_GPIO 2

// V4-only hardware (no V2 equivalent):
#define BOARD_ZC_DET_GPIO    8  // H11AA1M zero-cross, 120Hz pulses, 10k
                                // pullup R43 + 1nF C42 (C42 sits ~48mm
                                // away by U3 — scope at the pin during AC
                                // bring-up, bodge 1nF at pin if jittery)
#define BOARD_HOPPER_GPIO   14  // J9 pellet-low switch via 100R; 10k
                                // pullup at J9.1. Pellets present = switch
                                // held closed = LOW; open/HIGH = low pellets

#elif CONFIG_IDF_TARGET_ESP32

// ---- V2 board ---- (user-verified map, 2026-05-12)
#define BOARD_SPI_MOSI      23
#define BOARD_SPI_SCLK      18
#define BOARD_SPI_MISO      19

#define BOARD_LCD_CS        25
#define BOARD_LCD_DC        14
#define BOARD_LCD_RST       16

// V2 boards moved to the V4-style enclosure orientation 2026-08-23:
// LCD flipped 180 (MY+MX cleared) same as V4. Original V2 case value
// was 0xC0.
#define BOARD_LCD_MADCTL    0x00

// Same enclosure flip reverses the front-panel jack order vs PCB refs,
// matching V4: P1=J5 ... P5=J1. (Original V2 case: P1=J1 ... P5=J5.)
#define BOARD_JACK_P1_CS    21  // J5 / MAX5
#define BOARD_JACK_P2_CS    26  // J4 / MAX4
#define BOARD_JACK_P3_CS     5  // J3 / MAX3
#define BOARD_JACK_P4_CS    13  // J2 / MAX2
#define BOARD_JACK_P5_CS    27  // J1 / MAX1

#define BOARD_ENCODER_A     34  // input-only GPIOs; pullups on board
#define BOARD_ENCODER_B     35
#define BOARD_ENCODER_BTN   36

#define BOARD_IGNITER_GPIO  32
#define BOARD_AUGER_GPIO    33
#define BOARD_FAN_GPIO       4

#define BOARD_BACKLIGHT_GPIO 17

#define BOARD_HEARTBEAT_GPIO 2  // DevKit blue LED (external LED on bench)

#else
#error "boardpins.h: unsupported IDF target (expected esp32 = V2 or esp32s3 = V4)"
#endif

#endif // BOARDPINS_H
