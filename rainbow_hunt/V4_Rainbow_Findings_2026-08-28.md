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


# SESSION 2 — V2 comparison (2026-08-29, board: V2 production unit, clip-on at D1 12V pin + DevKit 3V3/GND pins)
USB baseline (DevKit LDO only, PS2 off, 12V floats at 2.5VDC): 3.3V = 1.65mVrms — pristine.
(Teaches: V4's bench 29mVrms was mostly PS2's own buck ripple.)

AC (PS1 alive, LCD CLEAN):
- 12V: 768mVpp / 32mVrms — comb 6.1/12.5kHz + 61/75kHz + ~114kHz flyback + 191kHz
- 3.3V: 218mVpp / 48.8mVrms — comb 5.1kHz dominant, decaying fast (rel 0.07 by 36kHz)

## VERDICT (with Session 1):
**PS1 bursts on BOTH boards — it is normal behavior for this part at these loads.** The differential is
DOWNSTREAM ATTENUATION: V2's 12V node ~2.3x quieter, V2's 3.3V chain kills comb harmonics that V4 passes.
Panel upset threshold bracketed: ~50mVrms (V2 clean) < threshold < ~110mVrms (V4 rainbow).
Contributors to V2's extra damping: DevKit's added 3.3V-rail capacitance + standing load + LDO domain;
possibly PS1 lot/vintage amplitude differences (V2 units 2026-05, V4 units 2026-07) — unresolved, not needed.

## V5 FIX PLAN (fold in before reorder):
1. LC/ferrite filter on 12V feed into PS2 (kill comb upstream)
2. Added 3.3V bulk + panel-rail post-filter (ferrite + cap at FPC 3.3V feed)
3. Acceptance: panel rail <30mVrms on AC (2-3x margin below threshold) — verified with this scope
   on first V5 article via TP/J7, same harness.
Optional validation before respin: bodge the 12V LC + 3.3V bulk onto V4 board 5 (sacrificial) and watch
the comb die live — de-risks the V5 change for the cost of an afternoon.


# SESSION 3 — bodge test + panel census (2026-08-28 evening). CASE CLOSED.

## Board 5 disqualified and retired
Attempted the 100uF bodge on board 5 first. Measurements revealed board 5's power tree is broken
independent of any bodge: PS2 misregulates the "3.3V" rail to 4.41V on bench (feedback damage —
surgery casualty or native defect), and on AC the rail collapses to ~2.1V at ~360Hz (6x line) with the
12V carrying 3.27Vpp. The junk-bin cap was innocent (sawtooth persisted with cap removed). ESP/panel
survived sustained 4.41V operation (1V over abs max) — noted for the "tough chips" file.
NOTE: session-1 "deep" archive AC_3V3_deep.bin is a failed 10k-pt read (pre STARt/STOP fix) — no valid
unbodged V4 deep spectrum exists on disk.

## Board 2 bodge: 100uF axial across J7.2/J7.4 (3.3V/GND), on AC
- 3.3V: 109-129mVrms -> **58.2-58.9mVrms** (halved). Vpp went UP (675 -> ~970mVpp) — ESL passes spikes.
- 12V: ~72mVrms (unchanged, as expected).
- Deep 1Mpt Goertzel band scan (captures/board2_bodge100u_spectrum.json): 7kHz comb SURVIVES —
  7k=3.8mV, **14k=5.9mV dominant**, 21k=1.9mV, 28k=0.8mV; 117kHz fundamental crushed (0.016mV).
  ESR floor (~1 ohm for aged axial) is transparent at 14kHz: bulk electrolytic alone CANNOT kill the comb.
- **Rainbow unchanged at 58mVrms.** Simple rms-threshold model dead: V2 clean at 48.8, board 2 rainbows
  at 58 — 20% gap can't flip a panel.

## Panel census (4 panels, board 2 bodged @58mVrms, AC): THE RESOLUTION
| Panel | On board 2 (AC) | On bench | On V2 (AC) |
|---|---|---|---|
| Original board-2 panel ("suspect") | RAINBOW | clean | **CLEAN** |
| Spare (breadboard era) | CLEAN | - | - |
| Brand new out of box | CLEAN | - | - |
| (4th used panel — not yet run) | - | - | - |

## FINAL VERDICT
**Rainbow = (V4's 3.3V kHz-comb noise) x (a marginal panel unit). Both ingredients required.**
- 3 of 4 panels tolerate V4's AC rail (at the bodged 58mVrms; unproven at the original 120).
- The 1 marginal panel rainbows on V4-AC, is clean on V4-bench (quiet rail), and **clean on V2-AC** —
  V2's quieter chain (48.8mVrms, comb decaying by 36kHz) rescues even the weak panel.
- HX8357D VCOM/gamma charge pumps are the sensitive element; unit-to-unit tolerance spread is real.
- Panels are a lottery: V5 must be designed for the WORST panel. Acceptance target unchanged:
  <30mVrms on AC at the panel feed, comb suppressed (ferrite post-filter, not bulk-only — proven
  tonight that bulk alone fails).
- Caveat: V4's rainbow history should be re-read as "the marginal panel was the witness on every
  sighting" (pending user confirming the same panel toured boards 1/2/5).

## Bench state at session end
Board 2: 100uF bodge still installed on J7 (decision pending: remove or leave), functional with any
good panel. Board 5: RETIRED (PS2 feedback broken, 4.41V rail). Suspect panel: needs a sharpie mark.
Session 3 artifacts: captures/board5_*, board2_bodge100u_*.png/json.
