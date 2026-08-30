"""C48 verification: nets, fields, footprint, position, polarity, copper tap."""
import re, sys, math
sys.stdout.reconfigure(encoding='utf-8')

n = open(r'C:\development\pelletpirate\rainbow_hunt\v5_raft_netlist.net', encoding='utf-8').read()

# component record
comp_sec = n[n.index('(components'):n.index('(libparts')]
for blk in re.split(r'\(comp\b', comp_sec)[1:]:
    ref = re.search(r'\(ref "([^"]+)"\)', blk)
    if ref and ref.group(1) in ('C48', 'C15'):
        val = re.search(r'\(value "([^"]*)"\)', blk)
        fp = re.search(r'\(footprint "([^"]*)"\)', blk)
        fields = dict(re.findall(r'\(name "([^"]+)"\) "([^"]*)"\)', blk))
        mp = fields.get('MP', '')
        mf = fields.get('MF', fields.get('MANUFACTURER', ''))
        link = 'Y' if fields.get('LINK', '').strip() else 'MISSING'
        print(f'{ref.group(1)}: val={val.group(1)} fp={fp.group(1)} MF={mf} MP={mp} LINK={link}')

# nets
net_sec = n[n.index('(nets'):]
for blk in re.split(r'\(net\b', net_sec)[1:]:
    name = re.search(r'\(name "([^"]+)"\)', blk)
    if not name:
        continue
    members = re.findall(r'\(ref "([^"]+)"\)\s+\(pin "([^"]+)"\)', blk)
    for ref, pin in members:
        if ref == 'C48':
            print(f'C48 pin {pin} -> {name.group(1)}')

# board side
s = open(r'C:\development\pelletpirate\kicad\pelletpirate_V5\PelletPirate_V5.kicad_pcb', encoding='utf-8').read()
i = s.find('"C48"')
if i < 0:
    print('C48 NOT ON BOARD (Update-PCB not run?)')
else:
    fs = s.rfind('(footprint', 0, i)
    fe = s.find('(footprint', fs + 10)
    blk = s[fs:fe if fe > 0 else len(s)]
    fpname = re.search(r'\(footprint\s+"([^"]+)"', blk)
    at = re.search(r'\(at ([\d.-]+) ([\d.-]+)(?: ([\d.-]+))?\)', blk)
    ox, oy, rot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
    print(f'C48 board: {fpname.group(1)} at ({ox},{oy}) rot {rot}')
    th = math.radians(rot)
    for pm in re.finditer(r'\(pad "([^"]+)"(.*?)(?=\(pad "|\Z)', blk, re.S):
        pname, body = pm.group(1), pm.group(2)
        pat = re.search(r'\(at ([\d.-]+) ([\d.-]+)', body)
        netm = re.search(r'\(net "([^"]+)"\)', body)
        if pat:
            px, py = float(pat.group(1)), float(pat.group(2))
            bx = ox + px * math.cos(th) + py * math.sin(th)
            by = oy - px * math.sin(th) + py * math.cos(th)
            print(f'  pad {pname} at ({bx:.2f},{by:.2f}) net={netm.group(1) if netm else "?"}')
    # nearest neighbors within 6mm of can center
    print('  neighbors within 7mm:')
    for m2 in re.finditer(r'\(footprint\s+"[^"]*"', s):
        st2 = m2.start()
        en2 = s.find('(footprint', st2 + 10)
        b2 = s[st2:en2 if en2 > 0 else len(s)]
        r2 = re.search(r'\(property "Reference" "([^"]+)"', b2)
        a2 = re.search(r'\(at ([\d.-]+) ([\d.-]+)', b2)
        l2 = re.search(r'\(layer "([^"]+)"\)', b2)
        if r2 and a2 and r2.group(1) != 'C48':
            d = math.hypot(float(a2.group(1)) - ox, float(a2.group(2)) - oy)
            if d < 7:
                print(f'    {r2.group(1):>6} at {d:.2f}mm ({l2.group(1)})')

# +12V copper touching C48 pads
seg_re = re.compile(r'\(segment\s+\(start ([\d.-]+) ([\d.-]+)\)\s+\(end ([\d.-]+) ([\d.-]+)\)\s+\(width ([\d.]+)\)\s+\(layer "([^"]+)"\)\s+(?:\(locked[^)]*\)\s+)?\(net "\+12V"\)', re.S)
segs = seg_re.findall(s)
print(f'+12V total segments: {len(segs)}')
