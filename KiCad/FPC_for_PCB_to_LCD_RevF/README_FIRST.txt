PCB_to_LCD_FPC REV F — COMPLETE PRE-DFM PACKAGE

FUNCTIONAL REQUIREMENT
LCD integral male FPC plugs into J1 Hirose female connector mounted on FRONT/Side A of this flex.
At the opposite end, this flex has an integral 39-position male gold-finger tongue on BACK/Side B.
That male tongue plugs into the Hirose ZIF on PCB V4.

REV F represents this literally as a 2-copper-layer flex:
- J1 SMT pads and short fanout on F.Cu
- 39 plated vias in the connector/stiffened region
- long traces and male gold fingers on B.Cu
- 14 mm body tapering symmetrically to 12 mm male tongue
- electrical intent 1->1 through 39->39

IMPORTANT: THIS IS PRE-DFM, NOT RELEASED FOR FAB.
FPCWay must determine whether the same functional orientation can be achieved with a 1-layer flex by changing
connector/stack orientation, or whether 2 layers/vias are required. If 2 layers are required, revise the quote.
Supplier must verify the exact current Hirose land pattern, contact side, Pin 1 orientation, via construction,
coverlay openings, stiffeners, gold-finger geometry and finished mating thickness before production.
