# PelletPirate V5 — boardpins.h Delta (firmware handoff)
Source: extracted from the V5 netlist during the 2026-08-27 pre-fab audit (commit cb08bea). Authoritative — not hand-copied from the schematic.

## Headline change vs V4: probe jacks renumbered to FRONT-PANEL order
The V2/V4-era enclosure reversal (front panel P1 = board J5 ... P5 = J1) is **eliminated in copper**.
On V5: **P1..P5 = J1..J5 = MAX1..MAX5**, left-to-right as the user sees them. Any firmware table that
re-ordered jacks for display (the V4-era P1=J5 mapping) must NOT be applied to V5.

## RTD chip-selects (the only GPIO deltas vs V4)
| Probe/Jack | MAX | V5 GPIO | V4 GPIO (for reference) |
|---|---|---|---|
| P1 / J1 | MAX1 | **IO4**  | IO4 (same) |
| P2 / J2 | MAX2 | **IO48** | IO12 |
| P3 / J3 | MAX3 | **IO42** | IO21 |
| P4 / J4 | MAX4 | **IO12** | IO47 |
| P5 / J5 | MAX5 | **IO21** | IO48 |

- **IO47 is now FREE** (was MAX4_CS on V4).
- **IO42 newly used** (module pad 35; unused on V4).
- IO2 remains free (heartbeat candidate; module pad 38).

## Unchanged vs V4 (verified identical in netlist diff)
| Function | GPIO |
|---|---|
| SPI MOSI /ALL_SDI | IO39 |
| SPI SCLK /ALL_SCLK | IO40 |
| SPI MISO /ALL_SDO | IO41 |
| LCD_CS / LCD_DC / LCD_RST | IO16 / IO18 / IO15 |
| Backlight CTRL /LED_PWM (TPS61165) | IO17 |
| Encoder A / B / Btn | IO5 / IO6 / IO7 |
| Igniter / Auger / Fan | IO3 / IO9 / IO10 |
| ZC_DET (H11AA1) | IO8 |
| Hopper switch (J9 screw terminal) | IO14 |
| Native USB D− / D+ | IO19 / IO20 |

## Ready-to-paste block
```c
/* ===== PelletPirate V5 board ===== */
/* Probe jacks are FRONT-PANEL ordered on V5: P1..P5 == J1..J5 == MAX1..5.
   Do NOT apply the V2/V4 P1=J5 reversal table to this board. */
#define V5_RTD_CS_P1   4    /* MAX1, jack J1 */
#define V5_RTD_CS_P2   48   /* MAX2, jack J2 */
#define V5_RTD_CS_P3   42   /* MAX3, jack J3 (pin new vs V4) */
#define V5_RTD_CS_P4   12   /* MAX4, jack J4 */
#define V5_RTD_CS_P5   21   /* MAX5, jack J5 */
/* All other assignments identical to V4:
   SPI 39/40/41, LCD cs16 dc18 rst15, backlight 17, encoder 5/6/7,
   igniter 3, auger 9, fan 10, zc 8, hopper 14, native USB 19/20. */
/* Freed vs V4: IO47. Free for future use: IO2 (pad 38). */
```

## Bring-up notes carried into V5
1. **Boot-safe rule stands:** IO3 (igniter) is an S3 strapping pin; first firmware statements must drive IO3/IO9/IO10/IO17 LOW (same as V4 rule).
2. **LCD orientation:** FPC1 moved again (V5: top-right, B side). Expect another 180° flip → verify BOARD_LCD_MADCTL at first boot; toggle if the dashboard is upside down.
3. **ZC_DET now has a test point (TP1)** next to the ESP with GND at TP2 — scope the 120Hz pulses there during fan-phase ISR bring-up. RC (R43 10k pullup + C42 1nF) sits at the IO8 pin; net is 26mm total (V4 was 106mm).
4. **USB is data-only** (VBUS lands only on J8 + USBLC6 pin 5) — board must be powered externally, same as V4. Post-flash: POWER CYCLE, no serial pokes (V4 rule stands).
5. Hopper input (backlog item 15): J9 pin 1 → 100Ω (R47) → IO14, 10k pullup (R40) to 3.3V, 100nF (C41) at the pin. Switch closes to GND.
