# PelletPirate V4 Routing Plan

Date: 07/24/2026
Project path: C:\development\pelletpirate\kicad\pelletpirate_v4
Status: placement exists, routing has not started.

## Routing Readiness Gate

Do not route until all gate items are complete:

1. Confirm fixed mechanical components.
2. Harden netclasses and custom DRC.
3. Resolve FPC1 vs triac/load placement risk.
4. Verify ESP2 pad 41 exposed-pad grounding.
5. Move USB ESD U1 next to J8.
6. Decide whether to move PS3 closer to FPC1.
7. Shorten MOC-to-triac gate loops if mechanically possible.
8. Add or reserve fiducials/test points.
9. Replace, unlock, or constrain the full-board GND zone.

## Constraint Plan

Set up constraints before tracks:

| Class | Proposed track width | Clearance notes |
| --- | ---: | --- |
| Fine signal | 0.20 mm allowed, 0.30 mm preferred | Use only where needed |
| General signal | 0.30 mm | ESP32 GPIO, SPI, control |
| USB D+/D- | stackup-controlled | Differential pair; route J8 -> ESD -> ESP2 |
| 3.3 V local | 0.30 mm minimum | Wider trunks where possible |
| 12 V | 0.50 mm minimum | Wider for trunk/load feed |
| LED boost/backlight | 0.50 mm minimum | Keep switch node compact |
| AC mains/load | 1.50 mm minimum | Verify by current and copper weight |
| Earth/chassis | width per fault/current strategy | Keep distinct from SELV unless approved |
| Mains-to-SELV | TBD by production target | Current 1.0 mm rule is not enough |

Current V4 rules are a starting point only:

- Default clearance 0.195 mm, width 0.2 mm.
- MAINS clearance 0.5 mm, width 1.5 mm.
- Signal clearance 0.4 mm, width 0.4 mm.
- Custom DRC mains-to-low-voltage min clearance is 1.0 mm.
- `mains-to-moc-nc` rule has a fragile condition: `B.NetName == 'unconnected-(MOC*'`. Confirm or replace before relying on it.

## Isolation Strategy

Before routing:

1. Draw a clear primary/secondary/SELV boundary.
2. Keep PS1 primary, fuse, MOV, X2/safety cap, LF1 primary, H11AA1 input, MOC triac side, triac terminals, and AC load pads on the mains/load side.
3. Keep ESP32, USB, MAX31865, RTD jacks, LCD FPC, PS2, PS3 logic/control side, and low-voltage caps on the SELV side.
4. Keep earth/chassis intentional and separate from logic GND unless the design explicitly ties them.
5. Use keepouts or slots where primary and SELV must pass close.
6. Do not allow GND pour into primary or load regions.

## Placement Refactor Routing Dependencies

Routing is blocked by these placement decisions:

- FPC1 is fixed near bottom edge. T1/WirePad3 are too close for casual mains routing. Either move the triac/load group away or establish an isolation slot/keepout path before routing.
- MOC1-MOC3 should move closer to T1-T3 if possible to reduce triac gate loop length.
- U1 must move close to J8 to protect USB at the connector.
- PS3 should move closer to FPC1 if possible; otherwise reserve a protected LED backlight corridor.
- U3/R41/R42 should make the primary input resistor chain visually and electrically obvious.

## System-By-System Routing Order

### 1. AC Mains Input And PS1 Primary

Route first after constraints are hardened.

Goals:

- Short, wide, direct mains input path.
- Fuse first in line with AC input.
- MOV and X2/safety-cap placement and wiring consistent with surge/EMI intent.
- Maintain creepage/clearance to all SELV nets.
- Avoid routing under or near FPC1, USB, ESP2, MAX31865, or RTD traces.

Checks:

- No GND copper in primary region.
- No accidental copper islands crossing primary/SELV boundary.
- Correct netclass applied to all primary nets, including zero-cross input nets.

### 2. PS1 Secondary And 12 V

Goals:

- Wide 12 V output path from PS1 to filter/protection network.
- Keep D1/L1/C13/C15 routing low impedance.
- Route 12 V trunk to PS2 and any 12 V loads with 0.5 mm minimum, wider if current requires.

Checks:

- No 12 V route crosses primary boundary.
- Bulk caps connect cleanly with low return impedance.
- Thermal reliefs are appropriate for solderability and current.

### 3. Triac Load Channels

Goals:

- Route AC load/neutral/current paths with 1.5 mm minimum, adjusted for current/copper temperature rise.
- Keep triac gate loops short.
- Keep load paths away from FPC1, LCD, USB, ESP32, and RTD analog traces.

Checks:

- MOC triac-side pins and triac gates are in MAINS/load class.
- Adequate clearance around triac pins and load wire pads.
- Thermal paths and mechanical clearances for TO-220 parts are acceptable.

### 4. Zero-Cross Detector

Goals:

- Keep R41/R42 and U3 input side on primary/mains side.
- Keep R43 and U3 output side on SELV side.
- Maintain optocoupler isolation.

Checks:

- R41/R42 voltage and power rating verified.
- H11AA1 input nets are in MAINS class. Current V4 netclass pattern includes U2 IN1/IN2, so confirm the actual V4 zero-cross nets for U3 are correctly covered.
- No copper pour violates isolation around U3.

### 5. 3.3 V Buck

Goals:

- Route PS2 switch loop tightly: PS2, L2, input cap, output cap, diode/internal switch loop as applicable.
- Keep noisy switch node short and away from ESP antenna, USB, RTD, and LCD FPC.
- Use 0.3 mm minimum for 3.3 V local, wider for trunks.

Checks:

- Good input cap return.
- Good output cap return.
- No switch node plane/pour that radiates unnecessarily.

### 6. ESP32 And USB

Goals:

- Place U1 at J8 before routing.
- Route USB D+/D- as differential pair from J8 to U1 to ESP2.
- Keep USB pair length short and avoid stubs.
- Keep ESP32 antenna keepout clear.
- Verify ESP2 exposed pad/thermal via region tied to GND.

Checks:

- CC resistors correct.
- Shield/chassis/earth strategy confirmed.
- EN/BOOT/reset traces sane.
- Decoupling close to ESP32 supply pins.

### 7. RTD And MAX31865

Goals:

- Keep RTD sense traces quiet and away from switching/mains/load traces.
- Route each MAX31865 to its jack with consistent topology.
- Keep reference resistor placement and routing tight.

Checks:

- Add or verify ESD/input protection for RTD jacks.
- Avoid ground-current contamination from triac/load or buck paths.
- SPI/control routing from ESP2 to MAX devices is clean and not routed near primary/load regions.

### 8. LCD/FPC And Backlight

Goals:

- Route FPC1 logic pins as SELV signals away from AC load and triac routes.
- Route LED+/LED- from PS3 to FPC1 with enough width and isolation from mains/load.
- Avoid using FPC area as a shortcut for load routing.

Checks:

- No copper or silkscreen creates assembly confusion at the FPC.
- Backlight current path has adequate width.
- FPC pins have manufacturable clearances.

### 9. Pours And Cleanup

Goals:

- Add GND pours only after primary/secondary keepouts are final.
- Stitch SELV GND where useful.
- Keep primary/load regions pour-free unless explicitly designed.
- Add test points, fiducials, labels, fab notes.

Checks:

- Refill zones and run DRC.
- Inspect board visually top and bottom.
- Confirm no thermal relief mistakes on high-current pads.
- Confirm no copper slivers or isolated islands.

## DRC/ERC And Manufacturing Sequence

After each subsystem:

1. Refill zones.
2. Run DRC.
3. Fix new issues immediately.
4. Save a checkpoint.

Before PCBWay package:

1. Run ERC from schematic.
2. Run DRC from PCB.
3. Generate Gerbers.
4. Generate drill files.
5. Generate IPC-D-356/netlist if needed.
6. Generate BOM.
7. Generate CPL/position files.
8. Generate assembly drawings.
9. Verify polarity, pin 1, and connector orientation.
10. Review fab notes for slots, mains clearance, copper weight, material, finish, and panel requirements.

## Stop Conditions

Stop and ask the user before continuing if:

- A fixed mechanical item must move.
- A routing path requires reducing mains-to-SELV spacing below the approved target.
- AC load current exceeds the assumed trace width/current capacity.
- ESP2 exposed pad cannot be confidently tied to GND.
- PS3 cannot be routed to FPC1 without crossing or approaching mains/load paths.
- USB differential routing cannot be kept sane with current placement.

## Routing Plan Update 2026-07-25

Confirmed assumptions:

- 120 VAC only.
- PCBWay 2-layer FR-4.
- 2 oz copper selected. Set F.Cu and B.Cu copper thickness to 0.070 mm in stackup when editing KiCad.
- Load currents: igniter 3 A, auger 0.75 A, fan 0.50 A, shared AC path up to 4.25 A.

Critical routing correction:

- Netclass width should not equal full current-carrying copper width if the net must escape dense triac pads.
- Use smaller default track widths that can exit pads.
- Expand into wide copper pours/shapes immediately after cramped fanout.

Updated AC-related defaults for routing:

| Netclass | Default width | Use |
| --- | ---: | --- |
| `AC_LOAD_SHARED` | 0.80 mm | Fused hot bus/neutral pad escape; expand to wider copper where open |
| `AC_LOAD_IGNITER` | 0.80 mm | T1 igniter pad escape; expand after fanout |
| `AC_LOAD_LOW` | 0.60 mm | T2/T3 auger/fan pad escape; expand after fanout |
| `MAINS_GATE` | 0.40 mm | MOC-to-triac gate/control side |
| `MAINS_SENSE` | 0.40 mm | U3/H11AA1 mains sense nets |

Target copper after fanout:

- Shared AC bus/neutral: 1.5-2.0 mm, preferably pours/shapes in open areas.
- Igniter branch: 1.2-1.5 mm after triac pad escape.
- Auger/fan branches: 0.8-1.0 mm after triac pad escape.
- Gate and zero-cross sense nets: 0.3-0.4 mm, with mains-domain awareness but not load-current width.

Custom rules correction:

- Do not use broad global 3.0 mm mains-to-non-mains rules during active layout. They create red clearance halos around all MOC/triac/power pads and make the board unworkable.
- Use targeted keepouts/rule areas later at actual isolation boundaries.
- Keep only the minimal PS1 manufacturer-footprint/edge exceptions while setting up placement and netclasses.
- If broad rules return later, add same-footprint manufacturer exceptions for MOC1-MOC3, U3, and T1-T3 at the bottom of the custom rules file so package-internal pad spacing is respected.

Tomorrow start from:

1. Clean custom rules.
2. Confirm 2 oz stackup.
3. Confirm adjusted AC netclass default widths.
4. Assign actual nets to netclasses.
5. Resume placement/refactor work once KiCad display is usable and not dominated by invalid clearance halos.
