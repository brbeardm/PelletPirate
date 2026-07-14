# PelletPirate V2 Hardware Bring-Up Context (July 11-13, 2026)

Merge this into the ESP32 UI/LCD/Encoder thread. It summarizes two days of
voltmeter diagnostics on the three PCBWay-assembled V2 boards and defines
firmware requirements that came out of the hardware findings.

## Board Overview (from KiCad schematic, PelletPirate_V2_PCBWay_2026-05-08)

- ESP32 DevKit-C seated in two 19-pin headers (ESP_J2, ESP_J3)
- Mains front end: fuse, MOV, line filter into PBO-5F-12 AC/DC module (12V)
- PS1 = 12V rail, PS2 = 3.3V 2A buck, PS3 = TPS61165 boost LED backlight driver
- 5x MAX31865 RTD converters on shared SPI (individual CS lines)
- 3x triac outputs (igniter, auger, fan) via MOC3063 optotriacs
- LCD on 39-pin Hirose FH26W FPC connector (FPC1), rotary encoder for UI

## GPIO Map (verified against schematic netlist and DevKit-C positions)

| Signal    | ESP32 GPIO | Header pin | FPC1 pin | Notes                          |
|-----------|-----------|------------|----------|--------------------------------|
| LCD_CS    | IO25      | J2_9       | 28       | display chip select            |
| LCD_DC    | IO14      | J2_12      | 31       | data/command                   |
| LCD_RST   | IO16      | J3_12      | 33       | display reset                  |
| LED_PWM   | IO17      | J3_11      | (CTRL)   | TPS61165 backlight enable/dim  |
| ALL_SCLK  | IO18      | J3_9       | 32       | shared SPI clock (LCD + MAXes) |
| ALL_SDI   | IO23      | J3_2       | 29       | shared MOSI                    |
| MAX_SDO   | IO19      | J3_8       | 30       | shared MISO                    |
| Heartbeat | IO2       | J3_15      | n/a      | DevKit onboard blue LED only;  |
|           |           |            |          | unconnected on the PCB         |

Known schematic nit: the ESP_J3 symbol labels both J3_8 and J3_9 as "IO19";
positionally J3_9 is IO18 and the SCLK wiring is correct. Symbol text typo only.

FPC1 other pins: 1 = LED- (backlight cathode, to TPS61165 FB via 4.99R sense),
2 = LED+ (backlight anode, boost output), 3/34/35 = 3.3V, 4-26/36/shields = GND,
27/37/38/39 = no connect. Backlight current is fixed at ~40mA (200mV/4.99R).

## Hardware Validation Status

- Board 1: PS1 good (12.35V), PS2 good (3.38V). DEFECT: with no ESP seated,
  the TPS61165 CTRL node floats to ~1.01V despite R16 (100k pulldown, verified
  present, 100.1k), ghost-enabling the boost to a solid 37.9V open-load
  (right at the 37-39V OVP window). ~11uA is leaking into CTRL from an
  unknown board-specific source (suspects: flux residue, micro-bridge between
  CTRL pin 2 and SW pin 3 on the SOT-23-6, or leaky IC). Pending: MOhm
  resistance check to 12V/LED+, IPA clean, retest. Board is likely usable
  once firmware drives IO17, but defect is being chased down.
- Board 2: fully healthy. 0V at CTRL, ~12.3V at LED+ (correct shutdown state).
  This is the golden reference board and the FPC1 validation target.
- Board 3: fully healthy. 99.98k pulldown, 3.33V rail, 12.01V at LED+, 0V CTRL.
- Bench power plan: mains validated once, now retired for debug. 12V DC is
  injected at C15's through-hole leads (positive = +12V) or D1's pads
  (SMBJ20A TVS across the rail: pin 1 = +12V, pin 2 = GND). AC cord physically
  unplugged whenever DC is injected. Supply 12.0-12.4V, current limit 200mA
  idle / 1A with ESP + backlight. Back-feeding the PBO-5F-12 output is safe.
- Next hardware step: FPC1 per-pin validation on board 2 using an FPC breakout
  (Phase A continuity unpowered, Phase B rail voltages powered, Phase C
  dynamic GPIO toggles with ESP seated). Display panel datasheet still needs
  cross-checking (LED+/- orientation on pins 2/1, and function of NC pins
  27/37/38/39 on the panel side, e.g. interface-select lines).

## Firmware Requirements Derived From Hardware Findings (ESP-IDF)

1. FIRST statements in app_main(): configure GPIO_NUM_17 as push-pull output,
   gpio_set_level(GPIO_NUM_17, 0). IO17 is the TPS61165 CTRL pin; if it
   floats, the backlight boost can self-enable to ~38V open-load (proven on
   board 1). Hardware pulldown R16 covers the bootloader window; firmware
   must own the pin from app_main() onward. Never leave IO17 floating or
   toggle it incidentally.
2. Heartbeat: GPIO_NUM_2 as output, dedicated low-priority FreeRTOS task
   toggling at 1 Hz via vTaskDelay(pdMS_TO_TICKS(500)). Bench alive-indicator
   only (DevKit onboard blue LED). Note: the DevKit red power LED never
   lights when powered through the 3V3 pin (5V rail unused by design);
   the heartbeat is the power/boot confirmation.
3. Backlight control: TPS61165 CTRL accepts PWM dimming only in the
   5 kHz - 100 kHz range; use LEDC on GPIO17 within that band. CTRL low
   > 2.5 ms = device shutdown, so slow PWM will cause shutdown cycling.
4. Open-LED behavior: if the boost hits OVP with FB near zero (display
   unplugged or backlight open), the chip latches OFF until CTRL is toggled.
   Backlight-on code should account for this (toggle CTRL to retry).
5. Never hot-plug the FPC display; with the driver enabled and no load,
   LED+ can sit near 38V.
6. Bring-up test hooks worth keeping in a hardware-test build: slow-toggle
   IO25/IO14/IO16 (visible at FPC pins 28/31/33), GPIO toggle IO18/IO23
   (FPC 32/29), and a MISO loopback test (jumper FPC 30 to 29, drive IO23,
   read IO19, report over serial).
