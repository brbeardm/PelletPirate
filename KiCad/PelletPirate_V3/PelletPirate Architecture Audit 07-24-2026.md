# PelletPirate Architecture Audit 07/24/2026

## Scope

This audit was performed read-only against the active PelletPirate V3 KiCad project in this directory. The requested document title contains slashes, which are not valid in a single Windows filename, so this report is saved as `PelletPirate Architecture Audit 07-24-2026.md`.

Files inspected:

- `PelletPirate_V3.kicad_sch`
- `PelletPirate_V3.kicad_pcb`
- `PelletPirate_V3.kicad_pro`
- `PelletPirate_V3.kicad_prl`
- `PelletPirate_V3.kicad_dru`
- `fp-lib-table`
- Project-local custom symbols and footprints referenced by the V3 schematic/PCB.

No legacy V2 design files, old ERC/DRC reports, PDFs, CSV BOM exports, or files outside this project directory were used.

## Architecture Summary

PelletPirate V3 is a two-layer, 1.6 mm FR4 controller PCB, approximately 65 mm x 110 mm. The design combines AC mains switching, isolated low-voltage power, an ESP32-S3 controller, five RTD measurement channels, USB-C, LCD/backlight support, and three triac-switched AC load outputs.

### AC Mains Front End

AC load/hot enters at `WirePad1`, passes through `F1` (`T5AH 250V 5x20`) to `/AC-LOAD`. Neutral enters at `WirePad2` as `/AC-NEUTRAL`. Protective earth enters at `Earth1`.

The mains input includes:

- `MOV1` across `/AC-LOAD` and `/AC-NEUTRAL`.
- `C4` 0.1 uF 275 V X2 across `/AC-LOAD` and `/AC-NEUTRAL`.
- `LF1` common-mode choke between the fused input and the AC/DC module input.
- `C5` and `C6` 470 pF X1/Y2 capacitors from PS1 AC input lines to `Earth`.

### Isolated 12 V Supply

`PS1` is a PBO-5F-12 open-frame AC/DC converter. It takes `Net-(PS1-AC(L))` and `Net-(PS1-AC(N))` from the filtered mains input and produces `Net-(PS1-+VO)` and `GND`.

The PS1 primary-side bulk capacitor pins are used:

- `C7` 15 uF 450 V across `Net-(PS1-+V(CAP))` and `Net-(PS1--V(CAP))`.
- `C12` 1000 pF 440 V from `Net-(PS1--V(CAP))` to secondary `GND`.

The 12 V output path is:

- `PS1` pad 6 to `Net-(PS1-+VO)`.
- `C13` 470 uF from `Net-(PS1-+VO)` to `GND`.
- `L1` 2.2 uH from `Net-(PS1-+VO)` to `+12V`.
- `D1` SMBJ20A and `C15` 220 uF on `+12V`.

### 3.3 V Supply

`PS2` is a TPS562201-style buck regulator from `+12V` to `+3.3V`.

Key parts:

- `R15` pulls `PS2-EN` up to `+12V`.
- `L2` connects `PS2-SW` to `+3.3V`.
- `R19` 33.2 k and `R20` 10 k set feedback.
- `C18`, `C19`, and `C20` provide 3.3 V bulk output capacitance.

### LCD Backlight Boost

`PS3` is a TPS61165-style LED boost driver:

- Input: `+12V`.
- Switch node: `Net-(PS3-SW)`.
- Boost inductor: `L3`.
- Schottky diode: `D2` to `/LED+`.
- Current sense: `R17` 4.99 ohm from `/LED-` to `GND`.
- Control: `/LED_PWM`.
- Output to LCD connector: FPC1 pins 1 `/LED-` and 2 `/LED+`.

### ESP32-S3 Control

`ESP2` is an ESP32-S3-WROOM-1U-N16R8 module. It is powered from `+3.3V`.

Main assigned functions:

- SPI bus shared by MAX31865 channels and LCD: `/ALL_SCLK`, `/ALL_SDI`, `/ALL_SDO`.
- MAX chip selects: `/MAX1_CS` through `/MAX5_CS`.
- LCD control: `/LCD_CS`, `/LCD_DC`, `/LCD_RST`.
- AC outputs: `/IGNITER_IO`, `/AUGER_IO`, `/FAN_IO`.
- Zero-cross or fan pulse input: `/ZC_DET`.
- USB native lines: `/USB_D+`, `/USB_D-`.
- Encoder inputs: `/Encoder_OutA`, `/Encoder_OutB`, `/Encoder_Btn`.
- Boot/reset: `S1` reset on `EN`; `S2` boot on `IO0`.

### RTD Measurement

There are five MAX31865 RTD front ends (`MAX1` through `MAX5`), each wired to a 3.5 mm jack (`J1` through `J5`). They share SPI clock/data and each has a dedicated chip-select line. Each MAX31865 has local 0.1 uF decoupling and a 400 ohm bias resistor.

### AC Load Switching

There are three line-side switched AC outputs:

- Igniter: `MOC1` plus `T1`, output `WirePad3`.
- Auger: `MOC2` plus `T2`, output `WirePad4`.
- Fan: `MOC3` plus `T3`, output `WirePad5`.

`MOC1` and `MOC2` are MOC3063M zero-cross optotriacs. `MOC3` is marked MOC3053M, which is a non-zero-cross optotriac and may be intentional for fan phase control. All three BTA12 triacs switch `/AC-LOAD` to the respective load output line. The design does not provide neutral output pads for the loads; the harness must provide neutral elsewhere.

### Zero-Cross Detector

`U3` H11AA1M senses line/neutral through:

- `R42` 15 k from `/AC-LOAD` to `Net-(U3-IN1)`.
- `R41` 15 k from `/AC-NEUTRAL` to `Net-(U3-IN2)`.
- H11AA1 output uses `R43` 10 k pullup to `+3.3V` and `C42` 1000 pF to `GND` on `/ZC_DET`.

## Unusual Items

- Board setup has global minimum clearance set to `0.0 mm`; safety intent depends on netclasses and custom rules.
- The custom mains-to-low-voltage clearance rule is only 1.0 mm.
- `Net-(U3-IN1)` and `Net-(U3-IN2)` are primary-side mains-derived nets but are not assigned to the `MAINS` netclass.
- The ESP32 footprint has multiple pad `41` exposed-pad/thermal-via features with no net assigned on the PCB.
- The ESP32 symbol uses hidden ground pins numbered `41_1` through `41_9`, while the footprint uses repeated pad number `41`.
- `C12` crosses from PS1 primary-side `Net-(PS1--V(CAP))` to secondary `GND`.
- The PS1 footprint embeds internal `Edge.Cuts` slot geometry.
- There are no detected fiducials or test points in the active V3 schematic/PCB text.
- The `.kicad_prl` file hides `Net-(PS1--V(CAP))`, which is a high-voltage primary-side net. This is only a UI state, but it is easy to miss during manual review.

## P0: Production Blockers

### P0-1: Mains-to-SELV Clearance and Creepage Rules Are Not Production-Ready

Finding:

The design uses AC mains and isolated low-voltage circuitry on the same PCB, but the custom DRC only requires 1.0 mm clearance from `MAINS` to non-`MAINS`. Board global minimum clearance is `0.0 mm`. Creepage is enabled as an error severity, but no explicit production creepage constraint was found in the custom rule file. A locked GND pour exists on both copper layers with 0.5 mm pad clearance.

Why this matters:

1.0 mm is not a production safety clearance for mains-to-user-accessible SELV in a product environment. The board has mains, PE, isolated 12 V/3.3 V, USB, LCD, and external RTD connectors, so the isolation boundary must be defined by an applicable safety standard and enforced in CAD.

Remediation steps:

1. Define the required safety standard and environment assumptions before release: nominal mains voltage, overvoltage category, pollution degree, material group, altitude, coating, and whether low-voltage connectors are user-accessible.
2. Replace the single `MAINS` class with explicit classes such as `PRIMARY_L`, `PRIMARY_N`, `PRIMARY_DC`, `TRIAC_LOAD`, `PE`, `SELV`, and `USER_IO`.
3. Add explicit clearance and creepage rules for primary-to-SELV and primary-to-user-accessible connectors. Use conservative production targets such as 3 mm minimum for basic primary spacing and 6 mm or more for reinforced primary-to-SELV spacing unless a qualified safety standard calculation says otherwise.
4. Make custom rules symmetric so they apply whether a mains item is `A` or `B`.
5. Add physical copper keepouts and zone keepouts along the isolation boundary. Do not rely only on netclass clearance.
6. Keep the GND pour away from primary-side nets with an explicit keepout, not just clearance.
7. Regenerate DRC after these rules are added and review every primary-to-SELV violation manually.

### P0-2: H11AA1 Primary Input Nets Are Not in the MAINS Netclass

Finding:

`R42` connects `/AC-LOAD` to `Net-(U3-IN1)`, and `R41` connects `/AC-NEUTRAL` to `Net-(U3-IN2)`. These two H11AA1 input nets are mains-derived primary-side nets, but the project netclass patterns do not assign them to `MAINS`.

Why this matters:

The DRC rules will not reliably enforce primary-to-low-voltage spacing around `U3`, `R41`, or `R42`. These nets sit on the high-voltage side of the zero-cross detector and must be treated as primary.

Remediation steps:

1. Add stable schematic net labels for the H11AA1 input side, for example `/ZC_PRIMARY_L` and `/ZC_PRIMARY_N`.
2. Assign both nets to a primary/mains netclass.
3. Add primary-side keepouts around `U3` input pins and the resistor chain.
4. Re-run DRC and inspect H11AA1 pad-to-pad, pad-to-copper, and pad-to-zone spacing.
5. Consider removing copper pads for unused H11AA1 NC pins if they reduce creepage.

### P0-3: H11AA1 Input Resistors Appear Underrated for Production Mains Use

Finding:

The zero-cross detector uses `R41` and `R42`, both 15 k 1206 resistors, across line/neutral into the H11AA1 input. At 120 VAC, the pair dissipates roughly 0.48 W total, or about 0.24 W per resistor. At 240 VAC, the total would be roughly 1.9 W. The AC/DC module is universal input, so the board-level mains rating must be explicit.

Why this matters:

The resistor voltage rating, pulse rating, continuous power, thermal rise, and creepage are safety-critical. A borderline resistor chain can drift, crack, char the PCB, or fail unsafe.

Remediation steps:

1. Decide whether the product is 120 VAC only or universal input.
2. Recalculate H11AA1 LED current at minimum and maximum line voltage.
3. Replace the two-resistor chain with a series stack of high-voltage resistors with adequate working voltage, pulse rating, creepage, and power margin.
4. Derate for enclosure temperature near triacs and the AC/DC module.
5. Place the resistor chain fully on the primary side with clear isolation spacing from SELV copper.
6. Add BOM fields that lock the exact high-voltage resistor series and rating.

### P0-4: ESP32-S3 Exposed Pad / Ground Pad Is Not Netted on the PCB

Finding:

The ESP32-S3 footprint contains multiple pad `41` thermal/exposed-pad features, including thermal vias and a central SMD pad. In the PCB pad map, those pad `41` features have no net assigned. The symbol appears to use hidden ground pins numbered `41_1` through `41_9`, while the footprint uses repeated pad number `41`.

Why this matters:

The ESP32 module ground paddle and via field are part of grounding, RF return, thermal performance, and assembly reliability. Leaving them unnetted can cause poor RF behavior, poor EMC performance, thermal issues, and DRC blind spots.

Remediation steps:

1. Align symbol and footprint pad numbering. Either make the symbol use one hidden `41` GND pin, or update the footprint pads to match the symbol numbers exactly.
2. Assign all exposed/thermal pad copper and vias to `GND`.
3. Verify that duplicate pad numbers are intentional and accepted by KiCad for this footprint.
4. Re-run schematic-to-PCB update and DRC.
5. Inspect the ESP2 pad map after update to confirm every pad `41` feature is tied to `GND`.

### P0-5: C12 Crosses the Isolation Barrier and Must Be Treated as a Certified Safety Part

Finding:

`C12` connects `Net-(PS1--V(CAP))`, a PS1 primary-side high-voltage DC node, to secondary `GND`. The BOM fields identify a Vishay VY2-style 1000 pF 440 VAC ceramic part, but the schematic value text does not explicitly mark it as X/Y safety-rated, and the CAD rules only provide 1.0 mm mains-to-low-voltage clearance.

Why this matters:

Any capacitor from primary to secondary defeats pure galvanic separation unless it is a certified safety capacitor used within its leakage, impulse, and creepage ratings. It also affects touch current, EMI, and certification.

Remediation steps:

1. Confirm whether PS1 documentation permits an external primary-to-secondary capacitor and where it should connect.
2. If the capacitor is required, use an explicitly certified Y1 or Y2 part as required by the product safety target.
3. Update value, description, and BOM fields to state the safety class and certification.
4. Ensure the footprint and copper routing around C12 meet the required creepage/clearance, not just the part pin pitch.
5. Add DRC rules specifically for primary-to-secondary Y capacitor placement.
6. If not required, remove C12 and validate EMI another way.

### P0-6: Mains Load Current, Fuse Rating, Trace Width, and Thermal Limits Are Not Proven

Finding:

The board uses a T5A fuse and BTA12 triacs, but routed mains load traces include 1.2 mm to 1.5 mm segments and the design uses 1 oz copper in the stackup. Some mains branch segments are 0.5 mm. The design also uses solder-wire pads for mains/load connections.

Why this matters:

The fuse rating, trace ampacity, triac dissipation, connector rating, and enclosure temperature must agree. A 5 A fuse does not automatically make every downstream copper path or pad safe for 5 A continuous load.

Remediation steps:

1. Define the maximum current for igniter, auger, fan, and total line feed.
2. Calculate trace temperature rise for 1 oz copper at maximum load and worst-case ambient.
3. Increase high-current load traces/pours or specify 2 oz copper if needed.
4. Separate low-current mains control branches from high-current load routes with different netclasses.
5. Verify triac heatsinking and board temperature at maximum load.
6. Replace solder-wire pads with rated connectors or document a controlled harness/strain-relief process.

## P1: High Priority Before Prototype Release

### P1-1: Custom DRC Rule for MOC NC Pads Appears Malformed

Finding:

The custom rule `mains-to-moc-nc` uses `B.NetName == 'unconnected-(MOC*'`, which appears to be an exact string comparison rather than a wildcard match. It likely does not cover actual nets such as `unconnected-(MOC1-NC-Pad3)`.

Remediation steps:

1. Replace the exact comparison with a valid KiCad pattern or regex expression.
2. Make the condition symmetric.
3. Better: remove copper pads for NC pins 3 and 5 on optocouplers where allowed, or create a footprint variant with NC holes/pads omitted to improve creepage.
4. Re-run DRC and confirm NC pads are not reducing isolation spacing.

### P1-2: PS1 Footprint Uses Edge.Cuts Slots Inside the Footprint

Finding:

The custom `CONV_PBO-5F-12_AUDIT` footprint includes `Edge.Cuts` slot geometry. This is likely intended to increase creepage near the AC/DC module, but embedded slots must be fabricated correctly.

Remediation steps:

1. Confirm the slots appear in Gerbers and drill/routing outputs exactly as intended.
2. Add fab notes calling out non-plated routed slots, slot width, and tolerance.
3. Confirm PCBWay minimum routed slot capability for the geometry.
4. Ensure copper, mask, and courtyard clearance around each slot meet the safety target.
5. Keep a reviewed PS1 footprint under a stable library name; remove `AUDIT` naming when released.

### P1-3: No Dedicated Isolation Boundary Keepout Was Found

Finding:

Only one GND zone was found, locked on both copper layers. No explicit keepout objects were detected for the primary-to-secondary boundary.

Remediation steps:

1. Draw a visible primary/secondary isolation boundary on `Dwgs.User` or `Cmts.User`.
2. Add copper keepouts on both layers along the boundary.
3. Add zone keepout rules so the GND pour cannot refill into primary spacing.
4. Add silkscreen or fab markings for primary and SELV zones where useful.

### P1-4: Earth Handling Needs a Product-Level Decision

Finding:

`Earth` enters at `Earth1` and connects to primary-side Y capacitors `C5` and `C6`. No direct PE-to-chassis, PE-to-load, or PE continuity path was found on the PCB beyond the wire pad and capacitors.

Remediation steps:

1. Define whether the final appliance is Class I or Class II.
2. If Class I, define PE continuity through the harness, enclosure, motor/igniter bodies, and any metal panel.
3. Use rated PE terminals and creepage/clearance appropriate for protective earth.
4. Do not rely on PCB signal naming alone for PE safety.

### P1-5: Triac Output EMI and Inductive Load Robustness Need Hardening

Finding:

The triac channels use optotriac drivers and gate resistors, but no per-output MOVs or RC snubbers were found across the individual switched loads. `MOC3` is non-zero-cross, likely for fan control, which can increase EMI and false-trigger risk.

Remediation steps:

1. Confirm load types: igniter resistive, auger motor, fan motor.
2. Add RC snubbers or MOVs per output where testing shows transients or false triggering.
3. Validate triac commutation with the actual fan and auger motors.
4. Perform conducted/radiated EMI pre-scan before production.
5. Keep gate loops short and away from ESP/RTD analog routing.

### P1-6: ESP32-S3 Boot Strap Network Should Be Verified

Finding:

The design uses `R31` 10 k pullup and `S2` boot switch on IO0, `R35` 10 k pulldown on IO46, `R36` 10 k pulldown on IO45, `C39` from EN to GND, `C40` from IO0 to `+3.3V`, and `C41` from IO14 to GND. These are strap-sensitive pins on the ESP32-S3 family.

Remediation steps:

1. Verify every ESP32-S3 strapping pin against the selected N16R8 module datasheet.
2. Confirm IO45 and IO46 forced-low states match the desired boot mode and flash voltage behavior.
3. Reconsider `C40` from IO0 to `+3.3V`; if it is intended as debounce or boot timing, document the timing effect. If not intentional, remove it or move it to the intended node.
4. Confirm USB download mode works from a cold power-up and after reset.
5. Add test pads for EN, IO0, TXD0, RXD0, 3.3 V, GND, and USB D+/D- if field recovery matters.

### P1-7: No Test Points or Fiducials Were Detected

Finding:

No `TestPoint` or `Fiducial` footprints were detected in the active schematic/PCB text.

Remediation steps:

1. Add global fiducials for PCBWay SMT assembly.
2. Add test points for `+12V`, `+3.3V`, `GND`, `/ZC_DET`, SPI bus, chip selects, triac drive IO, and boot/reset.
3. Add a programming/recovery header or pads.
4. Add isolated-safe test strategy for mains-side signals.

### P1-8: Solder-Wire Pads Are Risky for Production Mains Connections

Finding:

Mains input, earth, and load outputs use generic solder-wire pads rather than rated terminals.

Remediation steps:

1. Replace mains and load pads with rated terminal blocks, quick-connect tabs, or a controlled wire-to-board connector.
2. Add strain relief outside the solder joint.
3. Confirm creepage/clearance around connector bodies, not only pad centers.
4. Add clear silkscreen labels for line, neutral, earth, igniter, auger, and fan.

### P1-9: BOM Safety Fields Need Certification Cleanup

Finding:

The BOM contains exact MF/MP fields, but several safety-critical entries depend on value text and links. Some MP fields include trailing spaces. PS1 footprint metadata also showed inconsistent manufacturer-style fields in the local footprint, even though the schematic BOM field says Bel Fuse.

Remediation steps:

1. Lock exact manufacturer part numbers for all safety parts: fuse holder, fuse, MOV, X2 cap, Y caps, PS1, primary bulk capacitor, triacs, optocouplers, and zero-cross resistors.
2. Add voltage, safety class, flame rating, and agency certification notes to the schematic fields.
3. Remove trailing spaces from MPN fields.
4. Make schematic symbol fields the single BOM source of truth.
5. Generate a fresh BOM and verify PCBWay/JLC-style part mapping manually.

## P2: Medium Priority Hardening

### P2-1: USB-C Native ESP32 Connection Needs Layout Review

Finding:

USB-C CC resistors are present (`R37`, `R38` 5.1 k to GND), and ESD protection is present (`U1`). USB D+/D- routing uses 0.3 mm and 0.5 mm segments and no differential-pair tuning settings were evident.

Remediation steps:

1. Verify D+/D- impedance, spacing, and length matching for full-speed USB.
2. Place ESD protection as close as practical to the USB-C connector.
3. Decide whether USB VBUS should power the board, only detect cable presence, or remain ESD-only.
4. If USB cannot power the board, document that external 12 V/AC power is required for USB programming.

### P2-2: RTD Inputs Need External Connector Protection Review

Finding:

Five MAX31865 channels connect to 3.5 mm jacks. No dedicated ESD, surge, series protection, or input filtering beyond the MAX31865 application network was identified at the jacks.

Remediation steps:

1. Add ESD protection suitable for sensor leads that can be touched or routed externally.
2. Add small series resistors or RC filters if compatible with RTD accuracy.
3. Define supported RTD wiring mode and connector pinout.
4. Add open/short diagnostics to firmware and production test.
5. Verify 3.5 mm jack mechanical retention and contamination behavior in a smoker environment.

### P2-3: Assembly Is Mixed Technology and Double-Sided

Finding:

The board has SMD on both sides plus through-hole parts: fuse holder, AC/DC module, triacs, wire pads, capacitors, jacks, FPC, USB-C, and connectors.

Remediation steps:

1. Decide which parts PCBWay assembles and which parts are hand-soldered.
2. Provide separate SMT and through-hole assembly instructions.
3. Add fiducials before automated assembly.
4. Generate centroid/position files and verify bottom-side rotations.
5. Review tall components for enclosure clearance.

### P2-4: Many 0402 Passives Reduce Assembly Margin

Finding:

The design uses many 0402 capacitors/resistors, including near the ESP32, MAX31865s, and control signals.

Remediation steps:

1. Keep 0402 only where density requires it.
2. Move pullups, strap resistors, and non-critical decouplers to 0603 where possible.
3. Use 0805/1206 for parts exposed to connector abuse, voltage stress, or hand rework.

### P2-5: Triac Thermal and Mechanical Support Need Verification

Finding:

The BTA12 triacs are TO-220 vertical footprints. No heatsink or mechanical retention strategy was evident from the KiCad files.

Remediation steps:

1. Calculate dissipation for each load current using triac on-state voltage.
2. Verify junction temperature at worst-case enclosure temperature.
3. Add heatsinks or move to board-mounted tabs if needed.
4. Confirm insulated tab rating and creepage to nearby conductors and enclosure.
5. Add mechanical support if appliance vibration is expected.

### P2-6: High-Voltage Net Names Are Fragile

Finding:

Several safety rules depend on auto-generated net names such as `Net-(PS1--V(CAP))`, `Net-(T1-G)`, and `Net-(MOC1-Pad6)`.

Remediation steps:

1. Add explicit schematic labels for every primary/high-voltage net.
2. Assign netclasses by stable labels, not generated names.
3. Document the intended primary, PE, and SELV domains on the schematic.

### P2-7: PCB Documentation for Fabrication Is Thin

Finding:

No dimensions were found in the PCB file. No visible fab notes for mains spacing, slots, copper weight, UL rating, soldermask, or assembly process were identified.

Remediation steps:

1. Add board dimensions.
2. Add fab notes for material, copper weight, finish, slot requirements, controlled impedance if needed, and safety spacing.
3. Add assembly notes for no-clean/cleaning expectations around high impedance RTD inputs.
4. Add label text for board revision and safety warnings.

## P3: Cleanup and Maintainability

### P3-1: Remove Ambiguous Audit/Prototype Library Names Before Release

Finding:

The active PS1 footprint is named `CONV_PBO-5F-12_AUDIT`, and there are multiple PBO-5F-12 symbol/footprint attempts in local libraries.

Remediation steps:

1. Promote the reviewed footprint to a release name.
2. Archive or remove unused local library variants.
3. Keep one canonical symbol and footprint per production part.

### P3-2: Add 3D Models or Explicitly Accept Missing Models

Finding:

Many custom footprints have no 3D model. This does not block electrical function, but it weakens enclosure and mechanical clearance review.

Remediation steps:

1. Add 3D models for connectors, triacs, fuse holder, PS1, jacks, and tall capacitors.
2. Use 3D review to check enclosure, antenna cable, heat sinks, and service clearance.

### P3-3: Clean Up UI State That Hides High-Voltage Nets

Finding:

The `.kicad_prl` hides `Net-(PS1--V(CAP))`. This is not an electrical issue, but hidden high-voltage nets can be missed in review.

Remediation steps:

1. Unhide all primary/high-voltage nets during safety review.
2. Save review layer presets that show mains, SELV, PE, zones, and Edge.Cuts clearly.

### P3-4: Preserve Audit Outputs in the Project

Finding:

Long hardware reviews need durable project-local artifacts.

Remediation steps:

1. Keep this report under version control with the KiCad project.
2. Add a dated DRC/ERC summary after every major design change.
3. Add a release checklist for schematic, PCB, BOM, fabrication, assembly, and safety review.

## Recommended Release Sequence

1. Freeze safety assumptions: mains voltage, class, user-accessible connectors, enclosure, pollution degree, and certification target.
2. Fix netclasses and custom DRC first, especially H11AA1 primary nets and primary-to-SELV spacing.
3. Fix ESP2 pad 41 symbol/footprint mismatch.
4. Rework zero-cross input resistor chain for voltage, power, and creepage.
5. Verify or remove C12 as a certified Y capacitor across the isolation boundary.
6. Add isolation keepouts, creepage slots where required, and safe GND pour boundaries.
7. Recalculate fuse, trace, connector, and triac thermal limits.
8. Add fiducials, test points, programming access, fab notes, and board dimensions.
9. Clean BOM safety fields and generate a fresh production BOM.
10. Generate fresh ERC/DRC/Gerbers and review every remaining warning before PCBWay order.

## Bottom Line

The high-level architecture is coherent for a pellet grill controller: isolated 12 V supply, local 3.3 V regulation, ESP32-S3 controller, five RTD channels, LCD/backlight support, USB-C, zero-cross sensing, and three optically isolated triac outputs.

The design is not production-ready until the P0 issues are fixed. The biggest blockers are safety-rule weakness, missing MAINS classification for H11AA1 input nets, resistor stress in the zero-cross detector, the unnetted ESP32 exposed pad/thermal pad, and the isolation-barrier treatment of C12.
