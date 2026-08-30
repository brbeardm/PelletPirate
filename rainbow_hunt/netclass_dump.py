import json, sys
sys.stdout.reconfigure(encoding='utf-8')
pro = json.load(open(r'C:\development\pelletpirate\kicad\pelletpirate_V5\PelletPirate_V5.kicad_pro', encoding='utf-8'))
ns = pro.get('net_settings', {})
print('=== CLASSES ===')
for c in ns.get('classes', []):
    print(f"{c.get('name'):<20} track {c.get('track_width')}  clearance {c.get('clearance')}  via {c.get('via_diameter')}/{c.get('via_drill')}")
pats = ns.get('netclass_patterns', [])
print(f'=== {len(pats)} PATTERNS ===')
byclass = {}
for p in pats:
    byclass.setdefault(p.get('netclass'), []).append(p.get('pattern'))
for k in sorted(byclass):
    print(f'{k} ({len(byclass[k])}): {byclass[k]}')
