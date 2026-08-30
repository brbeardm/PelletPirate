"""Raft audit part 2: board-level — stub copper, island, stitch vias, distances, L5 pads."""
import re, sys, math
sys.stdout.reconfigure(encoding='utf-8')

s = open(r'C:\development\pelletpirate\kicad\pelletpirate_V5\PelletPirate_V5.kicad_pcb', encoding='utf-8').read()

# ---- footprints: ref -> (x, y, rot, block)
fps = {}
for m in re.finditer(r'\(footprint\s+"[^"]*"', s):
    st = m.start()
    en = s.find('(footprint', st + 10)
    blk = s[st:en if en > 0 else len(s)]
    ref = re.search(r'\(property "Reference" "([^"]+)"', blk)
    at = re.search(r'\(at ([\d.-]+) ([\d.-]+)(?: ([\d.-]+))?\)', blk)
    if ref and at:
        fps[ref.group(1)] = (float(at.group(1)), float(at.group(2)), float(at.group(3) or 0), blk)

def pos(ref):
    return fps[ref][:2] if ref in fps else None

def dist(a, b):
    pa, pb = pos(a), pos(b)
    return math.hypot(pa[0]-pb[0], pa[1]-pb[1]) if pa and pb else None

print('=== POSITIONS / DISTANCES ===')
esp = pos('ESP1')
print(f'ESP1 at {esp}')
for r in ('R48', 'R49', 'R50', 'R51', 'C44', 'C45', 'C17', 'FL1', 'C46', 'C47', 'L5', 'FPC1', 'C15', 'D1'):
    p = pos(r)
    if p:
        d = dist(r, 'ESP1')
        df = dist(r, 'FPC1')
        print(f'{r:>4} at ({p[0]:7.2f},{p[1]:7.2f})  to-ESP1 {d:6.2f}mm  to-FPC1 {df:6.2f}mm')
    else:
        print(f'{r:>4} NOT ON BOARD')

# ---- ESP pad absolute positions for stub measurement (pads 32,33 = SDI,SCLK)
ex, ey, erot, eblk = fps['ESP1']
th = math.radians(erot)
pads = {}
for m in re.finditer(r'\(pad "([^"]+)"(.*?)(?=\(pad "|\Z)', eblk, re.S):
    name, body = m.group(1), m.group(2)
    pat = re.search(r'\(at ([\d.-]+) ([\d.-]+)', body)
    if pat:
        px, py = float(pat.group(1)), float(pat.group(2))
        bx = ex + px*math.cos(th) + py*math.sin(th)
        by = ey - px*math.sin(th) + py*math.cos(th)
        pads[name] = (bx, by)
for p in ('9', '11', '32', '33'):
    if p in pads:
        print(f'ESP1 pad {p} at ({pads[p][0]:.2f},{pads[p][1]:.2f})')

# ---- segments per net (stub nets + island)
print()
print('=== STUB NET COPPER (branch-before-R check) ===')
for netname in ('Net-(ESP1-IO39)', 'Net-(ESP1-IO40)', 'Net-(ESP1-IO16)', 'Net-(ESP1-IO18)'):
    segs = re.findall(r'\(segment \(start ([\d.-]+) ([\d.-]+)\) \(end ([\d.-]+) ([\d.-]+)\) \(width ([\d.]+)\) \(layer "([^"]+)"\) \(net "' + re.escape(netname) + '"\)', s)
    total = sum(math.hypot(float(a)-float(c), float(b)-float(d)) for a, b, c, d, w, l in segs)
    vias = len(re.findall(r'\(via .*?\(net "' + re.escape(netname) + '"\)', s))
    print(f'{netname:<22}: {len(segs)} segs, total {total:6.2f}mm, {vias} vias')

print()
print('=== /3V3_LCD ISLAND COPPER ===')
segs = re.findall(r'\(segment \(start ([\d.-]+) ([\d.-]+)\) \(end ([\d.-]+) ([\d.-]+)\) \(width ([\d.]+)\) \(layer "([^"]+)"\) \(net "/3V3_LCD"\)', s)
total = sum(math.hypot(float(a)-float(c), float(b)-float(d)) for a, b, c, d, w, l in segs)
print(f'/3V3_LCD: {len(segs)} segs, total {total:.2f}mm, widths {sorted(set(w for *_, w, l in [(x[0],x[1],x[2],x[3],x[4],x[5]) for x in segs]))}')

# ---- stitch vias near ESP (GND vias within module bounds +2mm)
print()
print('=== GND STITCH VIAS AROUND ESP (within module envelope +2.5mm) ===')
allvias = re.findall(r'\(via\s+\(at ([\d.-]+) ([\d.-]+)\).*?\(net "GND"\)', s)
count = 0
for vx, vy in allvias:
    vx, vy = float(vx), float(vy)
    if abs(vx - esp[0]) < 11.5 and abs(vy - esp[1]) < 13.5:
        count += 1
print(f'GND vias in ESP neighborhood: {count}')

# ---- L5 footprint pads
print()
print('=== L5 FOOTPRINT PADS ===')
if 'L5' in fps:
    for m in re.finditer(r'\(pad "([^"]+)".*?\(size ([\d.]+) ([\d.]+)\)', fps['L5'][3], re.S):
        print(f'  pad {m.group(1)}: {m.group(2)} x {m.group(3)}mm')

# ---- netclass patterns in .kicad_pro mentioning new nets
print()
print('=== NETCLASS PATTERN COVERAGE (.kicad_pro) ===')
pro = open(r'C:\development\pelletpirate\kicad\pelletpirate_V5\PelletPirate_V5.kicad_pro', encoding='utf-8').read()
for probe in ('3V3_LCD', 'ESP1-IO39', 'ESP1-IO40', 'ESP1-IO16', 'ESP1-IO18', 'C15-Pad1', 'ALL_SCLK', 'ALL_SDI', 'LCD_CS'):
    hits = [ln.strip() for ln in pro.splitlines() if probe in ln]
    print(f'{probe:<12}: {"COVERED " + str(hits[:2]) if hits else "NO PATTERN"}')
