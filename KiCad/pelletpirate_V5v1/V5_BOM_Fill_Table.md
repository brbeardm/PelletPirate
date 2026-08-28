# V5 BOM Field Fill Table
Date: 2026-08-27 · For the schematic field-editing session. Sources: V4 turnkey BOM (parts PCBWay successfully sourced, value+footprint matched — NOT ref matched, V4 refs differ) and fresh DigiKey verification for parts with no prior source.

## Fill these fields (MF / MP / LINK)

| Refs | Value | MF | MP | LINK | Source |
|---|---|---|---|---|---|
| C1, C2, C3 | 0.01µF 305V X2 | EPCOS - TDK | B32921C3103M000 | https://www.digikey.com/en/products/detail/epcos-tdk-electronics/B32921C3103M000/778978 | NEW — verified: 10nF X2 305VAC PP, 13×4×9mm, 10mm LS (exact footprint match), −40..+110°C, in stock |
| C5, C6 | 470pF 440V X1Y2 | Vishay BC Components | VY2471K29Y5SS63V7 | https://www.digikey.com/en/products/detail/vishay-beyschlag-draloric-bc-components/VY2471K29Y5SS63V7/1983391 | V4 (ordered & fabbed) |
| C7 | 15µF 450V | Rubycon | 450BXG15MEFC10X20 | https://www.digikey.com/en/products/detail/rubycon/450BXG15MEFC10X20/23330777 | V4 |
| C8, C10 | 10µF 25V | KEMET | C1206C106K3RACTU | https://www.digikey.com/en/products/detail/kemet/C1206C106K3RACTU/3317648 | V4 |
| C9 | 0.1µF 25V | KEMET | C0402C104K3RACTU | https://www.digikey.com/en/products/detail/kemet/C0402C104K3RACTU/6697176 | V4 |
| J6 | Conn_01x04 | Hirose Electric | DF13-4P-1.25DSA(25) | https://www.digikey.com/en/products/detail/hirose-electric-co-ltd/DF13-4P-1-25DSA-25/15997347 | V4 |
| R1, R2, R3 | 39Ω 1W | Panasonic | ERJ-1TYJ390U | https://www.digikey.com/en/products/detail/panasonic-electronic-components/ERJ-1TYJ390U/365201 | NEW — 2512 1W 39Ω; DK page live, ⚠ stock not confirmed (page blocked scraping) — eyeball at order time |
| R4, R5, R26, R27 | 360Ω 1/4W | Vishay Dale | CRCW1206360RFKEA | https://www.digikey.com/en/products/detail/vishay-dale/CRCW1206360RFKEA/1176592 | V4 (its R26/R27) |
| R7, R8 | 330Ω | YAGEO | RC0805FR-07330RL | https://www.digikey.com/en/products/detail/yageo/RC0805FR-07330RL/727866 | NEW — verified in stock, 0805 1% 1/8W |
| R15 | 100KΩ (0805) | YAGEO | RC0805FR-07100KL | https://www.digikey.com/en/products/detail/yageo/RC0805FR-07100KL/727544 | NEW — verified in stock. NOTE: R15 is 0805, R16 is 0603 — different MPNs |
| R16 | 100KΩ (0603) | YAGEO | RC0603FR-07100KL | https://www.digikey.com/en/products/detail/yageo/RC0603FR-07100KL/726889 | V4 |
| R19 | 33.2KΩ | YAGEO | RC0805FR-0733K2L | https://www.digikey.com/en/products/detail/yageo/RC0805FR-0733K2L/727865 | V4 |
| R20 | 10KΩ | YAGEO | RC0402FR-0710KL | https://www.digikey.com/en/products/detail/yageo/RC0402FR-0710KL/726523 | V4 |

## LINK-only fills (MF/MP already present)

| Refs | LINK |
|---|---|
| J1-J5 (SJ1-3523NG) | https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/SJ1-3523NG/738692 — ⚠ DK shows backorder today; PCBWay sourced these fine on V2 and V4, informational only |
| J9 (TB002-500-02BE) | https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/TB002-500-02BE/10064069 — in stock |
| F1 (0031.8201 holder) | search DigiKey "0031.8201" Schurter — also DECIDE: 5×20mm 5A fuse cartridge is a separate purchasable, not placed by PCBWay; add a DNP/hand-load BOM line or buy separately |

## Other edits in the same session

1. **R6 vs R9 omega:** values look identical but use different Ω codepoints (R6 = U+03A9, R9 = U+2126) → splits the BOM line. Retype one to match the other.
2. **TP1, TP2:** symbol properties → check "Exclude from bill of materials".
3. (Optional, cosmetic) Board-wide the schematic mixes Ω codepoints 26:11 — only R6/R9 collide functionally; standardize the rest whenever convenient.

## Already verified clean — no action
- C21-C35: single line, Samsung CL05B104KO5NNNC, one footprint ✓
- J8: instance fields all 0170 ✓ (the 0070 string my audit first flagged lives only in the embedded symbol CACHE — inert)
- J8 masks: 12 critical pads at 0; remaining 8 (corner GND + shields) harmless ✓
- No same-Value+Footprint parts with conflicting MPNs ✓
- sch↔board footprint assignments match, parity 0 ✓
