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


# SESSION 4 — spec audit + interventions on board 2 (2026-08-28 late night -> 08-29). CASE REOPENED.
Session 3's "case closed" was premature. Session 4 killed the comb-amplitude model and reclassified the symptom.

## PBO-5F-12 datasheet audit (Bel rev 09/18/2025)
- Ripple/noise spec: 150mVp-p MAX at 20MHz BW, nominal input, RATED load (420mA), 25C. All regulation
  specs conditioned on 10-100% load (42-420mA); below that, behavior unspecified.
- ~~Max capacitive load violation~~ **RETRACTED (user caught it, 08-29): datasheet page 5 Figure 2 +
  Table 1 (EMC recommended circuit) explicitly specify for the 12V model: C2=470uF/25V at the module,
  L1=2.2uH, C3=220uF/35V behind it, TVS SMBJ20A — which IS the board's C13/L1/C15/D1 verbatim, and the
  front end (CX=C4, LF=LF1 30mH, MOV 14D561K=MOV1, C1 15uF/450V=C7) completes Figure 2 exactly.
  The 470uF max-cap-load spec applies to the module's direct load (C13 alone = compliant); the 220uF
  is decoupled behind L1 per Bel's own arrangement. NO SPEC VIOLATION EXISTED. The board already
  contains the C-L-C pi-filter; V2 was designed straight from this datasheet.**
- Consequence: the board-2 C13->220uF transplant was performed on a false premise and makes board 2
  DEVIATE from Fig 2 (220 where 470 belongs); reinstall the original 470uF when convenient
  (measurements showed the swap changed nothing, consistent with the retraction).
- Mitigation list reordered: "add pi-filter" is MOOT (exists). Primary V5 fixes = SPI series resistors,
  panel-feed filtering, ground stitching; plus optional HF ferrite bead in addition to L1 for the
  ns-band a 2.2uH power inductor doesn't block. V2-vs-V4 run the SAME Fig-2 circuit with different
  outcomes -> the differential is LAYOUT/GROUNDING of this circuit + the DevKit's buffering, not the
  circuit itself.
- Reinterpretation: our "117kHz flyback" = 2nd harmonic of ~58.5kHz (117/178/234 = 2x/3x/4x) —
  consistent with the 65kHz typ switching spec. Module behaves like a PBO-5F should.
- TPS562201 audit (same night, see V5 project): PS2 cluster 100% per datasheet Table 7-2. No PS2 changes
  justified — TPS562208 swap idea WITHDRAWN (V2 runs the same part clean; not the differential).

## Experiments on board 2 (all same session, fresh baselines each)
1. **Dummy load +75mA** (100+100 series || 390+390 series = 159 ohm at J7.1/J7.4): no effect beyond the
   comb's natural drift. Light-load theory: no resolvable effect at this scale (earlier "dead" verdict
   was over-claimed — the comb's own wandering exceeds any load effect).
2. **Comb wanders spontaneously**: 7k line 2.35 -> 5.04mV in minutes, same config, nothing touched.
   Any single-scan A/B on this board is unreliable; only repeated sampling counts.
3. **C13 -> 220uF transplant** (donor = board 5's C15; rail now 220+220 = 440uF, IN SPEC): rails healthy
   (11.97/3.31), comb NOT collapsed (7k hit record 7.04mV post-surgery), flicker rate ~unchanged.
   Spec fix KEPT (it is correct regardless); it is not the cure.
4. **Comb-amplitude <-> symptom correlation: DEAD.** Flaky episode at 7k=5.04mV, then CLEAN boots at
   4.30 and 7.04mV (the record). Amplitude does not predict the symptom.

## Symptom reclassification (user's precise description)
On board 2 with the NEW panel: orange selector elements flip orange->green and back; grayed text
(COOK MODE, TARGET) flickers gray->white->gray; episodic, boot-to-boot variable, sometimes absent for
minutes. This is WRITE-TIME pixel corruption (RGB565 bit-shift signature: one glitched SCLK edge shifts
all following bits -> wholesale hue flip until next clean write) — a DIGITAL/SPI mechanism, distinct
from analog VCOM/gamma "rainbow" shimmering. The historical record may conflate >=2 mechanisms; the
original suspect-panel behavior may still be analog. FPC-contact cause explicitly excluded by user
(cable touches nothing; verified every test).
Panel census caveat: "new panel clean" did NOT hold over hours — census re-read as: all panels
flicker occasionally on V4-AC (suspect panel = most susceptible), aggressor varies in time.

## Where the evidence now points
12V-side exonerated piece by piece: load point (no effect), cap loading (fixed, no change), comb
amplitude (uncorrelated). Leading model: **stochastic SPI write corruption during AC operation,
coupling path unknown.** Never directly observed — only inferred from pixel colors.
**NEXT SESSION: catch it in the act.** Solder thin wire tails to SCLK (ESP castellation pad) + GND;
CH3/CH4 on SPI, CH1 on 12V; trigger on SCLK runts/glitches; correlate with PS1 bursts. If glitches
appear on the wire during corruption events -> conducted/coupled aggressor confirmed and localizable.

## Program status
**V5 reorder ON HOLD (user, 2026-08-29): "can't justify $1000 until we resolve something concrete."**
V5 changes justified so far: 12V rail total capacitance <=470uF (datasheet-traceable). Ferrite/LC stack
and panel post-filter: PENDING the SPI hunt verdict. Board 2 config: C13=220uF (donor), 100uF bodge
still on J7 3.3V. Board 5: parts donor. Session 4 artifacts: captures/board2_noload_*, board2_load_removed_*,
board2_rainbow_visible_*, board2_c13_220u_*, board2_c13_cycle1_*, board2_c13_cycle2_*, tps562201_extract*.txt,
ps2_cluster.py, v5_audit_netlist.net.

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

## Addendum 2: board 4 survey (unbodged, AC, good panel — slight infrequent flicker)
DC 11.94V / **3.54V (3.3V rail runs +7% high — fleet DC gradient 3.31/3.39/3.54/4.41 tracks noise
rank; possible PS2 unit-variance spread with board 5 as the broken extreme — **DMM CONFIRMED 3.50V**,
reading is real. Unifying mechanism: D-CAP2 valley regulation — FB noise pushes DC up, so setpoint
gradient = same noise disease, four severities, board 5 the end state)**.
12V ~76mVrms. 3.3V: **184-185mVrms — noisiest healthy board — comb fundamental at 3.5kHz (71mV single
line!) not 7kHz**; burst repetition rate is unit-dependent. Display with a good panel: only barest
occasional flicker at 185mVrms while the marginal panel rainbows at 58 → **panel immunity spread >3x;
panel variance is the DOMINANT variable.** <30mVrms V5 target unchanged (must cover worst panel).
Artifacts: board4_AC_rails.png, board4_AC_3V3_spectrum.json.

## Addendum: board 3 survey (unbodged, AC, new panel — LCD CLEAN)
DC 11.70V / 3.39V. 12V: 3.1Vpp / ~71mVrms. 3.3V: **96mVrms with the same comb (7k dominant 2.9mV,
14k 1.6mV, 28/35k alive)** — and the display is CLEAN with a good panel. Confirms fleet consistency
(V4 boards all carry ~95-130mVrms comb) and panel-dominance of the symptom. This capture doubles as
the missing valid unbodged V4 deep spectrum: captures/board3_AC_3V3_spectrum.json (+ board3_AC_rails.png,
scan script board3_scan.py). Note: board-to-board harmonic variance (board2-bodged 14k line > board3-unbodged)
is comparable to the bodge effect — single-cap interventions are lost in unit variance; ferrite/LC stack it is.
