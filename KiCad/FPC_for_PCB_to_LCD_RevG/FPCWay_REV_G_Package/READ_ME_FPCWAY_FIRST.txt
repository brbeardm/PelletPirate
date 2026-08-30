================================================================================
PelletPirate  -  PCB-to-LCD FPC jumper  -  REV G   (FPCWay manufacturing package)
================================================================================
Generated from KiCad source dated 2026-08-14 20:35 (PCB) / 20:36 (schematic).
Build quantity requested: 25 pcs (prototype).
Status: PRE-DFM engineering release - FPCWay to confirm flex DFM before fab.
DRC: 0 violations / 0 unconnected on the source board.

--------------------------------------------------------------------------------
1. WHAT THIS PART IS
--------------------------------------------------------------------------------
A 2-layer flexible printed circuit (FPC) jumper that carries 39 LCD signals
straight through, pin-1 -> pin-1 ... pin-39 -> pin-39.

  END A (front / Side A, F.Cu):
     J1 = Hirose FH26W-39S-0.3SHW(60) ZIF connector, SMD, 0.30 mm pitch.
     FPCWay assembles this connector onto the flex. It receives the LCD's
     incoming male FPC ribbon.

  END B (back / Side B, B.Cu):
     J2 = integral MALE gold-finger tongue, 39 contacts @ 0.30 mm pitch, that
     plugs into the mating Hirose ZIF on the PelletPirate V4 PCB.

Signals cross F.Cu -> B.Cu through 39 staggered plated vias next to J1, then
run on B.Cu out to the gold fingers.

--------------------------------------------------------------------------------
2. GERBER / DRILL FILE MAP  (folder: /gerbers)   *** READ THE COVERLAY NOTE ***
--------------------------------------------------------------------------------
  ..-F_Cu.gbr .......... Front copper (Side A) - J1 pads + fanout
  ..-B_Cu.gbr .......... Back copper (Side B)  - traces + gold fingers
  ..-F_Mask.gbr ........ FRONT COVERLAY OPENINGS (see note)
  ..-B_Mask.gbr ........ BACK COVERLAY OPENINGS  (see note)
  ..-F_Paste.gbr ....... Solder paste stencil for J1 (front only)
  ..-f_silkscreen.gbr .. Front legend
  ..-b_silkscreen.gbr .. Back legend
  ..-Edge_Cuts.gbr ..... Board / flex outline (profile)
  ..-.drl .............. Excellon drill (PTH), absolute origin, mm
  ..-drl_map.gbr ....... Drill map (GerberX2)
  ..-job.gbrjob ........ Gerber job file

  *** COVERLAY NOTE ***
  This is a flex part with COVERLAY, not LPI solder mask. The KiCad "F.Mask"
  and "B.Mask" layers are the COVERLAY OPENINGS on each side:
     - F.Mask -> front coverlay openings over the J1 solder pads.
     - B.Mask -> back  coverlay openings over the 39 gold fingers.
  BACK GOLD FINGERS: please expose all 39 fingers with a SINGLE coverlay
  window over the whole finger bank. The per-pad B.Mask openings are
  INDICATIVE ONLY (they mark finger locations); merge them into one window
  per your standard gold-finger coverlay tooling. Fingers span approx
  X 176.2-179.7 mm on the back of the mating tongue.

--------------------------------------------------------------------------------
3. STACK-UP (as designed - confirm/adjust per your flex process)
--------------------------------------------------------------------------------
  2-layer flex, total ~0.13 mm:
     Coverlay (front)
     Copper F.Cu     0.035 mm
     Core (polyimide, drawn as 0.060 mm)
     Copper B.Cu     0.035 mm
     Coverlay (back)
  Finished plating on mating fingers: ENIG (0.2 um Au / 1-5 um Ni per Hirose).
  Stiffeners at BOTH connector ends - construction/thickness per FPCWay
  (connector-mount stiffener proposed as 0.15 mm SUS).

--------------------------------------------------------------------------------
4. KEY DIMENSIONS  (Hirose FH26W-39S-0.3SHW Recommended FPC Dimensions)
--------------------------------------------------------------------------------
  Signals ....................... 39, pin-1 -> pin-39 straight through
  Contact pitch ................. 0.30 mm
  Gold-finger conductor width ... 0.20 mm   (Hirose recommended)
  Gold-finger conductor gap ..... 0.10 mm   (Hirose recommended)
  Gold-finger tip lead-in ....... R0.2 max  (supplier to add per datasheet)
  Male tongue width F / C / B ... 12.0 / 11.4 / 10.8 mm
  Main body width ............... 14.0 mm
  Overall length ................ ~75.0 mm
  Signal trace width / clearance. 0.07 mm / 0.07 mm  (supplier to confirm)
  Layer-transition vias ......... 39 x 0.20 mm pad / 0.10 mm drill
  Male tongue finished thickness. 0.20 +/- 0.03 mm

  REV G CORRECTION: the gold-finger conductor width was corrected from 0.15 mm
  to 0.20 mm (0.10 mm gap) so the mating pattern now matches the Hirose
  Recommended FPC Dimensions. This resolves the golden-finger fit question
  raised in DFM.

--------------------------------------------------------------------------------
5. PRE-DFM ITEMS FOR FPCWAY TO VALIDATE / QUOTE
--------------------------------------------------------------------------------
  a) 2-layer flex construction and via reinforcement.
  b) *** VIA ANNULAR RING ***: vias are 0.20 mm pad / 0.10 mm drill = 0.05 mm
     annular per side. Please CONFIRM your flex process reliably supports
     0.05 mm annular on a 0.10 mm drill. The dense 0.30 mm-pitch fanout limits
     enlarging the via pads; advise if you need a different via/annular.
  c) 0.07 mm trace/space manufacturability (approve or revise).
  d) Single-window coverlay over the 39 gold fingers (see section 2).
  e) Gold-finger exposed length/width, ENIG plating, and R0.2 tip lead-in.
  f) 0.20 +/- 0.03 mm finished male mating thickness + stiffener stack.
  g) Pin-1 mapping / contact-side orientation verified at BOTH mating
     interfaces against bottom-contact FH26W-39S mating.
  h) 100% continuity + short test on all 39 circuits after assembly.

--------------------------------------------------------------------------------
6. OTHER FOLDERS
--------------------------------------------------------------------------------
  /kicad_source .. Current KiCad 10 project (schematic, board, libs, Hirose
                   footprint + .step) for your DFM reference.
  /docs .......... Regenerated BOM and connector placement (CPL) from the
                   current CAD.

Contact: Brian Beardmore  -  bbeardmore@carvizor.com
================================================================================
