"""Raft audit part 2b: copper with multiline-tolerant parsing."""
import re, sys, math
sys.stdout.reconfigure(encoding='utf-8')

s = open(r'C:\development\pelletpirate\kicad\pelletpirate_V5\PelletPirate_V5.kicad_pcb', encoding='utf-8').read()

seg_re = re.compile(
    r'\(segment\s+\(start ([\d.-]+) ([\d.-]+)\)\s+\(end ([\d.-]+) ([\d.-]+)\)\s+\(width ([\d.]+)\)\s+\(layer "([^"]+)"\)\s+(?:\(locked[^)]*\)\s+)?\(net "([^"]+)"\)', re.S)
via_re = re.compile(
    r'\(via\s+\(at ([\d.-]+) ([\d.-]+)\)\s+\(size ([\d.]+)\)\s+\(drill ([\d.]+)\)\s+\(layers[^)]+\)\s+(?:\([^)]*\)\s+)*?\(net "([^"]+)"\)', re.S)

segs_by_net = {}
for a, b, c, d, w, layer, net in seg_re.findall(s):
    segs_by_net.setdefault(net, []).append((float(a), float(b), float(c), float(d), float(w), layer))
vias_by_net = {}
for x, y, sz, dr, net in via_re.findall(s):
    vias_by_net.setdefault(net, []).append((float(x), float(y)))

print(f'parse totals: {sum(len(v) for v in segs_by_net.values())} segments, {sum(len(v) for v in vias_by_net.values())} vias, {len(segs_by_net)} routed nets')
print()
for net in ('Net-(ESP1-IO39)', 'Net-(ESP1-IO40)', 'Net-(ESP1-IO16)', 'Net-(ESP1-IO18)',
            '/3V3_LCD', 'Net-(C15-Pad1)', '/ALL_SCLK', '/ALL_SDI', '/LCD_CS', '/LCD_DC'):
    ss = segs_by_net.get(net, [])
    vv = vias_by_net.get(net, [])
    total = sum(math.hypot(a-c, b-d) for a, b, c, d, w, l in ss)
    widths = sorted(set(w for *_, w, l in [(x[0], x[1], x[2], x[3], x[4], x[5]) for x in ss]))
    print(f'{net:<22}: {len(ss):3d} segs {total:7.2f}mm  widths {widths}  vias {len(vv)}')

# stub extent: max distance of any stub segment endpoint from its ESP pad
print()
pads = {'Net-(ESP1-IO39)': (159.49, 79.01), 'Net-(ESP1-IO40)': (159.49, 77.74),
        'Net-(ESP1-IO16)': (141.99, 79.01), 'Net-(ESP1-IO18)': (141.99, 81.55)}
for net, (px, py) in pads.items():
    ss = segs_by_net.get(net, [])
    if ss:
        far = max(max(math.hypot(a-px, b-py), math.hypot(c-px, d-py)) for a, b, c, d, w, l in ss)
        print(f'{net}: farthest stub copper point = {far:.2f}mm from ESP pad')

# GND stitch vias near ESP (150.735, 77.26)
gnd_vias = vias_by_net.get('GND', [])
near = [(x, y) for x, y in gnd_vias if abs(x-150.735) < 11.5 and abs(y-77.26) < 13.5]
print(f'\nGND vias in ESP neighborhood (23x27mm box): {len(near)} of {len(gnd_vias)} total')

# island geometry: /3V3_LCD segments bounding box + FL1/C46/C47 pad connectivity sanity
ss = segs_by_net.get('/3V3_LCD', [])
if ss:
    xs = [v for a, b, c, d, w, l in ss for v in (a, c)]
    ys = [v for a, b, c, d, w, l in ss for v in (b, d)]
    print(f'/3V3_LCD copper bbox: x {min(xs):.1f}-{max(xs):.1f}, y {min(ys):.1f}-{max(ys):.1f}')
