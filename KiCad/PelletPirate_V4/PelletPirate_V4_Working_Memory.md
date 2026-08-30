# PelletPirate V4 Working Memory

Date saved: 07/24/2026
Project folder: C:\development\pelletpirate\kicad\pelletpirate_v4
Purpose: durable project-local memory for continuing PelletPirate V4 schematic, placement, routing, and production-readiness work in a new chat session.

## Related Files

- Formal V3 audit already exists in the V3 project folder:
  - C:\development\pelletpirate\kicad\pelletpirate_v3\PelletPirate Architecture Audit 07-24-2026.md
  - Filename uses hyphens because Windows cannot store slash characters in filenames.
- Formal V4 placement/architecture audit for this session:
  - PelletPirate_V4_Placement_Audit_07-24-2026.md
- V4 handoff files:
  - PelletPirate_V4_Working_Memory.md
  - PelletPirate_V4_Placement_And_Routing_Handoff_2026-07-24.md
  - PelletPirate_V4_Routing_Plan.md

## Last Inspected State

Inspection date: 07/24/2026.

Active V4 files inspected:

| File | Last write time | Size |
| --- | --- | ---: |
| PelletPirate_V4.kicad_sch | 07/23/2026 02:49:30 AM | 725274 bytes |
| PelletPirate_V4.kicad_pcb | 07/24/2026 01:07:50 AM | 1683586 bytes |
| PelletPirate_V4.kicad_pro | 07/24/2026 01:08:08 AM | 21661 bytes |
| PelletPirate_V4.kicad_dru | 07/22/2026 11:45:12 PM | 707 bytes |
| fp-lib-table | 07/23/2026 12:49:32 AM | 4470 bytes |

PCB status at inspection:

- Board outline: 65 mm x 110 mm, Edge.Cuts rectangle from x=120..185 and y=40..150.
- Footprints: 121.
- Routed track segments: 0.
- Vias: 0.
- One locked full-board GND zone exists near line 44812. Treat it as a placeholder/stale pour until isolation keepouts and constraints are hardened.
- Bottom silkscreen still says "PelletPirate v3.0 2026"; this is a V4 cleanup item.
- Local custom libraries are present in the V4 folder, including USB, ESP32, FPC, PBO, MOC, H11AA1, inductor, capacitor, varistor, fuse, jack, switch, and LED-driver libraries.

No KiCad schematic, PCB, project, DRC, symbol, or footprint files were modified when this memory file was created.

## Current Architecture Summary

PelletPirate V4 is an ESP32-S3 based pellet grill controller board with integrated AC mains power conversion, triac load switching, LCD interface, RTD temperature measurement, USB-C, buttons/encoder/user IO, and low-voltage power regulation.

Major subsystems:

- AC mains input and protection: wire pads, fuse F1, MOV1, X2/safety-cap area, common-mode choke LF1, PBO-5F-12 AC/DC module PS1, earth pad, primary-side capacitors.
- 12 V rail: PS1 output/filtering, protection diode D1, inductor L1, bulk capacitors C13/C15 and related output filtering.
- 3.3 V rail: buck converter PS2, L2, input/output capacitors, feeds ESP32-S3, MAX31865s, logic, USB-related circuits, and low-voltage controls.
- LCD/backlight rail: boost LED driver PS3, L3, D2, feedback/sense network, expected route to FPC1.
- MCU: ESP2 ESP32-S3-WROOM-1U-N16R8 module, USB, boot/reset/buttons, control signals to MAX31865 devices, LCD, triac optos, and zero-cross input.
- RTD inputs: five 3.5 mm jacks J1-J5 feeding five MAX31865 front ends MAX1-MAX5.
- AC load outputs: three triac channels T1-T3 driven through optocouplers MOC1-MOC3 and load wire pads WirePad3-WirePad5.
- Zero-cross detection: H11AA1M U3 with R41/R42 15k input resistors and R43 10k logic-side resistor.

## Fixed Mechanical Constraints

These placements are assumed mechanically constrained by the enclosure, LCD ribbon, edge access, or external harnesses:

- FPC1: LCD FPC connector on B.Cu at 136.4,146.925. Must stay where the LCD ribbon reaches.
- RTD jacks:
  - J1 B.Cu at 171.2,44.9 -90.
  - J2 B.Cu at 158.6,44.9 -90.
  - J3 B.Cu at 146,44.9 -90.
  - J4 B.Cu at 133.4,44.9 -90.
  - J5 B.Cu at 124.92,59.46.
- USB-C connector:
  - J8 F.Cu at 122.77,83.575 -90.
- Board outline:
  - Edge.Cuts rectangle from 120,40 to 185,150.
- Other enclosure-facing connectors, load wire pads, switches, and controls may also be constrained, but only the user can confirm which ones are absolutely fixed.

## Placement Decisions Already Made

- V4 starts from a clean PCB with all components placed and grouped by subsystem.
- RTD/MAX31865 placement is generally grouped near RTD jacks and is a better starting point than routing from a random placement.
- PS1 mains front-end parts are compact enough to evaluate as a system before routing.
- PS2 buck converter local switch loop appears reasonably compact: PS2 to L2 is about 5.55 mm.
- No routing exists yet, which is ideal for making safety and placement corrections before committing track topology.

## Important Component Coordinates

| Ref | Location/layer | Notes |
| --- | --- | --- |
| FPC1 | 136.4,146.925 B.Cu | Fixed LCD ribbon connector |
| J8 | 122.77,83.575 F.Cu | Fixed USB-C connector |
| U1 | 141.865,92.28 F.Cu | USB ESD, currently about 21 mm from J8 |
| ESP2 | 152.31,79.935 F.Cu | ESP32-S3-WROOM-1U-N16R8 |
| PS1 | 165.4,111.4 F.Cu | PBO-5F-12 AC/DC module |
| PS2 | 171.95,60 B.Cu | 3.3 V buck |
| PS3 | 143.1075,55.7125 B.Cu | LCD/backlight boost driver |
| U3 | 161,118.3 B.Cu | H11AA1M zero-cross optocoupler |
| MOC1 | 130.11,104.46 B.Cu | Triac optocoupler |
| MOC2 | 143.54,103.51 B.Cu | Triac optocoupler |
| MOC3 | 157.74,103.31 B.Cu | Triac optocoupler |
| T1 | 150.25,146.88 F.Cu | Triac near FPC1 region |
| T2 | 163,146.8 F.Cu | Triac near bottom edge |
| T3 | 175.8,145.68 F.Cu | Triac near bottom/right edge |
| WirePad3 | 155.7,147.1 F.Cu | AC load pad near bottom edge |
| WirePad4 | 168.6,147.1 F.Cu | AC load pad near bottom edge |
| WirePad5 | 180.5,140.3 F.Cu | AC load pad |
| R41 | 149.1,114.6 B.Cu | 15k zero-cross input resistor |
| R42 | 151.5,112.5 F.Cu | 15k zero-cross input resistor |
| R43 | 159.51,109.2 F.Cu | 10k logic-side resistor |

## Known Placement Risks

P0 risks must be resolved before production routing:

- FPC1 is very close to T1 and AC load pads. FPC1 is low-voltage/LCD; triacs and load pads are mains/load circuitry. Since FPC1 is fixed, the triac/load area must either move away or receive a hard isolation strategy: keepout, slot, no copper corridor, and routing constraints that keep mains away from the FPC side.
- Current DRC rules are not hard enough for a mixed mains/SELV product. The custom mains-to-low-voltage clearance is only 1.0 mm, MAINS class clearance is 0.5 mm, and Default clearance is 0.195 mm. Define production creepage/clearance rules before routing.
- The locked full-board GND zone must not be trusted until primary/secondary/earth keepouts are explicit. It currently spans the board region and can obscure isolation mistakes if refilled without constraints.
- ESP2 exposed pad/thermal pad mapping from V3 remains a critical check in V4. PCB has repeated footprint pads named "41"; schematic symbol uses the ESP32-S3 custom symbol. Confirm pad 41 is actually tied to GND and not left unnetted.
- Zero-cross input resistor chain uses R41/R42 = 15k each at 1206 size. Confirm voltage rating, power dissipation, surge capability, and whether a larger series stack is required for 120 VAC mains.

P1 risks:

- MOC1/MOC2/MOC3 are about 46 mm from their respective triacs. This creates long triac gate loops and more susceptibility to false triggering/noise.
- USB ESD U1 is about 21 mm from the USB-C connector J8. Move ESD protection close to J8.
- PS3 boost driver is about 90 mm from FPC1. If it drives LCD backlight pins, the LED+/LED- route will be long unless PS3 moves closer or a protected SELV corridor is reserved.
- H11AA1 U3 input side and output side need a visible isolation split. R41/R42 should remain on the mains side; R43 should remain on the logic side.
- PS1 secondary/filter components are spread toward the right edge; route the 12 V loop wide and short and keep it away from primary AC.

P2/P3 risks:

- Add fiducials and test points before PCBWay assembly.
- Add programming/debug/service access if not already covered.
- Confirm all custom footprints have correct pad numbers, hole sizes, courtyard, 3D models where useful, and assembly clearances.
- Update V4 silkscreen text that still says "v3.0".
- Confirm BOM values, voltage ratings, package ratings, and MPN availability.

## Netclass And Routing Width Assumptions

Current V4 netclasses:

- Default: clearance 0.195 mm, track width 0.2 mm.
- MAINS: clearance 0.5 mm, track width 1.5 mm, via 1.2/0.6 mm.
- Signal: clearance 0.4 mm, track width 0.4 mm.
- MAINS patterns include AC load/neutral, PS1 AC pins, PS1 cap pins, triac A1/gate nets, MOC pad 6 nets, C1-C3 pad 1 nets, and U2 IN1/IN2 patterns.

Working routing assumptions requested/discussed with user:

- Fine signal: 0.2 mm where needed, 0.3 mm preferred when space allows.
- General logic signal: 0.3 mm.
- USB D+/D-: route as a controlled differential pair per board stackup; do not treat as random 0.3 mm signal. ESD should be at J8.
- 3.3 V: 0.3 mm minimum, wider for trunk feeds where space allows.
- 12 V: 0.5 mm minimum, wider for source/load trunks and PS1-to-buck feed.
- LED boost/backlight: at least 0.5 mm for power path; keep switch node compact and away from sensitive/edge wiring.
- AC load / AC neutral / triac current path: 1.5 mm minimum, wider if current/temperature calculations require it.
- Mains-to-SELV spacing: do not rely on current 1.0 mm rule. Set actual production creepage/clearance targets before routing.

## P0/P1/P2/P3 Fix List

P0:

1. Harden DRC and netclasses before routing.
2. Resolve FPC1-to-triac/load proximity.
3. Verify and fix ESP2 exposed pad/thermal pad grounding.
4. Rework/verify H11AA1 mains resistor ratings and placement.
5. Ensure all primary, secondary, earth, and SELV boundaries are explicit on the PCB.

P1:

1. Move USB ESD U1 next to J8.
2. Move MOC1-MOC3 closer to T1-T3 or otherwise shorten triac gate loops.
3. Evaluate moving PS3 closer to FPC1, or reserve a protected LED backlight route.
4. Add isolation slots/keepouts where mains and SELV must pass near each other.
5. Add fiducials and test points before routing is considered complete.

P2:

1. Tighten PS1 secondary routing and bulk-cap placement if possible.
2. Improve RTD input ESD/protection strategy.
3. Confirm MAX31865 routing symmetry and Kelvin/reference resistor layout.
4. Confirm USB CC, shield, and chassis/earth handling.

P3:

1. Update stale V3 silkscreen.
2. Confirm all custom footprint documentation and 3D models.
3. Clean drawing/user guide layers.
4. Prepare PCBWay fab/assembly notes after layout passes DRC/ERC.

## Exact Next Steps

1. Do a placement-hardening pass before routing.
2. Confirm which non-listed mechanical parts are absolutely fixed.
3. Update or approve DRC/netclass policy for mains, load, SELV, USB, RTD, 3.3 V, 12 V, and LED boost nets.
4. Fix ESP2 exposed-pad mapping if V4 still has the V3 pad 41 issue.
5. Move U1 closer to J8.
6. Resolve FPC1 vs T1/load-pad conflict.
7. Shorten MOC-to-triac gate loops.
8. Decide whether to move PS3 closer to FPC1 or reserve a protected LED boost corridor.
9. Add fiducials/testpoints/service points.
10. Then route in this order: AC mains/PS1, triac/load current paths, isolated zero-cross, 12 V, 3.3 V, ESP32/USB, RTD/MAX31865, LCD/FPC, then pours.
11. Run DRC after every major subsystem routing pass.

## Things Not To Change Without User Approval

- Do not move FPC1 unless the LCD ribbon/enclosure constraint changes.
- Do not move J1-J5 RTD jacks unless the enclosure constraint changes.
- Do not move J8 USB-C unless the enclosure constraint changes.
- Do not change the board outline without user approval.
- Do not route mains or AC load tracks near FPC1 just because the current placement makes it easy.
- Do not trust the locked full-board GND zone until keepouts and isolation rules are in place.
- Do not route before constraints are hardened.
- Do not modify KiCad schematic/PCB/project/library files unless the user explicitly approves edits.

## Open Questions For User

1. Are T1/T2/T3 and WirePad3/WirePad4/WirePad5 mechanically fixed, or can the triac/load-output group move away from FPC1?
2. What is the intended maximum current for each AC load output and total board load?
3. Is this 120 VAC only, or must spacing/rating support 240 VAC too?
4. What PCB stackup, copper weight, and minimum trace/space should PCBWay use?
5. Is the target PCBWay process 2-layer, 1 oz copper, standard FR-4 unless specified otherwise?
6. What exact LCD backlight current/voltage does PS3 need to support?
7. Should USB shield tie to earth/chassis, logic ground, RC network, or isolated chassis strategy?
8. Which parts must be hand-solderable vs assembly-house placed?
9. Do you want Codex to patch the V4 KiCad files directly after the placement plan is approved?

## Session Checkpoint 2026-07-25

User returned and asked to proceed one step at a time. We resumed from the V4 handoff files and focused only on constraints/netclasses/custom rules, not routing or placement edits.

Confirmed design basis:

- AC input is 120 VAC only.
- PCB target is PCBWay standard 2-layer FR-4.
- Copper weight decision: use 2 oz copper, not 3 oz. Update stackup copper thickness from 0.035 mm to 0.070 mm on F.Cu and B.Cu when editing the KiCad board setup.
- Load currents:
  - Igniter: 3 A max.
  - Auger motor: 0.75 A max.
  - Fan: 0.50 A max.
  - Worst-case shared AC feed/neutral current: 4.25 A.

Important correction from the constraints discussion:

- Do not set high-current AC netclass default widths to the full desired ampacity width if that prevents escaping TO-220 triac pads.
- Netclass track widths should be routable pad-escape defaults.
- Carry current with short neckdowns at pads and wider copper pours/fills/traces once the route exits cramped geometry.
- For 2 oz copper, current working routing defaults:
  - `AC_LOAD_SHARED`: 0.80 mm default track width.
  - `AC_LOAD_IGNITER`: 0.80 mm default track width.
  - `AC_LOAD_LOW`: 0.60 mm default track width.
  - `MAINS_GATE`: 0.40 mm default track width.
  - `MAINS_SENSE`: 0.40 mm default track width.
- Actual copper targets after fanout:
  - Shared fused hot/neutral trunks: 1.5-2.0 mm pours/shapes where space opens.
  - Igniter branch: 1.2-1.5 mm after triac escape.
  - Auger/fan branches: 0.8-1.0 mm after triac escape.
  - MOC-to-triac gate traces: 0.3-0.4 mm.

Custom-rule lesson:

- Broad 3.0 mm mains-domain-to-non-mains custom rules created huge red clearance halos around every MOC, triac, and power pad. This made placement/routing unusable and is not the right working environment.
- Do not use broad global mains-to-SELV clearance rules during active placement/routing.
- Use netclasses for routing defaults and local clearances.
- Use deliberate isolation geometry later: keepout corridors, rule areas, slots, and targeted rules at real boundaries only.
- Real isolation boundaries to handle later:
  - PS1 primary-to-secondary.
  - MOC optocoupler input/output gaps.
  - U3 H11AA1 input/output gap.
  - FPC1 versus triac/load-output corridor.
  - AC input/load region versus ESP/RTD/LCD/USB region.

Footprint-rule lesson:

- Manufacturer footprints should not be invalidated by broad external clearance rules inside the package geometry.
- If broader external isolation rules are reintroduced later, add same-footprint internal exceptions for PS1, MOC1-MOC3, U3, and T1-T3 at the bottom of the custom rules file because later KiCad custom rules have priority.
- The user saw this KiCad message: "No errors found. Rules 'PS1 internal manufacturer footprint' and 'PS1-internal' share the same condition."
- Resolution: keep only one PS1 internal rule. Delete the older duplicate `PS1-internal`.

Current recommended minimal custom rules while doing placement/netclass setup:

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

Optional internal footprint exceptions to re-add only if broader external clearance rules return:

- MOC1/MOC2/MOC3 internal manufacturer footprint clearance min 0.25 mm.
- U3 internal manufacturer footprint clearance min 0.25 mm.
- T1/T2/T3 internal manufacturer footprint clearance min 0.25 mm.

Start here next session:

1. Confirm the current KiCad custom rules file has only the minimal PS1 rules above, or the equivalent cleaned version without duplicate PS1 conditions.
2. Confirm stackup F.Cu/B.Cu copper thickness is set for 2 oz: 0.070 mm.
3. Confirm netclass default track widths use pad-escape values, not full ampacity values.
4. Assign actual V4 nets to the new netclasses.
5. Only after the board is visually usable again, continue with component placement/refactor decisions.
