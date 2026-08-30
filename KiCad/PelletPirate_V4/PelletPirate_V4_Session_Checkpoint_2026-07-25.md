# PelletPirate V4 Session Checkpoint 2026-07-25

Purpose: quick reload note for the next PelletPirate V4 session.
Project folder: C:\development\pelletpirate\kicad\pelletpirate_v4
Scope: constraints, netclasses, copper weight, and custom-rule cleanup.

## Confirmed Today

- AC input: 120 VAC only.
- PCB: PCBWay 2-layer FR-4.
- Copper: 2 oz selected. Set F.Cu and B.Cu stackup copper thickness to 0.070 mm.
- Load currents:
  - Igniter: 3 A max.
  - Auger motor: 0.75 A max.
  - Fan: 0.50 A max.
  - Shared AC path worst case: 4.25 A.

## Main Lesson

Do not use a broad 3.0 mm custom clearance rule around every mains-related netclass during placement/routing.

That created huge red rings/halos around MOC, triac, and power pads and made the board effectively unroutable. It also failed to respect manufacturer package geometry.

Correct method:

- Netclasses define routable defaults.
- Board/global minimums stay low enough for fine-pitch and package geometry.
- Current capacity comes from wider copper pours/shapes after pad escape, not from forcing huge default track widths at every pad.
- Mains/SELV safety comes from deliberate isolation corridors, rule areas, keepouts, and slots at the actual boundaries.

## Corrected AC Netclass Width Philosophy

For 2 oz copper:

| Netclass | Default track width | Reason |
| --- | ---: | --- |
| `AC_LOAD_SHARED` | 0.80 mm | Escape pads first, expand later |
| `AC_LOAD_IGNITER` | 0.80 mm | T1/igniter pad escape |
| `AC_LOAD_LOW` | 0.60 mm | T2/T3 auger/fan pad escape |
| `MAINS_GATE` | 0.40 mm | Gate nets do not carry load current |
| `MAINS_SENSE` | 0.40 mm | Zero-cross sense nets do not carry load current |

Target copper after escaping cramped pads:

- Shared fused hot/neutral: 1.5-2.0 mm pours or filled shapes where space opens.
- Igniter switched branch: 1.2-1.5 mm after T1 escape.
- Auger/fan switched branches: 0.8-1.0 mm after T2/T3 escape.
- MOC gate and H11AA1 sense traces: 0.3-0.4 mm.

## Current Custom Rules Recommendation

Use minimal custom rules while setting up placement and netclasses:

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

Delete the duplicate older rule if KiCad reports:

```text
Rules 'PS1 internal manufacturer footprint' and 'PS1-internal' share the same condition.
```

Keep only `PS1 internal manufacturer footprint`.

## Rules Not To Use Right Now

Do not use the previous broad rules named like:

- `MAINS_INPUT to non-mains`
- `AC_LOAD_SHARED to non-mains`
- `AC_LOAD_IGNITER to non-mains`
- `AC_LOAD_LOW to non-mains`
- `MAINS_GATE to non-mains`
- `MAINS_SENSE to non-mains`

They are too blunt for active placement/routing.

## If Broad Rules Are Reintroduced Later

Add same-footprint internal manufacturer exceptions at the bottom of the rules file for:

- PS1
- MOC1, MOC2, MOC3
- U3
- T1, T2, T3

Those exceptions prevent external safety rules from firing inside manufacturer package geometry.

## Actual V4 Net Mapping To Apply

| Netclass | Nets |
| --- | --- |
| `AC_LOAD_SHARED` | `/AC-LOAD`, `/AC-NEUTRAL`, `Net-(WirePad1-Pin_1)` |
| `AC_LOAD_IGNITER` | `Net-(T1-A1)` |
| `AC_LOAD_LOW` | `Net-(T2-A1)`, `Net-(T3-A1)` |
| `MAINS_GATE` | `Net-(T1-G)`, `Net-(T2-G)`, `Net-(T3-G)`, `Net-(MOC1-Pad6)`, `Net-(MOC2-Pad6)`, `Net-(MOC3-Pad6)` |
| `MAINS_SENSE` | `Net-(U3-IN1)`, `Net-(U3-IN2)` |
| `MAINS_INPUT` | `Net-(PS1-AC(L))`, `Net-(PS1-AC(N))`, `Net-(PS1-+V(CAP))`, `Net-(PS1--V(CAP))` |
| `EARTH` | `Earth` |
| `SELV_12V` | `+12V`, `Net-(PS1-+VO)`, `Net-(PS2-SW)`, `Net-(PS2-VBST)` |
| `SELV_3V3` | `+3.3V` |
| `LED_BACKLIGHT` | `/LED+`, `/LED-`, `Net-(PS3-SW)`, `Net-(PS3-COMP)` |
| `USB` | `/USB-C_D+`, `/USB-C_D-`, `/USB_D+`, `/USB_D-`, `VBUS`, `Net-(J8-CC1)`, `Net-(J8-CC2)` |
| `RTD_ANALOG` | `Net-(J1-Ring)`, `Net-(J1-Sleeve)`, `Net-(J1-Tip)`, same for `J2` through `J5`, plus `Net-(MAX1-BIAS)`, `Net-(MAX1-ISENSOR)`, same for `MAX2` through `MAX5` |
| `LCD_FPC` | `/LCD_CS`, `/LCD_DC`, `/LCD_RST` |
| `SELV_SIGNAL` | `/ALL_SCLK`, `/ALL_SDI`, `/ALL_SDO`, `/AUGER_IO`, `/FAN_IO`, `/IGNITER_IO`, `/LED_PWM`, `/ZC_DET`, `/Encoder_Btn`, `/Encoder_OutA`, `/Encoder_OutB`, `/MAX1_CS` through `/MAX5_CS`, `Net-(ESP2-*)`, `Net-(MOC1-Pad1)`, `Net-(MOC2-Pad1)`, `Net-(MOC3-Pad1)`, `Net-(J9-Pin_1)`, `Net-(LED1-Pad2)`, `Net-(PS2-EN)`, `Net-(PS2-VFB)` |
| `Default` | All `unconnected-(...)` nets |

## Start Here Tomorrow

1. Verify custom rules are cleaned to the minimal PS1 rules above.
2. Verify 2 oz copper stackup.
3. Verify AC netclass default widths are pad-escape widths, not full ampacity widths.
4. Assign all V4 nets to the new netclasses.
5. Then resume component placement/refactor work.

