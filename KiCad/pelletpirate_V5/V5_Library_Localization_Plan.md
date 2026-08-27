# V5 Library Localization Plan
Date: 2026-08-27 · Status: PROPOSED (nothing executed)
Goal: V5 resolves every symbol and footprint from its own directory tree. No design files (.kicad_sch / .kicad_pcb) are touched — the schematic's embedded cache guarantees the design cannot break during this operation.

## Safety model
- Symbol instances reference `NICKNAME:NAME`. A **project** sym-lib-table using the **same nicknames** shadows the global table → every reference resolves to the V5 copy with zero schematic edits.
- Global sym-lib-table is LEFT ALONE (V2/V3/V4 still need those entries when opened).
- fp-lib-table nicknames are never renamed (the board references them); only dead entries removed and one missing entry added.
- KiCad must be CLOSED during execution (tables are read at startup).
- Everything is file copies + moves → fully reversible; archive_to_delete used, nothing deleted.

## Phase 1 — Copy 17 symbol files into V5
| # | Nickname (unchanged) | Source | V5 destination |
|---|---|---|---|
| 1 | 3.5MM_Jacks | rev2026-00/3.5mm_Jacks/SJ1-3523NG.kicad_sym | 3.5mm_Jacks/ (exists) |
| 2 | BTA12-600BWRG | PelletPirate_V3/BTA12-600BWRG.kicad_sym | BTA12-600BWRG/ (new dir) |
| 3 | ESP32-S3-WROOM-1U-N16R8 | V3/Expressif_S3_N16R8_chip_N1U/ESP32-S3-WROOM-1U-N16R8.kicad_sym | Expressif_S3_N16R8_chip_N1U/ (exists) |
| 4 | FH26W-39S-0.3SHW_60_ | V2/FPC_Connector_HiRose/FH26W-39S-0.3SHW_60_.kicad_sym | FPC_Connector_HiRose/ (exists) |
| 5 | KMR221GLFS | V3/KMR221GLFS_SMD_boot_reset_switch/KMR221GLFS.kicad_sym | KMR221GLFS_SMD_boot_reset_switch/ (exists) |
| 6 | LQH3NPH2R2MMEL | V2/2.2uH_Murata_1212_footprint/LQH3NPH2R2MMEL.kicad_sym | 2.2uH_Murata_1212_footprint/ (exists) |
| 7 | LQH43PN3R3M26L | V2/3.3uH_Inductor_Murata/LQH43PN3R3M26L.kicad_sym | 3.3uH_Inductor_Murata/ (exists) |
| 8 | LTST-C191KGKT_flyman | V3/LED_light/LTST-C191KGKT_flyman.kicad_sym | LED_light/ (exists) |
| 9 | MOC3063M-PP2 | rev2026-00/MOC3063M-PP2/MOC3063M-PP2.kicad_sym | MOC3063M-PP2/ (exists) |
| 10 | PBO-5F-12_pp2 | rev2026-00/PBO-5F-12_PelletPirate2/PBO-5F-12_pp2.kicad_sym | PBO-5F-12_PelletPirate2/ (new dir) |
| 11 | SMBJ20A_PelletPirate | rev2026-00/SMBJ20A_PelletPirate/SMBJ20A_PelletPirate.kicad_sym | SMBJ20A_PelletPirate/ (exists) |
| 12 | SRR4028 | rev2026-00/Inductors/SRR4028.kicad_sym | Inductors/ (exists) |
| 13 | STPS0540Z | rev2026-00/STPS0540Z_Schotty Diode LED/STPS0540Z.kicad_sym | STPS0540Z_Schotty Diode LED/ (exists) |
| 14 | TPS562201DDCR | rev2026-00/Buck_3.3V/TPS562201DDCR.kicad_sym | Buck_3.3V/ (exists) |
| 15 | TPS61165DBVR | V2/LED_Driver/TPS61165DBVR.kicad_sym | LED_Driver/ (exists) |
| 16 | USBLC6-2SC6 | V3/USBLC6-2SC6/USBLC6-2SC6.kicad_sym | USBC6-2SC6/ (exists; folder name keeps its historical typo — nickname unchanged) |
| 17 | 0031.8201 fuseblock | C:/Users/dbria/Downloads/0031.8201/0031.8201.kicad_sym | Schurter_Fuse_Holder/ (exists) — rescued from Downloads |

## Phase 2 — Write project sym-lib-table (NEW file)
Entries for all 20 project nicknames used by the schematic, same nicknames, `${KIPRJMOD}` URIs:
the 17 above + the 3 already local (H11AA1M_Opto_FanPulse, TB002-500-02BE, USB4145-03-0070-C_REVA2)
+ GCT_USB4145-03-0170-C (staged for the J8 part-number swap).
Note: H11AA1M's global entry uses ${KIPRJMOD} in the GLOBAL table (resolves against whatever project is open — works in V5 by luck). The project-table entry makes it deterministic.

## Phase 3 — fp-lib-table hygiene (EDIT, no renames)
ADD (missing, board already uses it): `SameSky_Screw_Terminal -> ${KIPRJMOD}/SameSky_Screw_Terminal`
REMOVE dead entries (target dirs do not exist): 3.3uH_Inductor · HiRose · LCD-Molex · NCP3065 LED Boost Driver · PBO-5F-12_PelletPirate · PBO-5F-12_PelletPirate2 · RC0805FR-Sense-Resistor-LED
REMOVE entries for dirs archived in Phase 4: Capacitors · ESP32-DevKitC-32E_PelletPirate · ESP32-Footprint-PelletPirate-02x19x2=39 · PBO-5F-12_PP_Try2 · Bourns_Encoder_New
KEEP as-is (typo nicknames referenced by the board — renaming would break it): Kermit_Choke_SSRH7H_MO5408, USBC6-2SC6.

## Phase 4 — Archive sweep → archive_to_delete/
| Item | Reason |
|---|---|
| Capacitors/ | not used by board |
| ESP32-DevKitC-32E_PelletPirate/ | DevKit era, V5 uses S3 module |
| ESP32-Footprint-PelletPirate-02x19x2=39.pretty/ | DevKit socket footprint |
| PBO-5F-12_PP_Try2/ | superseded PBO attempt |
| Bourns_Encoder_New/ | old PEC16 encoder (KEEPING Bourns_Encoder/ = PEC11R, the V3-selected part) |
| ESP32-S3-WROOM-1U-N16R8/ (top-level dir, 2 lib files) | redundant once symbol lives in Expressif_S3_N16R8_chip_N1U/ — content diff before moving |
| New folder/ | empty |
| USB_try2: USB4105-GF-A* (sym/mod/step/bak) | V3-era USB4105 attempt, not this design |
| USB_try2/USB4145-03-0070-C_REVA2.bak | editor backup |
| PelletPirate_V5.kicad_pro.pre_netclass_backup | superseded by git history |
NOT touched (no lib files; user decides later): DataSheets/, MAX_31865ATP+/, HiRose_Encoder_Jack/, .claude/

## Phase 5 — Verification (after user reopens KiCad)
1. Script check: every schematic nickname resolves to a ${KIPRJMOD} path that exists; all fp-lib-table URIs exist.
2. User: open schematic — no "library not found" warnings; Symbol Chooser shows V5-local libs.
3. ERC / DRC / parity re-run → expect 0/0/0 (design files untouched, so anything else means stop and investigate).
4. Commit as its own checkpoint before the J8 0170 swap.

## Known items deliberately NOT in scope
- J8 0070→0170 swap (user does in KiCad after this lands; the 0170 symbol is verified fixed; the 0170 .kicad_mod still needs its solder-mask margins zeroed before use — same for the 0070 lib file).
- Global sym-lib-table cleanup (needed by V2/V3/V4 — separate decision).
