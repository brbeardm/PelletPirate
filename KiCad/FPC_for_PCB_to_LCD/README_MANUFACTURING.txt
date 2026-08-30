PCB_to_LCD_FPC — 39-pin 0.3 mm FPC extension
PRELIMINARY DESIGN FOR KICAD REVIEW / QUOTATION

Connector:
  Hirose FH26W-39S-0.3SHW(60)
  HRS CL0580-2412-8-60
  39 positions, 0.3 mm contact pitch, bottom contact, ZIF
  Connector body: 13.2 mm long x 3.2 mm deep x 1.0 mm high
  Recommended mating FPC thickness: 0.20 +/- 0.03 mm, gold plated.

Board concept:
  Type: 1-layer polyimide flex
  Overall length: 75.0 mm
  Head width: 14.0 mm
  Tail width: 12.0 mm
  Copper side: B.Cu only
  Copper: 18 um RA recommended
  Trace width: 0.10 mm
  Tail finger width: 0.10 mm
  Tail finger pitch: 0.30 mm
  Tail exposed length: 2.50 mm
  Pin mapping: 1->1, 2->2, ... 39->39

IMPORTANT ORIENTATION DECISION:
  J1 is intentionally placed on B.Cu and the exposed male tail fingers are also on B.Cu.
  This permits a one-layer flex while keeping both interfaces compatible with bottom-contact
  ZIF connectors without requiring 39 layer-transition vias.

STIFFENERS:
  Tail stiffener must be selected so FINISHED mating thickness at the male end is
  0.20 +/- 0.03 mm. If the base finished flex is ~0.10 mm, the nominal added PI
  stiffener will be approximately 0.10 mm, subject to FPCWay stack-up confirmation.
  Connector assembly area has a separate stiffener outline. Ask FPCWay assembly engineering
  to confirm the minimum local stiffener construction required for reliable reflow of J1.

FPCWAY QUOTE CORRECTION:
  Do NOT quote this as 12.5 mm maximum width: the FH26W-39S connector is 13.2 mm long.
  Use an overall bounding size of approximately 14.0 mm x 75.0 mm.
  The cable necks down to the Hirose recommended 12.0 mm FPC width after the connector area.

FILES:
  PCB_to_LCD_FPC.kicad_pcb              KiCad PCB source
  PCB_to_LCD_FPC.kicad_pro              KiCad project container
  PCB_to_LCD_FPC_B_Cu.gbr               bottom copper
  PCB_to_LCD_FPC_B_Mask.gbr             bottom coverlay / mask openings
  PCB_to_LCD_FPC_Edge_Cuts.gbr          flex outline
  PCB_to_LCD_FPC_Stiffener_Connector.gbr connector stiffener outline
  PCB_to_LCD_FPC_Stiffener_Tail.gbr      tail stiffener outline
  PCB_to_LCD_FPC_BOM.csv                 assembly BOM
  PCB_to_LCD_FPC_Positions.csv           component placement
  PCB_to_LCD_FPC_Net_Map.csv             1:1 pin mapping

NO DRILL FILE:
  This design contains no plated or non-plated drilled holes.

REVIEW BEFORE ORDER:
  1. Open .kicad_pcb and verify J1 pad numbering against your actual LCD tail pin-1 orientation.
  2. Confirm which physical side of the LCD tail has exposed contacts.
  3. Confirm the PelletPirate PCB mating connector is bottom-contact and that its pin 1
     orientation matches this tail.
  4. Ask FPCWay to DFM-review 0.10 mm traces at 0.30 mm pitch and the local reflow stiffener.
  5. Confirm coverlay openings and gold finish at the tail fingers.
  6. Do not release to production until pin-1 orientation is physically checked with the LCD.

Revision: PRELIM-A
