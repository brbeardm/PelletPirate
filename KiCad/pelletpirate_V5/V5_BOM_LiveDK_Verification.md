# V5 BOM — Live DigiKey Verification Pass
Date: 2026-08-28 (searches run live this date unless noted 08-26/27). Scope: all 58 unique MPNs on the
current V5 BOM (post C9/C43/C39/C40/R20 fixes). Method: live DigiKey search/page per MPN; spec text from
DK listing compared to BOM Value/Package. Result classes: ✅ = in stock + spec confirmed; ⚠ = live stock
flag; ❓ = DK page verified to exist but stock indicator not retrievable by search (check at order).

## ✅ Confirmed in stock, spec matches BOM (47 lines)
| MPN | Refs | DK listing said | Evidence |
|---|---|---|---|
| CL05B104KO5NNNC | C21-C35 | 0.1µF ±10% 16V X7R 0402 | page fetch 08-26, 4.19M stock |
| CL05B102KB5NNNC | C42 | 1000pF ±10% 50V X7R 0402 | ships today |
| C0402C104K3RACTU | C17 C40 C41 | 0.1µF 25V X7R 0402 | ships today (08-27) |
| C0603C105K3RACTU | C39 | 1µF ±10% 25V X7R 0603 | in stock (08-28) |
| C0805C104K3RACTU | C9 | 0.1µF 25V X7R 0805 | in stock (08-28) |
| C0805C104K5RACTU | C11 C14 | 0.1µF 50V X7R 0805 | ships today |
| C0805C105K3RACTU | C37 | 1µF 25V X7R 0805 | ships today |
| C0805C105K5RACTU | C16 | 1µF 50V X7R 0805 | ships today |
| C0805C224K5RACTU | C36 | 0.22µF 50V X7R 0805 | ships today |
| C1206C106K3RACTU | C8 C10 C38 | 10µF 25V X7R 1206 | ~70,000 stock, $1.69 |
| GRM31CZ71C226ME15L | C18 C19 C20 | 22µF ±20% 16V X7R 1206 | ships today |
| B32921C3103M000 | C1 C2 C3 | 10nF X2 305VAC, 13×4×9mm LS10 | in stock (08-27) |
| R46KF310000M1M | C4 | 0.1µF X2 275VAC film | ships today (08-27) |
| VY2471K29Y5SS63V7 | C5 C6 | 470pF Y2 440VAC disc, 7.5mm LS | ships today |
| VY2102M29Y5US63V7 | C12 | 1000pF Y5U 440VAC disc | ships today |
| A750MS477M1EAAE015 | C13 | 470µF 25V Al-polymer radial | ships today |
| RPF0811221M035K | C15 | 220µF 35V Al-polymer radial | ships today |
| RC0402FR-07100RL | R47 | 100Ω 1% 0402 | ships today |
| RMCF0603FT5K10 | R37 R38 | 5.1k 1% 0603 AEC-Q200 | ships today |
| RMCF0603FT1K00 | R39 | 1k 1% 0603 AEC-Q200 | ships today |
| RC0603FR-07100KL | R16 | 100k 1% 0603 | ships today (08-27) |
| RC0805FR-07330RL | R7 R8 | 330Ω 1% 0805 | in stock (08-27) |
| RC0805FR-07100KL | R15 | 100k 1% 0805 | in stock (08-27) |
| RC0805FR-0710KL | R20 | 10k 1% 0805 | in stock (08-28) |
| RC1206FR-074R99L | R17 | 4.99Ω 1% 1/4W 1206 | ships today |
| MAX31865ATP+ | MAX1-5 | RTD conv, TQFN-20 5×5, ±0.5°C, −40..125 | 13,943 stock, $10.23 |
| BTA12-600BWRG | T1 T2 T3 | Snubberless triac 600V 12A TO-220 | 1,847 stock, $2.57 |
| MOC3053M | MOC3 | random-phase triac driver 6-DIP | ships today |
| SMBJ20A | D1 | TVS 32.4V clamp SMB (Littelfuse) | in stock |
| STPS0540Z | D2 | Schottky 40V 500mA SOD-123 | in stock |
| LTST-C191KGKT | LED1 | green 571nm 0603 | ships today |
| H11AA1M | U3 | AC-input opto 4170Vrms 6-DIP | ships today |
| USBLC6-2SC6 | U1 | USB ESD SOT-23-6 | ~223k reel + 70k cut tape at DK |
| TPS562201DDCR | PS2 | buck 2A SOT-23-6 | in stock, $0.40 |
| TPS61165DBVR | PS3 | LED boost SOT-23-6 | in stock |
| LQH3NPH2R2MMEL | L1 | 2.2µH 2.1A 1212 shielded | 4,436 stock |
| LQH43PN3R3M26L | L2 | 3.3µH 2.1A 1812 shielded | immediate |
| SRR4028-100Y | L3 | 10µH 1.19A shielded (boost L — sane for TPS61165) | ships today |
| MOV-14D561K | MOV1 | 560V 4.5kA 14mm disc | ships today |
| 0031.8201 | F1 | Schurter OGN 5×20 holder (DK id 669925) | listed in stock |
| PPTC041LFBN-RC | J7 | 4-pos 2.54mm header | ships today |
| KMR221G LFS | S1 S2 | C&K tactile SMD | ships today, $0.67 |
| FH26W-39S-0.3SHW(60) | FPC1 | 39-pos 0.3mm FPC, bottom contact | 10,625 stock, $2.21 |
| DF13-4P-1.25DSA(25) | J6 | 4-pos 1.25mm header | ships today |
| TB002-500-02BE | J9 | 2-pos 5.00mm screw terminal | in stock (08-27) |
| USB4145-03-0170-C | J8 | vertical USB-C, 1.70mm stakes | in stock, $1.03 (08-27) |
| SSRH7H-M05408 | LF1 | CM choke 40.8mH 0.5A (spec ✓) | listed; stock indicator not shown — treat as ❓ for stock |

## ⚠ Live stock flags (3)
| MPN | Refs | Finding | Action at order |
|---|---|---|---|
| RC0402FR-0710KL | R32-R36 R40 R43 R44 R45 (9 refs) | **DK OUT OF STOCK, backorder** | Substitution-allowed generic 0402 10k 1% — PCBWay subs freely or use own stock; equal-or-better spec, −40°C min |
| MOC3063M | MOC1 MOC2 | **onsemi version OOS at DK (backorder); Lite-On MOC3063M ships today** | DNS line — decide: hold for onsemi (other distributors/PCBWay channels) or explicitly authorize Lite-On (licensed second source) before order. Do NOT let it be silently subbed |
| SJ1-3523NG | J1-J5 | DK shows backorder (noted 08-27) | PCBWay self-sourced these successfully on V2 and V4; informational |

## ❓ DK page verified, stock not retrievable by search (check at order) (8)
| MPN | Refs | Notes |
|---|---|---|
| PBO-5F-12 | PS1 | **HIGH WATCH** — DNS-critical; DK id 28717413 exists; PBO family showing constraints (PBO-5C-12 OOS, no backorder). If PCBWay cannot source genuine, consign fresh units |
| 450BXG15MEFC10X20 | C7 | DNS (450V primary bulk); DK id 23330777 exists; verify stock at order |
| ERJ-P08J153V | R41 R42 | DNS (anti-surge); PCBWay sourced 07-2026; verify at order |
| RT1206BRD07400RL | R21-R25 | Rref precision — sub only with equal tolerance+tempco; PCBWay sourced 07-2026 |
| ERJ-UP6F1800V | R6 R9 | fan channel 180Ω; sub-allowed equal spec |
| ERJ-1TYJ390U | R1 R2 R3 | 39Ω 1W 2512; page live (id 365201), stock never surfaced in 3 attempts; sub-allowed |
| CRCW1206360RFKEA | R4 R5 R26 R27 R28 | 360Ω 1206; sub-allowed; PCBWay sourced 07-2026 |
| ESP32-S3-WROOM-1U-N16R8 | ESP1 | DK $6.76 (stock not shown); Mouser stocked, LCSC 14,529 — multi-source deep. DNS: exact variant only |

## Summary
- 58/58 MPNs verified to exist as real, currently-listed products. **Zero spec mismatches** found on any
  line where DK data was retrievable (values, packages, voltages, dielectrics, pitches all agree with BOM).
- 3 live stock flags, of which one (MOC3063M) needs a **decision** before reorder; one (PBO-5F-12) is the
  standing high-watch line; the 10k 0402 OOS is routine substitution territory.
- This pass supersedes decode heuristics with vendor-listed data for every retrievable line.
