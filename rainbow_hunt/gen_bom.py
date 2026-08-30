"""Generate PCBWay BOM.csv from fresh netlist, matching the prior package format."""
import re, sys, csv
sys.stdout.reconfigure(encoding='utf-8')

n = open(r'C:\development\pelletpirate\rainbow_hunt\v5_pkg_netlist.net', encoding='utf-8').read()

NOSUB = {
    # ref-prefix exact refs -> reason   (carried from cancelled package + raft additions)
    'C1': 'X2 mains safety capacitor, 305VAC (triac snubbers)', 'C2': '', 'C3': '',
    'C4': 'X2 mains safety capacitor, 275VAC (line filter)',
    'C5': 'Y-class safety capacitor, line-to-earth', 'C6': '',
    'C12': 'Y-class safety capacitor, isolation barrier',
    'C7': '450V primary-side bulk electrolytic',
    'MOV1': 'Mains surge varistor',
    'T1': 'Snubberless mains triacs (load switching)', 'T2': '', 'T3': '',
    'MOC1': 'Zero-cross opto-triac drivers (mains isolation)', 'MOC2': '',
    'MOC3': 'Random-phase opto-triac driver (fan phase control) - do NOT swap with MOC3063',
    'R41': 'Anti-surge rated resistors on mains sense divider', 'R42': '',
    'PS1': 'AC/DC power module - genuine CUI only (counterfeit risk)',
    'ESP1': 'Exact flash/PSRAM variant required (16MB flash / 8MB octal PSRAM, U = external antenna)',
    'MAX1': 'Precision RTD converters (measurement accuracy)', 'MAX2': '', 'MAX3': '', 'MAX4': '', 'MAX5': '',
    'FPC1': '0.3mm-pitch FPC connector, exact mating geometry',
    'J8': 'Vertical USB-C, 1.70mm shell stake variant - do NOT substitute the -0070 or -0230 stake versions',
    # raft additions
    'FL1': 'EMI ferrite bead, panel supply filter - impedance curve is functional, exact part only',
    'L5': 'EMI ferrite bead, 12V rail filter - impedance curve is functional, exact part only',
    'C13': 'Power-module output filter electrolytic (PBO-5F Fig.2 network)',
    'C15': 'Output filter / damping electrolytics - ESR value is functional, no low-ESR substitutes', 'C48': '',
}

comps = {}
comp_sec = n[n.index('(components'):n.index('(libparts')]
for blk in re.split(r'\(comp\b', comp_sec)[1:]:
    ref = re.search(r'\(ref "([^"]+)"\)', blk)
    if not ref:
        continue
    r = ref.group(1)
    if r.startswith(('TP', 'WirePad', 'Earth', 'MH', '#')):
        continue
    val = re.search(r'\(value "([^"]*)"\)', blk)
    fp = re.search(r'\(footprint "([^"]*)"\)', blk)
    desc = re.search(r'\(description "([^"]*)"\)', blk)
    fields = dict(re.findall(r'\(name "([^"]+)"\) "([^"]*)"\)', blk))
    dnp = '(dnp yes)' in blk or re.search(r'\(property "dnp"', blk)
    comps[r] = {
        'val': (val.group(1) if val else '').strip(),
        'fp': (fp.group(1) if fp else '').split(':')[-1],
        'desc': (desc.group(1) if desc else '').strip(),
        'mf': fields.get('MF', fields.get('MANUFACTURER', '')).strip(),
        'mp': fields.get('MP', '').strip(),
        'link': fields.get('LINK', '').strip(),
        'dnp': bool(dnp),
    }

def refkey(r):
    m = re.match(r'([A-Za-z]+)(\d+)', r)
    return (m.group(1), int(m.group(2))) if m else (r, 0)

# group by (MP if present else val+fp)
groups = {}
for r, c in sorted(comps.items(), key=lambda kv: refkey(kv[0])):
    key = c['mp'] if c['mp'] else f"VAL:{c['val']}|{c['fp']}"
    groups.setdefault(key, []).append(r)

rows = []
for key, refs in groups.items():
    c = comps[refs[0]]
    nosub = any(r in NOSUB for r in refs)
    reason = next((NOSUB[r] for r in refs if NOSUB.get(r)), '')
    rows.append({
        'refs': ' '.join(refs), 'qty': len(refs), 'mf': c['mf'], 'mp': c['mp'],
        'val': c['val'], 'fp': c['fp'], 'desc': c['desc'], 'link': c['link'],
        'sub': ('NO SUBSTITUTION' if nosub else 'Equal or better spec allowed'),
    })

rows.sort(key=lambda x: refkey(x['refs'].split()[0]))
out = r'C:\development\pelletpirate\kicad\pelletpirate_V5\PCBWay\PelletPirate_V5_BOM.csv'
with open(out, 'w', newline='', encoding='utf-8-sig') as f:
    w = csv.writer(f)
    w.writerow(['Item', 'References', 'Qty', 'Manufacturer', 'Manufacturer Part Number', 'Value', 'Package', 'Description', 'DigiKey Link', 'Substitution'])
    for i, r in enumerate(rows, 1):
        w.writerow([i, r['refs'], r['qty'], r['mf'], r['mp'], r['val'], r['fp'], r['desc'], r['link'], r['sub']])

total = sum(r['qty'] for r in rows)
print(f'{len(rows)} BOM lines, {total} components -> {out}')
print('\nNO-SUB lines:')
for i, r in enumerate(rows, 1):
    if r['sub'] == 'NO SUBSTITUTION':
        print(f'  {i:2d}. {r["refs"]:<28} {r["mp"]}')
print('\nMissing MF/MP:')
for r in rows:
    if not r['mp'] or not r['mf']:
        print(f'  {r["refs"]}: MF="{r["mf"]}" MP="{r["mp"]}"')
print('\nRaft lines check:')
for probe in ('R48', 'FL1', 'L5', 'C44', 'C46', 'C48'):
    for i, r in enumerate(rows, 1):
        if probe in r['refs'].split():
            print(f'  {probe}: line {i}: {r["refs"]} | qty {r["qty"]} | {r["mp"]} | {r["sub"]}')
            break
