"""Locate ESP1 SPI/GND castellation pads on the V4 board for probe-wire soldering."""
import re, sys, math
sys.stdout.reconfigure(encoding='utf-8')

s = open(r'C:\development\pelletpirate\kicad\pelletpirate_v4\PelletPirate_V4.kicad_pcb', encoding='utf-8').read()
i = s.find('"ESP1"')
fs = s.rfind('(footprint', 0, i)
fe = s.find('(footprint', fs + 10)
blk = s[fs:fe if fe > 0 else len(s)]
at = re.search(r'\(at ([\d.]+) ([\d.]+)(?: ([\d.-]+))?\)', blk)
ox, oy, rot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
print(f'ESP1 origin ({ox}, {oy}) rot {rot}')

targets = {'/ALL_SCLK', '/ALL_SDI', '/ALL_SDO', '/LCD_CS', 'GND'}
th = math.radians(rot)
for m in re.finditer(r'\(pad "([^"]+)"(.*?)(?=\(pad "|\Z)', blk, re.S):
    name, body = m.group(1), m.group(2)
    pat = re.search(r'\(at ([\d.-]+) ([\d.-]+)(?: ([\d.-]+))?\)', body)
    net = re.search(r'\(net "([^"]*)"\)', body)
    netname = net.group(1) if net else '?'
    if netname in targets or name in ('1', '40', '41'):
        px, py = float(pat.group(1)), float(pat.group(2))
        bx = ox + px * math.cos(th) + py * math.sin(th)
        by = oy - px * math.sin(th) + py * math.cos(th)
        print(f'pad {name:>3}  board({bx:7.2f},{by:7.2f})  net={netname}')
