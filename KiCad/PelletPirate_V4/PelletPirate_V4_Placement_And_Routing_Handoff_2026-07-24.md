# PelletPirate V4 Placement And Routing Handoff

Date: 07/24/2026
Scope: handoff for continuing V4 placement hardening and routing from a new chat session.
Project path: C:\development\pelletpirate\kicad\pelletpirate_v4

## Read This First In A New Session

The V4 board is placed but unrouted. Do not start routing until the placement risks and design-rule issues below are addressed.

Primary instruction for the next session:

1. Read this file.
2. Read PelletPirate_V4_Working_Memory.md.
3. Read PelletPirate_V4_Routing_Plan.md.
4. Inspect only the V4 KiCad files and V4 local libraries unless the user explicitly asks for another folder.
5. Do not modify schematic, PCB, project, DRC, symbol, or footprint files until the user approves edits.

## Current Board State

- Board outline: 65 mm x 110 mm, x=120..185, y=40..150.
- Components placed: 121 footprints.
- Segments: 0.
- Vias: 0.
- A locked GND zone exists in the PCB near line 44812. Treat as placeholder/stale until keepouts are explicit.
- V4 still contains bottom silkscreen text saying "PelletPirate v3.0 2026".
- Project has local custom footprint and symbol libraries in the V4 folder.

## Architecture In One Page

PelletPirate V4 combines these subsystems on one mixed-voltage PCB:

- Mains input/protection: WirePad1/WirePad2, F1, MOV1, C4, LF1, PS1 AC input, earth pad, and primary caps.
- Isolated AC/DC conversion: PS1 PBO-5F-12 generates nominal 12 V.
- 12 V filtering/protection: L1, D1, C13, C15, C7/C12 area and related components.
- 3.3 V conversion: PS2 buck with L2 and local capacitors.
- LCD backlight boost: PS3 with L3, D2, sense/feedback parts and route to FPC1.
- MCU and logic: ESP2 ESP32-S3-WROOM-1U-N16R8, USB-C, buttons/encoder/control IO.
- RTD measurement: J1-J5 jacks with MAX1-MAX5 MAX31865 front ends.
- AC load control: MOC1-MOC3 optos driving T1-T3 triacs and WirePad3-WirePad5 load outputs.
- Zero-cross detection: U3 H11AA1M with R41/R42 15k input resistors and R43 10k output/logic-side resistor.

## Fixed Or Likely Fixed Mechanical Items

Do not move these without explicit user approval:

- FPC1 at 136.4,146.925 B.Cu: fixed by LCD ribbon.
- J1 at 171.2,44.9 B.Cu: RTD jack at edge.
- J2 at 158.6,44.9 B.Cu: RTD jack at edge.
- J3 at 146,44.9 B.Cu: RTD jack at edge.
- J4 at 133.4,44.9 B.Cu: RTD jack at edge.
- J5 at 124.92,59.46 B.Cu: RTD jack at edge.
- J8 at 122.77,83.575 F.Cu: USB-C edge connector.
- Edge.Cuts outline from 120,40 to 185,150.

Ask user whether these are fixed before moving:

- T1/T2/T3 triacs.
- WirePad3/WirePad4/WirePad5 AC load pads.
- Buttons/encoder/user controls.
- Earth1 and primary input wire pads.

## Placement Assessment

Good starting points:

- The board is unrouted, so major mistakes can still be prevented.
- RTD jacks and MAX31865 devices are grouped in a reasonable general layout.
- PS2 and L2 are close enough for a compact buck switch loop.
- PS1 front-end parts are grouped compactly enough to draw a clear primary/secondary boundary.

High-risk placement observations:

- FPC1 is near the triac/load-output group. T1 is about 13.9 mm from FPC1 and WirePad3 is about 19.3 mm from FPC1. This is not automatically unsafe by distance alone, but it is a major routing risk because low-voltage FPC/LCD wiring and mains/load wiring could easily be forced into the same bottom-edge region.
- MOC1/MOC2/MOC3 are about 46 mm from T1/T2/T3. That makes long triac gate routing likely.
- U1 USB ESD is about 21 mm from J8. ESD should be placed at the connector.
- PS3 is about 90 mm from FPC1. If it drives LCD backlight pins, the LED backlight route is long.
- U3/R41/R42/R43 need a clear primary/secondary split. R41/R42 are 15k each and must be validated for mains voltage, power, and surge duty.
- The board has a locked full-board GND zone before final isolation rules. This should be removed, unlocked, or replaced after keepouts are defined.

## P0 Items Before Routing

1. Harden mains/SELV DRC.
   - Current `PelletPirate_V4.kicad_dru` has a 1.0 mm mains-to-low-voltage rule. That is not enough for production mains isolation without a reviewed standard and enclosure context.
   - Current MAINS class clearance is 0.5 mm and Default is 0.195 mm.
   - Add explicit netclasses or constraints for primary mains, AC load, earth/chassis, SELV logic, USB, RTD, 12 V, 3.3 V, and LED boost.

2. Resolve FPC1 vs triac/load proximity.
   - Since FPC1 is fixed, either move the triac/load hardware away or create a hard isolation strategy before routing.
   - Do not route AC load/neutral along the FPC edge.

3. Verify ESP2 exposed pad mapping.
   - PCB footprint contains repeated pads named "41" for the ESP32 exposed pad/thermal via region.
   - Confirm schematic symbol maps pad 41 to GND correctly in V4. The V3 audit flagged this as critical.

4. Rework/verify zero-cross input network.
   - R41/R42 are 15k each and connect to H11AA1M input.
   - Confirm voltage rating, power dissipation, surge rating, and spacing. Consider larger series resistor stack.

5. Do not route with the current locked GND zone active as the trusted final pour.
   - It spans both layers over the board region. Rebuild zones after keepouts/isolation are defined.

## P1 Placement Refactors

1. Move U1 USB ESD close to J8.
   - Place at the USB connector side of the D+/D- path.
   - Route J8 -> U1 -> ESP2.

2. Shorten MOC-to-triac gate loops.
   - Move MOC1 closer to T1, MOC2 closer to T2, MOC3 closer to T3 if mechanically possible.
   - Preserve optocoupler primary/secondary orientation and creepage.

3. Review PS3 location.
   - Best electrical placement is closer to FPC1 if PS3 drives LCD LED pins.
   - If moving PS3 closer to FPC1 conflicts with mains isolation, keep PS3 where it is but reserve a wide, protected SELV route corridor.

4. Clarify the primary/secondary boundary around PS1 and U3.
   - Use silkscreen/drawing/keepout guides to prevent routing ambiguity.

5. Add fiducials and test points.
   - Needed before PCBWay assembly readiness.

## P2/P3 Cleanup

P2:

- Add RTD input protection strategy.
- Verify MAX31865 reference resistor placement and Kelvin-style routing where applicable.
- Confirm PS1 secondary filter loop and buck input route widths.
- Confirm USB shield/earth/GND strategy.
- Confirm thermal relief settings for AC load pads, triacs, and power connectors.

P3:

- Update stale V3 silkscreen.
- Clean labels and assembly drawings.
- Confirm MPNs and BOM values.
- Confirm courtyard and 3D model quality for custom footprints.
- Generate PCBWay-ready Gerbers, drill files, position files, BOM, and assembly drawings only after DRC/ERC pass.

## Recommended Routing Sequence

1. Constraints and keepouts first.
2. Mechanical/fixed-connector confirmation.
3. Placement refactor pass.
4. AC mains and PS1 primary routing.
5. PS1 secondary/12 V routing.
6. Triac/load current routing.
7. Zero-cross routing.
8. 3.3 V buck routing.
9. ESP32/USB routing.
10. RTD/MAX31865 routing.
11. LCD/FPC/backlight routing.
12. GND and power pours.
13. Full DRC/ERC, visual review, manufacturing package.

## Things Not To Change

- Do not move FPC1, J1-J5, J8, or board outline unless user explicitly approves.
- Do not modify V4 KiCad files from a future session until the user asks for edits.
- Do not start routing before DRC/netclasses are hardened.
- Do not leave mains/SELV isolation dependent on visual spacing only.
- Do not route USB D+/D- as arbitrary signal traces.
- Do not leave ESP2 exposed pad unverified.

## User Questions To Resolve Next

1. Are triacs T1-T3 and load pads WirePad3-WirePad5 mechanically fixed?
2. What current must each AC output support?
3. Is the board 120 VAC only?
4. What copper weight and layer count will be ordered?
5. Should Codex directly patch the V4 KiCad files after the refactor list is approved?

## Session Update 2026-07-25

Resolved inputs:

- 120 VAC only.
- 2-layer FR-4.
- 2 oz copper selected. Do not use 3 oz unless later routing proves 2 oz impossible.
- Load currents: igniter 3 A, auger 0.75 A, fan 0.50 A, shared feed/neutral worst case 4.25 A.

Important workflow correction:

- Do not use a blanket 3.0 mm custom clearance halo around all mains-related netclasses while placing/routing. It makes the MOC and triac footprints unusable and is not the right working method.
- Use netclasses for routable default widths.
- Use local neckdowns at pads, then expand into wide copper pours/shapes.
- Use targeted keepout/rule areas later for real mains-to-SELV boundaries.

Current preferred AC routing defaults for 2 oz copper:

| Netclass | Default track width |
| --- | ---: |
| `AC_LOAD_SHARED` | 0.80 mm |
| `AC_LOAD_IGNITER` | 0.80 mm |
| `AC_LOAD_LOW` | 0.60 mm |
| `MAINS_GATE` | 0.40 mm |
| `MAINS_SENSE` | 0.40 mm |

Actual current-carrying copper should expand after fanout:

- Shared AC feed/neutral: 1.5-2.0 mm copper pours/shapes where space opens.
- Igniter switched branch: 1.2-1.5 mm after triac escape.
- Auger/fan switched branches: 0.8-1.0 mm after triac escape.
- Gate/sense traces: 0.3-0.4 mm is acceptable because these are not load-current paths.

Current minimal custom rules for active placement work:

```scheme
(version 1)

(rule "PS1 internal manufacturer footprint"
(condition "A.memberOfFootprint('PS1') && B.memberOfFootprint('PS1')")
(constraint clearance)
(severity ignore))

(rule "PS1-edge"
(condition "A.memberOfFootprint('PS1') || B.memberOfFootprint('PS1')")
(constraint edge_clearance (min 0.25mm)))
```

Delete duplicate `PS1-internal` if both it and `PS1 internal manufacturer footprint` are present.

Next single step:

1. Clean custom rules to the minimal state above.
2. Confirm 2 oz stackup and netclass default widths.
3. Assign V4 nets to the new netclasses.
