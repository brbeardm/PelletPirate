PelletPirate LCD-to-Main FPC Jumper  —  REV H  —  FABRICATION PACKAGE
====================================================================
Date:        2026-08-16
Customer:    Brian Beardmore  (bbeardmore@carvizor.com)
Quote ref:   F30924Q01  (prior 1-layer quote, PAID) — 2-layer re-quote requested
Quantity:    25 pcs

WHAT CHANGED IN REV H
---------------------
- The golden finger (mating end for Hirose FH26W-39S-0.3SHW) has been rebuilt to the
  Hirose "Recommended FPC" STAGGERED two-row pattern. The previous versions were a
  uniform straight comb and did not match the datasheet.
- See PCB_to_LCD_FPC_REV_H_GoldenFinger.dxf and
  PCB_to_LCD_FPC_REV_H_DimensionSheet.pdf for the exact finger geometry.

CONSTRUCTION
------------
- 2-layer double-sided flex (NOT 1-layer). Signals route from the Hirose connector on
  the front, through 39 plated through-vias, to the gold fingers on the BACK of the
  mating tongue.
- Finish: ENIG (gold), for the ZIF mating contact.
- Finished mating thickness at the tongue: 0.20 +/- 0.03 mm.
- Connector-area stiffener: 0.15 mm SUS on the bottom side (subject to your assembly
  engineering confirming suitability for SMT reflow).

GOLDEN FINGER  (see dimension sheet / DXF)
------------------------------------------
- Hirose FH26W-39S-0.3SHW(60), 39 positions, 0.30 mm pitch, straight-through 1->1..39->39
- STAGGERED 2-row: odd fingers 1.1 mm exposed, even fingers 2.1 mm exposed
- 0.30 mm contact pads; 0.30 mm setback from FPC leading edge; 0.15 mm edge margin
- End fingers (1 and 39): 0.25 mm outer trace, mirror pair
- R0.2 max relief at the two outer tongue corners
- Taper-zone copper spacing is 0.0707 mm (= 0.10 x cos45deg, per the datasheet geometry).
  PLEASE CONFIRM your flex process reliably holds this spacing at 0.30 mm pitch.

COVERLAY
--------
- Expose all 39 gold fingers with a SINGLE coverlay window over the finger bank
  (approx. 12.0 x 2.5 mm). Any per-pad openings in the B.Mask gerber are indicative only.

VIAS
----
- 39 plated through-vias, 0.20 mm pad / 0.10 mm drill (= 0.05 mm annular per side).
  Please confirm your capability, or advise your recommended minimum.

TESTING
-------
- Please perform 100% continuity and short-circuit testing of all 39 conductors,
  straight-through pin 1->1 through pin 39->39.

STACK-UP / QUOTE
----------------
- Please propose your 2-layer flex stack-up (copper weights, coverlay, ENIG) that meets
  the mechanical requirements above, and provide the 2-layer re-quote plus the
  supplementary invoice vs. the paid 1-layer quote F30924Q01.

OPEN DFM QUESTIONS FOR YOUR REVIEW
----------------------------------
- 0.0707 mm finger-to-finger spacing capability (see Golden Finger, above)
- 0.05 mm via annular on 0.10 mm drill capability
- 0.5 mm tongue side tab / edge features — please confirm these are correct for the
  FH26W-39S insertion guides, or advise.

FILE MANIFEST
-------------
- gerbers/                                  RS-274X gerbers (all layers) + Excellon drill (.drl)
- PCB_to_LCD_FPC_REV_H_GoldenFinger.dxf     golden-finger geometry (B.Cu + coverlay + outline)
- PCB_to_LCD_FPC_REV_H_DimensionSheet.pdf   golden-finger dimensioned specification (1 page)
- kicad_source/                             KiCad 10 project (board, schematic, rules, footprint libs)
- README_FPCWay_REV_H.txt                   this file
