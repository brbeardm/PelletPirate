# PelletPirate V5v1 — Enhancement Backlog
Created 2026-08-28 as a self-contained clone of PelletPirate_V5 (as-ordered state, commit 88b1ec5 lineage).
This project holds queued improvements for the NEXT PCB run. V5 (5 boards, PCBWay order of 2026-08-27)
remains the reference design; make changes HERE only.

## Queued enhancements
1. **USB ESD ballast:** add 100nF X7R from U1 (USBLC6-2SC6) pin 5 (VBUS) to GND, placed at the pin.
   Part: KEMET C0402C104K3RACTU (already BOM line C9 — quantity bump, no new line).
   Rationale: local charge reservoir for the ESD steering rail; rail is otherwise floating (data-only design)
   or stiffened only through cable inductance. Not a defect fix — a robustness nicety.
2. **TP2 relocation (optional):** move TP2 (GND) to within ~5mm of TP1 (ZC_DET) so a scope spring-ground
   reaches both. Currently ~13mm apart — usable with wire loops, not with a spring tip.
3. **Silkscreen version text:** update board silk from V5 to V5.1 before any fab (user edit in KiCad).
4. **PLACEHOLDER — rainbow-hunt findings:** ✅ RESOLVED 2026-08-30 — the raft revision (C44/C45 bulk,
   33Ω SPI series R, /3V3_LCD ferrite island, L5/C48) was folded into V5 itself before the order; this
   clone inherits it. Nothing further pending from the hunt except item 6 below.

## State inherited from V5 (all verified at clone time)
- ERC 0 / DRC 0 / parity 0, run from THIS directory (proves self-containment).
- sym-lib-table + fp-lib-table: 100% ${KIPRJMOD} — no references outside this folder.
- All internal file references renamed PelletPirate_V5 -> PelletPirate_V5v1
  (pro meta + sheet names + BOM export path, sch instance project names x189, pcb sheetfile refs x130, prl meta).
- Excluded from clone (V5-specific): PelletPirate_V5-backups/, archive_to_delete/, PCBWay/ (V5 order package),
  .claude/, editor .bak/.lck files.
- Docs carried over (still accurate for this rev): boardpins delta (pin map unchanged), BOM fill table reference.

## Reminders for the eventual V5.1 fab package
- Regenerate everything from THIS project (gerbers/positions/BOM/DNS notes) — do not reuse V5's PCBWay/ files.
- BOM DNS list carries forward unchanged unless the rainbow verdict adds parts.
- USB4145 stake variant remains -0170 (1.6mm board).

## Added 2026-09-03 (scope cross-cal session fallout)
6. **SCLK test point (user edit in KiCad):** V5 has probe access for 12V/3.3V/GND/LED+ (J7) and ZC
   (TP1/TP2) but NO test point on SCLK — the single most-probed net of the entire rainbow saga, which
   required soldering a tail onto ESP pad 33 on V4 board 2. Add a TP on SCLK (near the ESP SPI pads,
   with a GND spring-tip landing within ~5mm — same lesson as item 2). This is the measurement node for
   the V5 acceptance criteria themselves (runt sentinel + idle-peak <300mV on the MHO954), so until V5.1,
   any V5 board that needs the full acceptance check gets a soldered tail; rail-level screening via J7
   comes first and may suffice. Consider TPs on SDI/CS too while in there — cheap now, priceless later.

## Added 2026-08-28 (C9/R20 package-mismatch fallout)
5. **C9 footprint shrink (user request "smaller"):** C9 (0.1uF 25V, PS3 area) is 0805 by V2 heritage.
   Shrink footprint to C_0402_1005Metric and set MP back to C0402C104K3RACTU (0402) - same class of
   change as the C21-C35 shrink already proven on V5. Optional same treatment for R20 (10k) -> RC0402FR-0710KL.
   Until then, fields say the 0805 parts (C0805C104K3RACTU / RC0805FR-0710KL) matching current copper.
   The NEW VBUS cap (item 1) is to be designed 0402 from the start.
   LESSON: BOM value-matching from another rev MUST verify footprint size per line - two slipped through
   on the V5 order (C9, R20) and were corrected in the PCBWay BOM during review, 2026-08-28.
