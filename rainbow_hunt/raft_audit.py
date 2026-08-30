"""Raft audit part 1: netlist-level — membership, stars, islands, WP4 split, fields."""
import re, sys
sys.stdout.reconfigure(encoding='utf-8')

s = open(r'C:\development\pelletpirate\rainbow_hunt\v5_raft_netlist.net', encoding='utf-8').read()

comps = {}
comp_sec = s[s.index('(components'):s.index('(libparts')]
for blk in re.split(r'\(comp\b', comp_sec)[1:]:
    ref = re.search(r'\(ref "([^"]+)"\)', blk)
    val = re.search(r'\(value "([^"]*)"\)', blk)
    fp = re.search(r'\(footprint "([^"]*)"\)', blk)
    fields = dict(re.findall(r'\(name "([^"]+)"\) "([^"]*)"\)', blk))
    if ref:
        comps[ref.group(1)] = {'val': val.group(1) if val else '', 'fp': fp.group(1) if fp else '', 'f': fields}

nets = {}
net_sec = s[s.index('(nets'):]
for blk in re.split(r'\(net\b', net_sec)[1:]:
    name = re.search(r'\(name "([^"]+)"\)', blk)
    if not name:
        continue
    members = sorted(set(re.findall(r'\(ref "([^"]+)"\)\s+\(pin "([^"]+)"\)', blk)))
    nets[name.group(1)] = members

def net_of(ref, pin):
    for nname, members in nets.items():
        if (ref, pin) in members:
            return nname
    return None

print('=== A. NEW-PART INVENTORY (refs not in the pre-raft design) ===')
# candidates: anything with value 33, BLM, or new C refs >= C44, L4
newparts = []
for ref, c in sorted(comps.items()):
    v = c['val']
    if 'BLM' in v or v.strip() in ('33', '33R', '33Ω') or '33Ω' in v or ref == 'L4':
        newparts.append(ref)
    m = re.match(r'C(\d+)$', ref)
    if m and int(m.group(1)) >= 44:
        newparts.append(ref)
newparts = sorted(set(newparts))
for ref in newparts:
    c = comps[ref]
    mp = c['f'].get('MP', c['f'].get('Mfr. No', ''))
    mf = c['f'].get('MF', c['f'].get('MANUFACTURER', ''))
    link = 'Y' if ('LINK' in c['f'] and c['f']['LINK'].strip()) else 'MISSING'
    pins = {p: net_of(ref, p) for p in ('1', '2')}
    print(f'{ref:>5} | {c["val"]:<22} | {c["fp"].split(":")[-1][:44]:<44} | MF={mf[:16]:<16} | MP={mp:<22} | LINK={link}')
    print(f'      pins: 1->{pins["1"]}  2->{pins["2"]}')

print()
print('=== B. SPI STAR AUDIT — nets containing 33-ohm resistors + ESP SPI pins ===')
esp_spi = {'31': 'IO39?/check', '32': '/ALL_SDI-was', '33': '/ALL_SCLK-was', '34': '/ALL_SDO-was', '9': 'LCD_CS-was', '11': 'LCD_DC-was'}
for pad in ('9', '11', '31', '32', '33', '34'):
    nn = net_of('ESP1', pad)
    if nn:
        print(f'ESP1 pad {pad:>2} -> net {nn:<24} members: {nets[nn]}')
print()
spi_new = {n: m for n, m in nets.items() if any(('SCLK' in n.upper() or 'SDI' in n.upper() or 'SDO' in n.upper() or 'LCD_CS' in n.upper() or 'LCD_DC' in n.upper()) for _ in [0])}
for n in sorted(spi_new):
    print(f'{n:<26} ({len(spi_new[n])} nodes): {spi_new[n]}')

print()
print('=== C. 3V3_PANEL ISLAND ===')
for n in nets:
    if 'PANEL' in n.upper() or 'LCD' in n.upper() and '3V3' in n.upper() or '3V3' in n.upper():
        print(f'{n}: {nets[n]}')
print('FPC1 3.3V pins land on:', net_of('FPC1', '3'), net_of('FPC1', '34'), net_of('FPC1', '35'))

print()
print('=== D. WP4 12V SPLIT ===')
for n in nets:
    mem = nets[n]
    if ('L4', '1') in mem or ('L4', '2') in mem:
        print(f'{n} ({len(mem)} nodes): {mem}')
print('D1.1 on:', net_of('D1', '1'), '| C15.1 on:', net_of('C15', '1'), '| C13.1 on:', net_of('C13', '1'))
print('PS2.3 on:', net_of('PS2', '3'), '| PS3.1 on:', net_of('PS3', '1'), '| J7.1 on:', net_of('J7', '1'), '| L3.1 on:', net_of('L3', '1'))

print()
print('=== E. +3.3V NET (post-raft membership; C44/C45 present? FPC gone?) ===')
m = nets.get('+3.3V', [])
print(f'+3.3V ({len(m)} nodes): {m}')
