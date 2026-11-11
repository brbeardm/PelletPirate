# Pellet Hopper Gauge & Flame-Out Safety — Feature Spec (V5)

**Status: PLANNED — no code yet.** Agreed in planning session 2026-09-24 while V5 boards
are in assembly at PCBWay (order YV1799215, early-October arrival). Implementation
begins after V5 first-article bring-up.

Bench validation backing this spec (2026-09-22, ESP_Breadboard rig): Sharp GP2Y0E02A
electrically validated 0.5–2.2V; ADC2_CH3/IO14 sampled 4400+ times at 10Hz with WiFi
connected (PS_NONE) + web traffic = **zero ESP_ERR_TIMEOUT**. Prototype dashboard with
Set EMPTY/FULL capture and median+EMA smoothing lives in `esp_breadboard_test_fw`.

---

## 1. Hardware context (as-built V5)

- Sensor: **Sharp GP2Y0E02A** analog IR ranger. Output is **linear in distance**
  (~36 mV/cm; 2.2V @ 4cm → 0.55V @ 50cm — verify exact slope/intercept + tolerances
  against `datasheets/gp2y0e02a_e.pdf` at implementation time).
  Blind zone < 4cm (1.6"); hard max range 50cm (19.7"); rated −10..+60°C
  (convenience feature, not safety-critical sensing).
- Wiring: sensor VDD + GPIO1(enable) from **J7 pin 2 (3.3V)**; Vout → **J9.1** →
  R47 100Ω → **IO14 = ADC2_CH3**; C41 100nF to GND; **R40 10k pull-up must be DNP**
  (ECO sent; verify factory-DNP vs bench-remove at first article). J9.2 = GND.
- Sensor wire colors (photo-verified): YELLOW=VDD, BLUE=GND, RED=Vout, BLACK=GPIO1→3.3V.
- **Mounting: aim the optical spot at the auger throat / crater center**, recessed in
  lid ~2–2.5" above max fill. Rationale: pellets tunnel out in a funnel; the auger
  starves when the crater bottoms out even while shoulders hold pellets. The narrow
  spot measuring the crater = measuring what the auger can actually eat.

## 2. Firmware: board support + hopper driver

- **Board revision flag** (recommended: compile-time Kconfig, same pattern as the
  existing V2/S3 dual-target): `V4` = J9 dry-contact switch semantics; `V5` = analog
  hopper gauge. All other V5 IO identical to V4 (CS map P1..5=J1..5, IO4/48/42/12/21).
- New `components/hopper` module, ported from the bench rig:
  - adc_oneshot on ADC2_CH3, ADC_ATTEN_DB_12, curve-fitting cal, **retry loop** on
    ESP_ERR_TIMEOUT (proven unnecessary in practice but kept as armor).
  - Sampling: lazy **burst every few seconds** (not 10Hz) → median-of-9 + EMA.
  - Publishes: **percent, raw mV (diagnostics), health state**.
- Health states from voltage alone:
  | Reading | Meaning | UI |
  |---|---|---|
  | ~0V | sensor unplugged/dead | "–" in icon, gray, one-time warning; **never** triggers safety |
  | below empty-cal | crater beyond empty point | clamp 0% |
  | above full-cal / ~2.2V | blind zone / overfull | clamp 100% |
  | long-term downward drift at known-full | lens dust | drift warning (v2) |

## 3. Calibration: depth-in-inches (primary), capture (advanced)

- **Primary UX = one setting: Hopper Depth in inches**, defined as **sensor lens
  straight down to hopper bottom** (mount offset thereby included). Valid range
  **6–18"** (sensor max range 19.7" — deeper hoppers rejected with help text).
  Firmware computes mV endpoints from the linear transfer curve:
  `%full = (d_empty − d_now) / (d_empty − d_full) × 100`, d_full = fixed ~2.5" const.
- v1 semantic = **level %** (linear in distance). Taper-corrected volume % is a
  possible v2 refinement.
- Expected absolute error from unit tolerances (sensor + front end + ADC): ±5–10% of
  reading — acceptable for alerts at 20/10%.
- **Advanced/fallback: two-point capture** (Set Empty / Set Full, averaged with live
  bar, sanity check span ≥ ~200mV) — service tool for edge-tolerance units, odd
  mounts, dust drift. Already prototyped on bench. All cal in **NVS**.

## 4. Settings — "Pellet Control" menu (LCD + web parity)

1. **Hopper Depth**: 6–18 inches.
2. **Flame-Out Safety**: On / Off.
3. **Flame-Out %**: 5 / 10 / 15 / 20 / 25 (5% steps, max 25). User tunes after
   watching one real burn-down (crater geometry varies per hopper).

Fixed (not settings): informational alerts at 20% and 10% (ntfy push + UI banners).
Push policy: during active cook only (dashboard icon always live). *(open: confirm)*

## 5. Flame-Out Safety behavior (state machine)

Trigger: % **continuously below Flame-Out % for ~60s**, armed **only in active cook
states**, with hysteresis. Sensor fault never triggers it.

Action = enter **PELLET HOLD** (a sub-state *inside* the cook session):
1. Auger off immediately (prevents pellet pile → relight overfire hazard).
2. Fan cooldown cycle, then idle. Igniter off.
3. Critical ntfy ("PELLET OUT — grill shut down safely") + red web banner +
   full-screen LCD alert with distinct reason code.

**Session survives.** Cook clock, temp history, program step, setpoint, probe targets
all preserved. Program step timers **freeze** during hold. Grill may hold
indefinitely (electrically idle = safe); optional repeat reminder push every
30–60 min *(open: confirm)*.

**Resume** (button on LCD + web):
- Reads hopper %. Must exceed **Flame-Out % + ~5% margin** to accept.
- Below margin → "**Add Pellets to the hopper to Resume**"; live icon on the same
  screen so the user watches % climb while pouring; button enables in real time.
- Accepted → full from-cold **ignition sequence**, then back to COOK at the exact
  prior setpoint/program step. Never auto-relight without a human press.

**Exit rule: only user END → OFF closes a session** — from COOK or HOLD alike.

Analytics: hold recorded as **event markers** on the cook timeline
("Pellet out 3:42 PM, resumed 4:07 PM"); pattern extends later to lid-open dips,
setpoint changes, step transitions.

## 6. UI: hopper icon (shared design language)

Vertical rectangle with **"V" taper at the bottom** (hopper silhouette), fill drains
top-down, **% number rendered inside the icon, iPhone-battery style**:

```
┌──────┐
│██████│
│██████│
│ 42%  │
 \░░░░/
  \░░/
   \/
```

- Text must survive the fill line passing behind it (outline or two-tone flip).
- Color ladder: green → yellow (≤20%) → red (≤10%); fault = gray outline + "–".
- **Web: inline SVG** (outline path + clipped fill rect + centered text).
- **LCD: LVGL** canvas/polygon + clipped bar + centered label (snprintf, no %f).
  Icon sized to hold "100%" legibly; earns real dashboard space.
- Web-first parity per standing preference; both ship in the same release.

## 7. Build order (each step independently testable)

1. Board-revision flag (V4/V5) in pelletpirate_ESP.
2. Hopper driver port — doubles as **V5 first-article acceptance test** with full
   grill FW live (LVGL + MAX31865 SPI + web UI + WiFi).
3. Web dashboard icon + Pellet Control settings + depth cal.
4. LCD icon + menu flow (Pellet Control under settings; encoder steps map to 5%
   increments).
5. Alert ladder + Flame-Out Safety + PELLET HOLD/Resume state machine work.
6. Advanced capture cal + event markers.

## Open questions carried forward

- Push alerts during active cook only? (lean: yes)
- Repeat reminder during long PELLET HOLD? (lean: every 30–60 min)
- Compile-time board flag confirmed over runtime? (lean: compile-time)
- v1 level-% (linear) confirmed over volume-%? (lean: yes)
