# V5 Footprint Verification Sweep
Date: 2026-08-28. Third pass of the deep re-verification program (after reconciliation and live-DigiKey).

## Tier 1 — J8's current (0170-named) footprint vs GCT USB4145 drawing
The drawing-level audit of 08-27 was performed on the 0070_REVA2 instance BEFORE the user's footprint swap;
the 0170 .kicad_mod was re-measured today against the same GCT recommended-layout numbers:
- 16 SMD signal pads, 0.30 x 1.15mm, rows at ±1.485, x at ±0.25/±0.75/±1.25/±2.75 (0.5mm pitch, 5.50 span): MATCH
- 4 shield slots 1.6 x 1.1 oval at (±4.00, ±1.43): MATCH
- 2 NPTH guide holes at (±4.00, 0): MATCH
Verdict: the 0170 footprint is geometrically identical to the verified land pattern. Mask state: 12 signal
pads at 0 expansion, corner-GND/shields at 0.102 (verified harmless 08-27).

## Tier 2 — every board footprint vs its source library file
Method: pad-by-pad comparison (name, type, position, size) of all 137 board footprint instances against
their .kicad_mod source (project libs via fp-lib-table + KiCad stock), with two representation
normalizations: (a) B.Cu placement stores Y-mirrored pad coords - expected KiCad behavior, not a diff;
(b) legacy-format lib files (unquoted pad names) parsed with format-tolerant parser.
Result: **137/137 identical. Zero hand-modified pads. Zero missing library sources.**
(First-run raw output showed 25 "diffs" - all were the two representation artifacts above; documented
here so a future re-run doesn't re-flag them.)

## Tier 3 — provenance of every footprint shape
| Class | Footprints | Provenance |
|---|---|---|
| Drawing-verified in-project | USB4145 (08-27 + Tier 1 today), TB002 (Same Sky datasheet 08-27), FH26W-39S (REV-H flex program + V2/V4 fab), MOV1 + C5/C6/C12 (user caliper measurements + Bourns/Vishay drawings, V4 era), C7/C15 can dims (MPN decode vs footprint 08-27) | direct dimensional evidence |
| Build-proven | SJ1-3523NG jacks, SCHURTER 0031.8201, PBO-5F-12 custom, MOC DIP customs + DIP762, SOT-23-6 (PS2/PS3/U1), SMB (D1), SOD-123 (D2), SW_KMR221GLFS, L1/L2/L3 vendor fps, SSRH7H choke, TQFN-20 (MAX), ESP32-S3 flyman, DF13, PPTC header, all KiCad-standard chip/radial/disc fps | fabbed + turnkey-assembled on V2 (2026-05) and/or V4 (2026-07); boards function |
| New since last fab | GCT_USB4145-03-0170-C (Tier 1 verified today), TestPoint_THTPad_D1.5mm (KiCad standard, trivial annular), C_0603_1608Metric for C39 (KiCad standard) | verified today / standard library |

**No footprint on the board lacks verification provenance.**

## Deep re-verification program status
1. Board-symbol-footprint-BOM reconciliation: DONE 08-28, 0 errors (after R20 fix)
2. Live DigiKey per-line BOM verification: DONE 08-28, 58/58 MPNs, 0 spec mismatches (3 stock flags)
3. Footprint vs manufacturer drawing sweep: DONE 08-28 (this document), 0 defects
Remaining reorder gate: rainbow-hunt verdict (scope) folded into the design, then regenerate fab package.
