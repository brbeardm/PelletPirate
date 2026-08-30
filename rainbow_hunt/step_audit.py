"""3D model audit: every footprint's model path resolved and checked on disk."""
import re, os, sys
sys.stdout.reconfigure(encoding='utf-8')

PRJ = r'C:\development\pelletpirate\kicad\pelletpirate_V5'
s = open(os.path.join(PRJ, 'PelletPirate_V5.kicad_pcb'), encoding='utf-8').read()

# candidate 3D dirs for KiCad 10 stock models
stock_candidates = [
    r'C:\Program Files\KiCad\10.0\share\kicad\3dmodels',
    r'C:\Program Files\KiCad\9.0\share\kicad\3dmodels',
    r'C:\Program Files\KiCad\8.0\share\kicad\3dmodels',
]
stock = next((p for p in stock_candidates if os.path.isdir(p)), None)

def resolve(path):
    p = path
    for var, sub in (('${KIPRJMOD}', PRJ), ('${KICAD10_3DMODEL_DIR}', stock), ('${KICAD9_3DMODEL_DIR}', stock),
                     ('${KICAD8_3DMODEL_DIR}', stock), ('${KICAD7_3DMODEL_DIR}', stock), ('${KICAD6_3DMODEL_DIR}', stock)):
        if var in p and sub:
            p = p.replace(var, sub)
    return os.path.normpath(p)

rows = []
for m in re.finditer(r'\(footprint\s+"([^"]*)"', s):
    st = m.start()
    en = s.find('(footprint', st + 10)
    blk = s[st:en if en > 0 else len(s)]
    ref = re.search(r'\(property "Reference" "([^"]+)"', blk)
    if not ref:
        continue
    models = re.findall(r'\(model\s+"([^"]+)"', blk)
    hidden = len(re.findall(r'\(model\s+"[^"]+"\s*\(hide yes\)', blk))
    if not models:
        rows.append((ref.group(1), m.group(1).split(':')[-1], 'NO MODEL', ''))
    for mp in models:
        rp = resolve(mp)
        ok = os.path.isfile(rp)
        if '${' in rp:
            rows.append((ref.group(1), m.group(1).split(':')[-1], 'UNRESOLVED VAR', mp))
        elif not ok:
            rows.append((ref.group(1), m.group(1).split(':')[-1], 'FILE MISSING', mp))

print(f'stock 3D dir: {stock}')
total = len(re.findall(r'\(property "Reference" "', s))
print(f'footprints: {total}, problems: {len(rows)}')
print()
for ref, fp, prob, path in sorted(rows):
    print(f'{ref:>6} | {prob:<14} | {fp[:36]:<36} | {path}')
