"""Survey the region around L5/PS2/PS3 for a D6.3mm radial site on +12V."""
import re, sys, math
sys.stdout.reconfigure(encoding='utf-8')

s = open(r'C:\development\pelletpirate\kicad\pelletpirate_V5\PelletPirate_V5.kicad_pcb', encoding='utf-8').read()

# board outline
edges = re.findall(r'\(gr_line\s+\(start ([\d.-]+) ([\d.-]+)\)\s+\(end ([\d.-]+) ([\d.-]+)\)\s+.*?\(layer "Edge.Cuts"\)', s, re.S)
xs = [float(v) for e in edges for v in (e[0], e[2])]
ys = [float(v) for e in edges for v in (e[1], e[3])]
if xs:
    print(f'board outline: x {min(xs):.1f}-{max(xs):.1f}, y {min(ys):.1f}-{max(ys):.1f}')

# footprints with pad extents
fps = {}
for m in re.finditer(r'\(footprint\s+"([^"]*)"', s):
    st = m.start()
    en = s.find('(footprint', st + 10)
    blk = s[st:en if en > 0 else len(s)]
    ref = re.search(r'\(property "Reference" "([^"]+)"', blk)
    at = re.search(r'\(at ([\d.-]+) ([\d.-]+)(?: ([\d.-]+))?\)', blk)
    layer = re.search(r'\(layer "([^"]+)"\)', blk)
    if not (ref and at):
        continue
    ox, oy, rot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
    th = math.radians(rot)
    pts = []
    for pm in re.finditer(r'\(pad "[^"]*"\s+\S+\s+\S+\s+\(at ([\d.-]+) ([\d.-]+)[^)]*\)\s+\(size ([\d.]+) ([\d.]+)\)', blk):
        px, py, w, h = (float(pm.group(i)) for i in range(1, 5))
        r = math.hypot(w, h) / 2
        bx = ox + px * math.cos(th) + py * math.sin(th)
        by = oy - px * math.sin(th) + py * math.cos(th)
        pts.append((bx, by, r))
    if pts:
        minx = min(p[0] - p[2] for p in pts); maxx = max(p[0] + p[2] for p in pts)
        miny = min(p[1] - p[2] for p in pts); maxy = max(p[1] + p[2] for p in pts)
    else:
        minx = maxx = ox; miny = maxy = oy
    fps[ref.group(1)] = (ox, oy, minx, maxx, miny, maxy, layer.group(1) if layer else '?')

for r in ('L5', 'D1', 'PS2', 'PS3', 'L2', 'L3', 'C8', 'C9', 'C10', 'C11', 'C14', 'R15', 'J7', 'C15', 'C13', 'MOV1', 'C16', 'C36', 'R16', 'R17'):
    if r in fps:
        ox, oy, a, b, c, d, lay = fps[r]
        print(f'{r:>5} at ({ox:7.2f},{oy:7.2f})  bbox x{a:6.1f}-{b:6.1f} y{c:6.1f}-{d:6.1f}  {lay}')

# occupancy map region around L5/PS2: choose window from L5 and PS2 positions
inter = ['L5', 'PS2', 'PS3', 'J7']
cx = sum(fps[r][0] for r in inter if r in fps) / len([r for r in inter if r in fps])
cy = sum(fps[r][1] for r in inter if r in fps) / len([r for r in inter if r in fps])
x0, x1 = cx - 22, cx + 22
y0, y1 = cy - 18, cy + 18
print(f'\n=== occupancy map x {x0:.0f}-{x1:.0f}, y {y0:.0f}-{y1:.0f} (1mm cells; letters=parts, .=free) ===')
cells = {}
for ref, (ox, oy, a, b, c, d, lay) in fps.items():
    for gx in range(int(a), int(b) + 1):
        for gy in range(int(c), int(d) + 1):
            if x0 <= gx <= x1 and y0 <= gy <= y1:
                cells.setdefault((gx, gy), ref)
hdr = '     ' + ''.join(str(int(gx) // 10 % 10) for gx in range(int(x0), int(x1) + 1))
hdr2 = '     ' + ''.join(str(int(gx) % 10) for gx in range(int(x0), int(x1) + 1))
print(hdr); print(hdr2)
legend = {}
for gy in range(int(y0), int(y1) + 1):
    row = f'{gy:4d} '
    for gx in range(int(x0), int(x1) + 1):
        ref = cells.get((gx, gy))
        if ref:
            ch = legend.setdefault(ref, chr(ord('A') + len(legend) % 26))
            row += ch
        else:
            row += '.'
    print(row)
print('legend:', {v: k for k, v in legend.items()})

# +12V copper segments in the window (tap points)
seg_re = re.compile(r'\(segment\s+\(start ([\d.-]+) ([\d.-]+)\)\s+\(end ([\d.-]+) ([\d.-]+)\)\s+\(width ([\d.]+)\)\s+\(layer "([^"]+)"\)\s+(?:\(locked[^)]*\)\s+)?\(net "\+12V"\)', re.S)
print('\n=== +12V segments in window ===')
for a, b, c, d, w, lay in seg_re.findall(s):
    a, b, c, d = float(a), float(b), float(c), float(d)
    if x0 <= a <= x1 and y0 <= b <= y1:
        print(f'  ({a:.1f},{b:.1f})->({c:.1f},{d:.1f}) w{w} {lay}')
