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

## Firmware Backlog (started 2026-07-17, post-OTA batch 37081be)

Running bug/enhancement list. Mark items done with the commit hash rather
than deleting them.

1. LCD idle dimming — dim to 50%/25% (or off) after encoder inactivity,
   restore on encoder turn. Backlight component already does LEDC 10-100%
   with NVS-saved level; this is an inactivity timer + temporary override.
   TPS61165 rule still applies: CTRL low >2.5ms = shutdown, so dim rather
   than drive to zero.
2. Fan/Auger/Igniter on/off status on the LCD cook dashboard — web has
   actuator pills already; state lives in grill_state (fan_on/auger_on/
   igniter_on). LCD layout addition only.
3. Review Grill Temp Drop alarm — fires at 30F below target after reaching
   band; too eager for lid-open/wind dips (real example: fired 18:03
   2026-07-17, grill self-recovered, alarm nagged until acked 19:13).
   Consider dwell time, wider band, and/or auto-clear on recovery. Review
   together with the flame-out shutdown (target-60F / 10 min) as two tiers.
4. BUG: encoder alarm-ack bleed-through — hold-to-ack also emits a click
   into whatever is focused (jumps into temp edit etc.). LVGL fires CLICKED
   on long-press release (same mechanism as the profiles-delete lesson —
   it uses SHORT_CLICKED for this reason). Fix: after ack, swallow ALL
   encoder input for ~1-2s; ignore rotation while the button is held.
5. Fan burst cycle 30s -> 2s (in-band Cook modulation) — fan audibly stops
   for 15s stretches. MOC3063 zero-cross rules out PWM/phase-angle, but
   2s integral-cycle bursts keep the impeller spinning (quasi-continuous
   reduced speed). One constant (FAN_CYCLE_SEC, actuator.cpp); MUST
   listening-test the real fan for hum/surging. Fallback: FAN_MIN_DUTY=1.0
   (always on in band). True variable speed = V3 hardware (MOC3052
   random-phase + zero-cross detect, or DC blower).
6. Settings: probe/jack assignment — assign which physical jack (J1-J5) is
   the GRILL probe; remap meat probes accordingly. Map is hardcoded today
   (main.c s_rtd_cs[5] = {27,13,5,26,21} = J1..J5, index 0 = grill). Store
   in NVS, apply in temp_task indexing, LCD Settings UI (+ web parity).
   SAFETY: grill channel drives PID/igniter-inhibit/fault-shutdown — remap
   only while mode==Off, applied atomically.
7. Continue the same log file across power-loss resume — today a resume
   opens a NEW CSV (one cook = several files) and ET/EST/graph reset.
   Persist active filename + cook wall-clock start in NVS beside
   run_mode/run_tgt; on resume reopen in append mode with a
   "POWER LOSS - resumed" event row; restore ET. Wrinkles: SNTP not synced
   yet at reopen (brief boot-relative timestamps acceptable); rotation must
   never delete the active file.
8. Cook start timestamp on Web + LCD — "Mode: Cook - Start: 7/17/2026
   11:19 AM" next to the mode on both UIs. Start = wall clock at the
   Off->active transition, FIXED across mode changes and (with item 7)
   across power loss. Duration of the smoke is a headline BBQ stat: start
   to SHUTDOWN-complete. Existing grill_state cook_start_time is
   monotonic-based (for ET) — add a wall-clock sibling, persist to NVS,
   surface in status JSON + LCD dashboard; backfill gracefully if ignition
   precedes SNTP sync.
9. Goal-reached alert, GREEN, distinct from RED action alarm — only
   alarm_temp fires today (target_temp is EST/display only); probe 2's
   155F goal passed silently 2026-07-17. Crossing target_temp should fire
   its own alert with a GREEN banner (done = good news); action alarms
   (Wrap/Baste/...) stay RED. Needs per-probe goal-alarm state (same
   ack/hysteresis pattern), green styling on LCD banner + web alarm bar,
   distinct cooklog event ("GOAL probe N reached X"), and graceful
   coexistence when both fire close together.
10. BUG (fix design approved): temp-drop alarm fires on target raise —
    make grill_reached_band honest. The flag latches "reached temp" but
    not for WHICH target; raising 225->250 at 220F fires GRILL TEMP DROP
    instantly on stale evidence (fired live twice, 2026-07-17). Fix: in
    grill_state_alarms_update, track previous target; on any target
    change recompute grill_reached_band = (temp within GRILL_INBAND_F of
    the NEW target). Large raise -> flag clears -> climb phase (alarm
    silent until the new band is genuinely reached); small bump or lower
    -> protection continues seamlessly. Also auto-clear an ACTIVE drop
    alarm whose premise the recompute invalidates. Side benefit: the
    flame-out shutdown shares this flag and stops misfiring during
    target-raise climbs too. Orthogonal to item 3 (lid-open dips) — both.
11. Ack disables probe action alarm (one-shot) — today ALARM_ACKED
    re-arms once the probe cools 5F below threshold; wrapping the meat
    dips the probe during handling and the alarm re-fires for meat
    already dealt with (happened live: pork-butt Wrap@165, 2026-07-17).
    Fix: acking a PROBE action alarm disables it (clear alarm_temp or
    latch a done state, persist to NVS; UI shows it as spent until a new
    alarm is set). Grill temp-drop and sensor-fault alarms KEEP their
    re-arm behavior — those are condition-based safety alerts, not
    one-shot actions. Item 9's green goal alert: one-shot the same way.
12. EST review + "Next Goal ET" — probe EST felt frozen and showed 9:53
    near end of cook. Root cause: linear extrapolation of last-hour rate
    (5-min samples); during a stall (~2.5F/hr) it honestly projects ~10h,
    then collapses when the stall breaks. (a) Improve EST: weight recent
    rate higher, show a "stalled" indicator instead of absurd hours, or
    stall-aware heuristic; at minimum make it visibly update. (b) NEW:
    "Next Goal ET" beside elapsed time on both UIs — soonest upcoming
    goal across enabled probes ("Next: P2 Pork 203 ~0:45"), recomputed
    as goals are reached. Web-first.
13. Pellet-starvation early warning (firmware) — detect "auger pegged AND
    temp diving" in COOK (e.g. u >= 0.95 sustained AND temp fell >= 20F
    over 4 min) -> distinct RED alarm "CHECK PELLETS" + cooklog event,
    ~10 min ahead of the flame-out shutdown. Catches ALL fuel-delivery
    failures (empty hopper, pellet bridging, auger jam, shear pin, motor)
    — reactive last line vs item 15's proactive gauge. Signature
    validated by the 2026-07-18 07:36-07:48 pellet-out (u=1.00, 260->215
    in 4 min). OTA-deployable to V2 immediately.
14. Push notifications to phone (HIGHEST VALUE) — alarms only exist on
    the LCD banner and an open web page; the 04:38 Wrap alarm and 07:48
    flame-out rang into a sleeping house. Fire every alarm (red action,
    green goal, starvation, hopper-low) as a phone push. Simplest:
    ntfy.sh (free, no account, plain HTTP POST; user subscribes to a
    topic). Needs Kconfig/NVS topic setting, POST on alarm transitions,
    settings UI (web-first). Multiplier for items 3/9/13/15.
15. V3 hardware: hopper low-level switch — the prevention layer (warns
    hours ahead; "HOPPER LOW" push at 2AM -> dump a bag -> non-event).
    Prefer a mechanical paddle/lever microswitch (pellet dust fouls IR
    optics); V3 PCB adds one protected GPIO input (pullup + ESD/RC) and
    a connector for the hopper harness. Firmware: yellow HOPPER LOW
    alarm + push via item 14. Blind to bridging/jams — item 13 covers
    those (fuel gauge vs check-engine light; keep both).
