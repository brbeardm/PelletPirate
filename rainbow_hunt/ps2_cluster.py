"""Extract the PS2 buck cluster from the V5 netlist: nets on PS2 pins + component values."""
import re, sys
sys.stdout.reconfigure(encoding='utf-8')

s = open(r'C:\development\pelletpirate\rainbow_hunt\v5_audit_netlist.net', encoding='utf-8').read()

# --- components: ref -> (value, footprint, MPN-ish fields)
comps = {}
comp_sec = s[s.index('(components'):s.index('(libparts')]
for blk in re.split(r'\(comp\b', comp_sec)[1:]:
    ref = re.search(r'\(ref "([^"]+)"\)', blk)
    val = re.search(r'\(value "([^"]*)"\)', blk)
    fp = re.search(r'\(footprint "([^"]*)"\)', blk)
    fields = dict(re.findall(r'\(name "([^"]+)"\) "([^"]*)"\)', blk))
    if ref:
        comps[ref.group(1)] = (val.group(1) if val else '', fp.group(1) if fp else '', fields)

# --- nets: name -> [(ref, pin, pinfunction)]
nets = {}
net_sec = s[s.index('(nets'):]
for blk in re.split(r'\(net\b', net_sec)[1:]:
    name = re.search(r'\(name "([^"]+)"\)', blk)
    if not name:
        continue
    members = []
    for node in re.finditer(r'\(node\s+\(ref "([^"]+)"\)\s+\(pin "([^"]+)"\)(?:\s+\(pinfunction "([^"]+)"\))?', blk):
        members.append((node.group(1), node.group(2), node.group(3) or ''))
    nets[name.group(1)] = members

# nets touching PS2
for name, members in nets.items():
    refs = [m[0] for m in members]
    if 'PS2' in refs:
        print(f'=== NET {name} ({len(members)} nodes)')
        for ref, pin, pf in sorted(set(members)):
            val, fp, fields = comps.get(ref, ('?', '?', {}))
            mpn = fields.get('MPN') or fields.get('Mfr. No') or fields.get('MANUFACTURER_PART_NUMBER') or ''
            print(f'  {ref}.{pin:<3} {pf:<10} | {val:<28} | {fp.split(":")[-1][:38]:<38} | {mpn}')
        print()

# also dump L-and-cap details for anything on the +3.3V net explicitly
print('--- key component fields ---')
for ref in ['PS2', 'L1', 'L2', 'L3', 'R19', 'R20', 'R15', 'C14']:
    if ref in comps:
        val, fp, fields = comps[ref]
        interesting = {k: v for k, v in fields.items() if v and v != 'None' and k not in ('Price',)}
        print(f'{ref}: {val} | {fp} | {interesting}')
