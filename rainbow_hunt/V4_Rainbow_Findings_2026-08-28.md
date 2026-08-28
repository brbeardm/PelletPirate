# V4 Rainbow Hunt — Scope Session 1 Findings (2026-08-28)
Instrument: Rigol DHO814 (SCPI over LAN, harness-driven). Board: V4 #2 (unmodified, rainbow-confirmed).
Probes: CH1=J7.1 (12V), CH2=J7.2 (3.3V), CH3=J7.3 (LED+), gator gnd J7.4, all 1x. Pickup floor
calibrated: 25-46mVpp (tip-shorted test) — measurements are ~96% real signal.

## The headline numbers
| Rail | BENCH (clean LCD) | AC (rainbow) | Delta |
|---|---|---|---|
| 12V | 707-1123mVpp / ~108mVrms | 1312-1461mVpp / ~74mVrms | peaks +30%, rms down |
| **3.3V (panel)** | 228-234mVpp / **28.9mVrms** | **675-684mVpp / 109-129mVrms** | **rms x4.5** |
| LED+ | 224-256mVpp / ~27mVrms | 627-655mVpp / ~24mVrms | peaks x2.5, rms flat |

**The 3.3V rail is the differential.** ~110mVrms continuous disturbance on the panel supply in the
rainbow state vs ~29mVrms clean. (Explains the old DMM confusion: hash exists in both modes; only the
scope shows the 4x energy jump.)

## Character of the disturbance (all measured, AC state)
1. **NOT line-correlated:** trigger-delay sweep across a full 60Hz cycle: 109.7-110.9mVrms flat
   (ratio 1.01). Kills all 120Hz-envelope / zero-cross theories.
2. **True-rate spectrum (1Mpt, 6.25MSa/s):** 3.3V energy at ~14/21/28/35kHz (≈7kHz comb);
   12V shows the same ~7kHz comb PLUS ~117kHz + harmonics (178/234kHz) = **PS1 flyback switching
   fundamental ~117kHz, burst-grouped at ~7kHz repetition.**
3. **Backlight load sweep (10% / prior / 100%): 111.0 / 108.7 / 110.8mVrms — flat.** The comb is
   insensitive to backlight-range load changes; not a simple threshold within that range.
4. Bench-mode 12V carries its own ~1.1Vpp of PS2 buck spikes (~470kHz-ish rep) with clean LCD —
   the panel tolerates HF spikes fine; it is the AC-only kHz-rate burst content that correlates
   with the rainbow.

## Measured causal chain
PS1 (PBO-5F-12) emits switching bursts (~117kHz events grouped at ~7kHz) -> 12V rail carries the comb
(fast edges bypass the 700uF bulk via ESL) -> passes into/through PS2 -> panel 3.3V rail at ~110mVrms
-> HX8357D VCOM/gamma charge pumps upset -> rainbow. LED+/12V rms deltas are secondary.

## Consistent with all prior eliminations
Board 5's amputations (ZC, H11AA1, MOC3053), polarity checks, layout corridor scans — none touched this
mechanism, which is why none changed the symptom. Not layout. Not added circuitry. Not assembly polarity.

## Open questions (next sessions)
1. **Why is V2 clean with the same PS1 model?** Candidates: higher 12V standing load (DevKit overhead)
   keeping PS1 out of burst mode; unit/vintage differences in PS1 control behavior; V2's 3.3V chain
   filtering the comb better. TEST: identical clip-on session on a V2 board (C15 legs + 3.3V access),
   user-approved earlier. If V2's PS1 also bursts but 3.3V stays clean -> difference is downstream
   filtering; if V2's PS1 doesn't burst -> operating point/vintage.
2. **Dummy-load test:** a real 12V load (e.g., 100 ohm 2W = 120mA) to push PS1 harder than the
   backlight can — does the comb collapse? Needs a part from the junk bin.
3. Panel-rail sensitivity threshold: somewhere between 29 and 110mVrms.

## Fix directions for V5/V5.1 (pending confirmation)
- **LC/RC filtering between 12V entry and PS2 input** (kills the comb before the 3.3V domain);
  V5 currently = V2-proven clusters, but the comb is upstream of all of it.
- **Minimum-load resistor on 12V** if burst-threshold theory confirms (cheapest fix, ~0.25W burned).
- Panel 3.3V post-filter (ferrite + bulk at FPC feed) as belt-and-suspenders.
- NOTE: V5's already-built improvements (short ZC, LED pair, corridor separation) are all still good —
  they just address different (cook-time) risks than this one.

## Session artifacts
captures/: bench + AC 3-rail screenshots, ranged measurements, pickup calibration, line-trigger frames,
deep spectra source data (AC_3V3_deep.bin, AC_3V3_100ms_deep.csv). Harness: scope_harness.py.
