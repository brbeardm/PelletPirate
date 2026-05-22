import uuid

def uid():
    return str(uuid.uuid4())

def fmt(v):
    """Format a float to 2 decimal places, stripping trailing zeros but keeping at least one decimal."""
    return f"{v:.2f}"

def make_channel(u_ref, cx, cy, r_ref, c_ref, jp_ref, j_ref, j_label, gnd_ref, vcc_ref):
    rx, ry = cx+24.13, cy-10.16
    capx, capy = cx+24.13, cy+8.89
    jpx, jpy = cx+30.48, cy+1.27
    jx, jy = cx+45.72, cy-1.27

    # Pin positions
    bias = (cx+15.24, cy-15.24)
    refp = (cx+15.24, cy-12.7)
    refm = (cx+15.24, cy-7.62)
    isen = (cx+15.24, cy-5.08)
    fp   = (cx+15.24, cy)
    f2   = (cx+15.24, cy+2.54)
    rp   = (cx+15.24, cy+5.08)
    rm   = (cx+15.24, cy+10.16)
    fm   = (cx+15.24, cy+12.7)
    gnd  = (cx+2.54, cy+17.78)
    dgnd = (cx-2.54, cy+17.78)
    dvdd = (cx-2.54, cy-20.32)
    vdd  = (cx+2.54, cy-20.32)

    r_top = (rx, ry-3.81)
    r_bot = (rx, ry+3.81)
    c_top = (capx, capy-2.54)
    c_bot = (capx, capy+2.54)
    jp_top = (jpx, jpy-2.54)
    jp_bot = (jpx, jpy+2.54)
    jt = (jx-5.08, jy-2.54)
    jr = (jx-5.08, jy)
    js = (jx-5.08, jy+2.54)

    rcx = cx + 19.05
    gnd_x = cx + 2.54
    gnd_y = cy + 25.4
    vcc_x = cx - 2.54
    vcc_y = cy - 24.13

    lines = []

    def w(x1, y1, x2, y2):
        lines.append(f'\t(wire\n\t\t(pts\n\t\t\t(xy {fmt(x1)} {fmt(y1)}) (xy {fmt(x2)} {fmt(y2)})\n\t\t)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n\t\t(uuid "{uid()}")\n\t)')

    def j(x, y):
        lines.append(f'\t(junction\n\t\t(at {fmt(x)} {fmt(y)})\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)\n\t\t(uuid "{uid()}")\n\t)')

    def symbol(lib_id, ref, x, y, props_str, pins_str, rotation=0, mirror=""):
        rot_str = f" {rotation}"
        mirror_str = f"\n\t\t(mirror {mirror})" if mirror else ""
        lines.append(f'\t(symbol\n\t\t(lib_id "{lib_id}")\n\t\t(at {fmt(x)} {fmt(y)}{rot_str}){mirror_str}\n\t\t(unit 1)\n\t\t(body_style 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(in_pos_files yes)\n\t\t(dnp no)\n\t\t(uuid "{uid()}")\n{props_str}\n{pins_str}\n\t\t(instances\n\t\t\t(project "PelletPirate-rev2026-00"\n\t\t\t\t(path "/a462ae40-4659-4403-8ea2-3d9d06403884"\n\t\t\t\t\t(reference "{ref}")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')

    def prop(name, val, x, y, hide=False, justify_left=False):
        hide_str = "\n\t\t\t(hide yes)" if hide else ""
        just_str = "\n\t\t\t\t(justify left)" if justify_left else ""
        return f'\t\t(property "{name}" "{val}"\n\t\t\t(at {fmt(x)} {fmt(y)} 0){hide_str}\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t){just_str}\n\t\t\t)\n\t\t)'

    def pin(num):
        return f'\t\t(pin "{num}"\n\t\t\t(uuid "{uid()}")\n\t\t)'

    # --- Components ---

    # Resistor
    r_props = '\n'.join([
        prop("Reference", r_ref, rx+2.54, ry-1.27, justify_left=True),
        prop("Value", "400", rx+2.54, ry+1.27, justify_left=True),
        prop("Footprint", "", rx, ry, hide=True),
        prop("Datasheet", "", rx, ry, hide=True),
        prop("Description", "Resistor, US symbol", rx, ry, hide=True),
    ])
    r_pins = '\n'.join([pin("1"), pin("2")])
    symbol("Device:R_US", r_ref, rx, ry, r_props, r_pins)

    # Cap
    c_props = '\n'.join([
        prop("Reference", c_ref, capx+2.54, capy-1.14, justify_left=True),
        prop("Value", "0.22uF", capx+2.54, capy+1.4, justify_left=True),
        prop("Footprint", "", capx, capy, hide=True),
        prop("Datasheet", "", capx, capy, hide=True),
        prop("Description", "capacitor, small US symbol", capx, capy, hide=True),
    ])
    c_pins = '\n'.join([pin("1"), pin("2")])
    symbol("Device:C_Small_US", c_ref, capx, capy, c_props, c_pins)

    # Jumper
    jp_props = '\n'.join([
        prop("Reference", jp_ref, jpx+5.08, jpy),
        prop("Value", "2wire", jpx+2.54, jpy),
        prop("Footprint", "", jpx, jpy, hide=True),
        prop("Datasheet", "", jpx, jpy, hide=True),
        prop("Description", "Jumper, 2-pole, small symbol, bridged", jpx, jpy, hide=True),
    ])
    jp_pins = '\n'.join([pin("1"), pin("2")])
    symbol("Jumper:Jumper_2_Small_Bridged", jp_ref, jpx, jpy, jp_props, jp_pins, rotation=90, mirror="x")

    # Audio Jack
    j_props = '\n'.join([
        prop("Reference", j_ref, jx+1.9, jy+8.89),
        prop("Value", j_label, jx+1.9, jy+6.35),
        prop("Footprint", "", jx, jy, hide=True),
        prop("Datasheet", "", jx, jy, hide=True),
        prop("Description", "Audio Jack, 3 Poles (Stereo / TRS)", jx, jy, hide=True),
    ])
    j_pins = '\n'.join([pin("S"), pin("R"), pin("T")])
    symbol("Connector_Audio:AudioJack3", j_ref, jx, jy, j_props, j_pins, rotation=180)

    # GND symbol
    g_props = '\n'.join([
        prop("Reference", gnd_ref, gnd_x, gnd_y+6.35, hide=True),
        prop("Value", "GND", gnd_x, gnd_y+5.08),
        prop("Footprint", "", gnd_x, gnd_y, hide=True),
        prop("Datasheet", "", gnd_x, gnd_y, hide=True),
        prop("Description", 'Power symbol creates a global label with name \\"GND\\" , ground', gnd_x, gnd_y, hide=True),
    ])
    g_pins = pin("1")
    symbol("power:GND", gnd_ref, gnd_x, gnd_y, g_props, g_pins)

    # VCC symbol
    v_props = '\n'.join([
        prop("Reference", vcc_ref, vcc_x, vcc_y-3.81, hide=True),
        prop("Value", "VCC3V3", vcc_x, vcc_y+5.08),
        prop("Footprint", "", vcc_x, vcc_y, hide=True),
        prop("Datasheet", "", vcc_x, vcc_y, hide=True),
        prop("Description", 'Power symbol creates a global label with name \\"VCC\\"', vcc_x, vcc_y, hide=True),
    ])
    v_pins = pin("1")
    symbol("power:VCC", vcc_ref, vcc_x, vcc_y, v_props, v_pins, mirror="x")

    # --- Wires ---

    # BIAS -> REFIN+ -> R top
    w(bias[0], bias[1], rcx, bias[1])
    w(rcx, bias[1], rcx, refp[1])
    w(refp[0], refp[1], rcx, refp[1])
    w(rcx, refp[1], rx, refp[1])
    w(rx, refp[1], rx, r_top[1])
    j(rcx, refp[1])

    # REFIN- -> R bottom <- ISENSOR
    w(refm[0], refm[1], rcx, refm[1])
    w(rcx, refm[1], rx, refm[1])
    w(rx, refm[1], rx, r_bot[1])
    w(isen[0], isen[1], rcx, isen[1])
    w(rcx, isen[1], rx, isen[1])
    w(rx, isen[1], rx, refm[1])
    j(rx, refm[1])

    # FORCE+ shorted to FORCE2
    w(fp[0], fp[1], rcx, fp[1])
    w(f2[0], f2[1], rcx, f2[1])
    w(rcx, fp[1], rcx, f2[1])
    j(rcx, f2[1])

    # FORCE+/FORCE2 -> Tip -> JP top
    w(rcx, f2[1], rx, f2[1])
    w(rx, f2[1], rx, jt[1])
    w(rx, jt[1], jpx, jt[1])
    j(jpx, jt[1])
    w(jpx, jt[1], jt[0], jt[1])
    w(jpx, jt[1], jpx, jp_top[1])

    # JP bottom -> RTDIN+
    w(jpx, jp_bot[1], jpx, rp[1])

    # RTDIN+ -> C top, -> Ring
    w(rp[0], rp[1], rx, rp[1])
    j(rx, rp[1])
    w(rx, rp[1], rx, c_top[1])
    w(rx, rp[1], jpx, rp[1])
    j(jpx, rp[1])
    w(jpx, rp[1], jr[0], rp[1])
    w(jr[0], rp[1], jr[0], jr[1])

    # C bottom -> RTDIN-/FORCE- node
    w(rx, c_bot[1], rx, rm[1])
    j(rx, rm[1])

    # RTDIN- shorted to FORCE-
    w(rm[0], rm[1], rcx, rm[1])
    w(fm[0], fm[1], rcx, fm[1])
    w(rcx, rm[1], rcx, fm[1])
    j(rcx, fm[1])
    w(rcx, fm[1], rx, fm[1])
    w(rx, fm[1], rx, rm[1])

    # FORCE-/RTDIN- -> Sleeve
    w(rx, rm[1], js[0], rm[1])
    w(js[0], rm[1], js[0], js[1])

    # GND
    w(gnd[0], gnd[1], gnd_x, gnd[1])
    w(dgnd[0], dgnd[1], gnd_x-5.08, dgnd[1])
    w(gnd_x, gnd[1], gnd_x, gnd_y)
    w(gnd_x-5.08, dgnd[1], gnd_x-5.08, gnd_y)
    w(gnd_x-5.08, gnd_y, gnd_x, gnd_y)
    j(gnd_x, gnd_y)

    # VDD/DVDD -> VCC3V3
    w(dvdd[0], dvdd[1], vcc_x, dvdd[1])
    w(vdd[0], vdd[1], vcc_x+5.08, vdd[1])
    w(vcc_x, dvdd[1], vcc_x, vcc_y)
    w(vcc_x+5.08, vdd[1], vcc_x+5.08, vcc_y)
    w(vcc_x, vcc_y, vcc_x+5.08, vcc_y)
    j(vcc_x, vcc_y)

    return '\n'.join(lines)


channels = [
    ("U2", 378.46, 149.86, "R2", "C3", "JP2", "J2", "Meat 1", "#PWR05", "#PWR06"),
    ("U3", 275.59, 152.4,  "R3", "C4", "JP3", "J3", "Meat 2", "#PWR07", "#PWR08"),
    ("U4", 325.12, 179.07, "R4", "C5", "JP4", "J4", "Meat 3", "#PWR09", "#PWR10"),
    ("U5", 326.39, 227.33, "R5", "C6", "JP5", "J5", "Meat 4", "#PWR11", "#PWR12"),
]

# Read original file
with open("C:/development/pelletpirate/KiCad/PelletPirate-rev2026-00/PelletPirate-rev2026-00.kicad_sch", "r") as f:
    content = f.read()

# Generate new content
new_elements = []
for u, cx, cy, r, c, jp, jj, label, gnd, vcc in channels:
    new_elements.append(f"\t# --- {label} ({u}) ---")
    new_elements.append(make_channel(u, cx, cy, r, c, jp, jj, label, gnd, vcc))

insert_text = '\n'.join(new_elements) + '\n'

# Insert before (sheet_instances
marker = "\t(sheet_instances"
content = content.replace(marker, insert_text + marker)

with open("C:/development/pelletpirate/KiCad/PelletPirate-rev2026-00/PelletPirate-rev2026-00.kicad_sch", "w") as f:
    f.write(content)

print("Done. File updated.")
