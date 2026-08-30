# PelletPirate V4 Placement Audit 07/24/2026

Project: PelletPirate V4
Folder: C:\development\pelletpirate\kicad\pelletpirate_v4
Audit type: pre-routing placement, architecture, safety, and production-readiness review
Status: documentation-only; no KiCad design files modified

## Scope

Inspected V4 KiCad project files and local project libraries available in the V4 project folder. The goal was to capture the state of the placed but unrouted V4 PCB and identify the next required actions before routing and production hardening.

Key inspected files:

- PelletPirate_V4.kicad_sch
- PelletPirate_V4.kicad_pcb
- PelletPirate_V4.kicad_pro
- PelletPirate_V4.kicad_dru
- fp-lib-table
- Local V4 symbol and footprint libraries/directories

## Executive Summary

V4 is in a good phase for correction: all components are placed and grouped, but there are no tracks or vias yet. This means the board can still be hardened before layout decisions become expensive.

Do not route yet. The next engineering step is a placement and rules hardening pass. The main blockers are mains-to-SELV isolation policy, the FPC1 proximity to triac/load circuitry, long MOC-to-triac gate loops, USB ESD placement, PS3 distance from the LCD FPC, zero-cross primary/secondary separation, and the ESP32 exposed pad grounding verification.

## Architecture Summary

PelletPirate V4 appears to use this architecture:

- AC mains input enters through wire pads and passes through fuse/protection/EMI components.
- PS1 PBO-5F-12 module creates an isolated 12 V rail.
- PS1 secondary feeds filtering/protection and a 12 V distribution path.
- PS2 generates 3.3 V for ESP32-S3 logic and peripherals.
- PS3 appears to generate LCD LED/backlight drive routed toward FPC1.
- ESP2 ESP32-S3-WROOM-1U-N16R8 is the main controller.
- USB-C J8 provides USB connectivity; U1 is USB ESD protection.
- Five MAX31865 devices interface with five RTD jacks J1-J5.
- Three optotriac/triac channels switch AC loads through MOC1-MOC3 and T1-T3.
- U3 H11AA1M provides zero-cross detection with R41/R42 mains-side input resistors and R43 logic-side resistor.

## Current Board State

- Board size: 65 mm x 110 mm.
- Board outline: x=120..185, y=40..150.
- Footprints: 121.
- Routed segments: 0.
- Vias: 0.
- Locked GND zone exists near line 44812. Treat as placeholder/stale until final isolation rules are defined.
- V4 PCB still contains "PelletPirate v3.0 2026" text on B.SilkS.

## Positive Findings

- The board is clean and unrouted, which is the correct time to fix placement, DRC, and manufacturability issues.
- Fixed external connectors appear intentionally placed around enclosure/ribbon constraints.
- RTD jacks and MAX31865 devices are generally grouped with their related subsystem.
- PS2 buck converter and L2 are close enough for compact power-stage routing.
- PS1 front-end components are grouped compactly enough to create a deliberate primary/secondary boundary.

## P0 Findings

### P0-1: Mains-to-SELV rules are not production-ready

Finding:
Current V4 custom DRC has a 1.0 mm mains-to-low-voltage clearance rule. The MAINS netclass clearance is 0.5 mm and Default clearance is 0.195 mm. This is not adequate as a production safety strategy for a mixed AC mains and low-voltage controller.

Remediation:

1. Define target safety standard and voltage domain: 120 VAC only or 120/240 VAC.
2. Define explicit netclasses for primary mains, AC load, earth/chassis, SELV logic, USB, RTD, 12 V, 3.3 V, and LED boost.
3. Replace the current generic 1.0 mm rule with reviewed creepage/clearance values.
4. Add keepout zones or slots where required.
5. Run DRC before any routing and after every routing pass.

### P0-2: FPC1 is too close to triac/load region for casual routing

Finding:
FPC1 is fixed at the bottom edge for LCD ribbon access. T1 and WirePad3 are close to FPC1. This creates a high-risk routing situation where LCD/SELV traces and AC load/mains traces could be forced into the same area.

Remediation:

1. Confirm whether T1/T2/T3 and WirePad3/WirePad4/WirePad5 are mechanically fixed.
2. If possible, move triacs/load pads away from FPC1.
3. If they cannot move, add an explicit isolation slot/keepout/no-copper corridor between FPC1 and all AC load routing.
4. Prohibit AC load/neutral routing along the FPC edge.
5. Re-run DRC with final creepage/clearance constraints.

### P0-3: ESP2 exposed pad grounding must be verified before routing

Finding:
The V4 PCB footprint contains repeated pads named "41" in the ESP32-S3 exposed-pad/thermal-via region. The V3 audit found a critical symbol/footprint mapping risk where the exposed pad could be unnetted. V4 must be explicitly verified before routing.

Remediation:

1. Confirm the ESP32-S3 symbol pad 41 mapping.
2. Confirm PCB pad 41 is assigned to GND.
3. Confirm all exposed-pad thermal vias connect to GND.
4. If not correct, fix symbol/footprint mapping before routing ground or power.
5. Run ERC/DRC after the fix.

### P0-4: Zero-cross input resistor chain may be under-rated

Finding:
R41 and R42 are 15k each and feed the H11AA1M input. For mains use, resistor voltage rating, dissipation, surge capability, creepage, and failure mode must be validated. A pair of 1206 resistors may not be sufficient depending on RMS voltage, line tolerance, transients, and required margin.

Remediation:

1. Calculate steady-state current and power at high-line voltage.
2. Validate resistor working voltage and overload/surge rating.
3. Consider a larger series resistor stack with appropriate voltage spacing.
4. Keep R41/R42 on the primary side of the isolation boundary.
5. Keep R43 and logic pullup/output components on the SELV side.

### P0-5: Locked full-board GND zone must not be used as final layout

Finding:
A locked GND zone exists over both copper layers and spans the board region. It should not be trusted before primary/secondary keepouts and netclass constraints are finalized.

Remediation:

1. Unlock/remove/replace the zone before final routing.
2. Define primary, load, earth, and SELV no-copper boundaries.
3. Rebuild GND zones only in SELV-safe regions.
4. Refill zones and visually inspect both layers before final DRC.

## P1 Findings

### P1-1: USB ESD U1 is too far from J8

Finding:
U1 is about 21 mm from the USB-C connector J8. ESD protection should be placed as close as possible to the connector so surge energy is handled before entering the board.

Remediation:

1. Move U1 adjacent to J8.
2. Route USB D+/D- from J8 to U1 first, then from U1 to ESP2.
3. Avoid stubs and keep the pair short and symmetric.
4. Confirm USB shield/chassis/earth handling.

### P1-2: MOC-to-triac gate loops are long

Finding:
MOC1/MOC2/MOC3 are approximately 46 mm from T1/T2/T3. Long triac gate loops increase susceptibility to noise and false triggering.

Remediation:

1. Move MOC1 closer to T1, MOC2 closer to T2, and MOC3 closer to T3 if mechanically possible.
2. Preserve optocoupler isolation orientation.
3. Keep triac-side opto pins and gate nets in the mains/load class.
4. Keep gate loops short and away from ESP32/RTD/LCD signals.

### P1-3: PS3 is far from FPC1

Finding:
PS3 is about 90 mm from FPC1. If PS3 drives LCD backlight pins, LED+/LED- routing will be long and may be hard to protect from mains/load routes.

Remediation:

1. Move PS3 closer to FPC1 if this can be done without compromising mains isolation.
2. If PS3 remains where it is, reserve a protected SELV backlight corridor from PS3 to FPC1.
3. Keep the boost switch node tight around PS3/L3/D2.
4. Keep LED current traces adequately wide.

### P1-4: Zero-cross placement needs a visible isolation split

Finding:
U3, R41, R42, and R43 are located near the PS1/primary region. The H11AA1 input side and logic output side should be visually and electrically separated.

Remediation:

1. Place primary input resistors on the primary side.
2. Place output pullup/filter logic on the SELV side.
3. Add keepout/slot or drawing-layer boundary through the optocoupler isolation gap if needed.
4. Confirm V4 netclass patterns cover the actual U3 input nets; current project patterns include U2 IN1/IN2 names, which may be stale if U3 is the active zero-cross reference.

### P1-5: Add fiducials and test points before routing completion

Finding:
No fiducial/test point evidence was found during the V4 inspection.

Remediation:

1. Add global fiducials for assembly.
2. Add test points for GND, 12 V, 3.3 V, ESP EN/BOOT/UART or programming interface, zero-cross output, triac drive outputs, MAX31865 SPI, and LCD/backlight rails.
3. Confirm test points do not compromise creepage/clearance.

## P2 Findings

### P2-1: RTD front-end protection should be reviewed

Finding:
The RTD jacks are user-accessible edge connectors feeding sensitive MAX31865 front ends. External sensor wiring can bring ESD/noise into the board.

Remediation:

1. Add or verify ESD protection appropriate for RTD inputs.
2. Keep RTD traces away from mains/load and switching nodes.
3. Use consistent routing topology for all five MAX31865 channels.
4. Keep reference resistor routing tight and quiet.

### P2-2: PS1 secondary and 12 V distribution should be tightened during routing

Finding:
PS1 secondary components are spread along the right-side power area. This may be acceptable, but routing must keep the 12 V path low impedance and away from primary copper.

Remediation:

1. Route PS1 output to protection/filter components with wide traces.
2. Keep high-current 12 V paths compact.
3. Use a deliberate 12 V trunk to PS2 and loads.
4. Avoid routing 12 V through quiet sensor regions.

### P2-3: USB layout needs full high-speed treatment

Finding:
USB D+/D- should be treated as a differential pair with the ESD device at the connector, not as generic GPIO routing.

Remediation:

1. Confirm stackup and impedance target.
2. Move U1 next to J8.
3. Route D+/D- as a short pair with matched topology and no unnecessary vias/stubs.
4. Keep pair away from switching/mains/load regions.

### P2-4: PCBWay readiness requires custom footprint validation

Finding:
The project uses many custom footprints and local libraries. These need footprint, courtyard, drill, orientation, pad numbering, and assembly validation before fabrication/assembly.

Remediation:

1. Verify pad numbers against schematic symbols for all custom footprints.
2. Verify pin 1/orientation and polarity marks.
3. Verify hole sizes and slots.
4. Verify courtyards and assembly clearances.
5. Confirm BOM MPN availability and package matches.

## P3 Findings

### P3-1: Silkscreen still says V3

Finding:
The V4 PCB contains bottom silkscreen text reading "PelletPirate v3.0 2026".

Remediation:

1. Update silkscreen to V4 before production.
2. Confirm board revision, date, copyright, and polarity labels.

### P3-2: Drawing layers need cleanup before release

Finding:
There is at least one Dwgs.User rectangle near x=161.23..162.15, y=119.6..136.75. Its purpose should be documented or removed before manufacturing output.

Remediation:

1. Review drawing/user layers.
2. Keep meaningful fabrication/assembly guides.
3. Remove stale construction geometry before release.

## Recommended Next Step

Perform a placement-hardening pass before routing:

1. Confirm fixed mechanical parts.
2. Update DRC/netclass policy.
3. Resolve FPC1 vs triac/load conflict.
4. Move U1 close to J8.
5. Shorten MOC-to-triac gate loops.
6. Decide PS3 location/corridor.
7. Verify ESP2 pad 41 grounding.
8. Verify/rework H11AA1 input resistor chain.
9. Add fiducials/test points.
10. Only then begin routing system-by-system.

