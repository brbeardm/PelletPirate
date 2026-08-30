COMPLETE PROJECT PACKAGE

Open PCB_to_LCD_FPC.kicad_pro in KiCad Project Manager.

Included:
PCB_to_LCD_FPC.kicad_pro   current project file
PCB_to_LCD_FPC.kicad_pcb   PCB layout
PCB_to_LCD_FPC.sch         complete 39-pin schematic
PCB_to_LCD_FPC-cache.lib   embedded legacy symbol cache required to load schematic
PCB_to_LCD_FPC.pro         legacy project compatibility pointer
FPC_BOM.csv
Net_Map.csv
Critical_Dimensions.csv
README_MANUFACTURING.txt

WHY .sch IS INCLUDED:
KiCad's official documentation states that legacy .sch schematics are readable and converted to .kicad_sch on write, and that -cache.lib is required for proper loading of a legacy schematic. Both are included so you do not have to find or install a symbol library.

When you first open/save the schematic, KiCad will create PCB_to_LCD_FPC.kicad_sch. After that, the project is fully native-current format.

DESIGN:
LCD male -> J1 FH26W-39S female -> flex -> integral male tongue -> FH26W-39S female on PCB V4.
39 positions, 0.30 mm pitch, electrical intent 1->1 through 39->39.
14 mm body tapers symmetrically to 12 mm male mating tongue.
Male mating thickness 0.20 +/-0.03 mm.

PRE-DFM: FPCWay must verify exact Hirose contact-side orientation, Pin 1, gold-finger geometry, connector land pattern and stiffeners before fabrication.
