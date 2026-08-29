"""Board 3 AC scan: rms on both rails + deep comb spectrum on 3.3V."""
import sys, math, json, time
sys.path.insert(0, r'C:\development\pelletpirate\rainbow_hunt')
from scope_harness import Scope, shot

sc = Scope()
sc.cmd(':CHANnel1:COUPling AC'); sc.cmd(':CHANnel1:SCALe 0.5'); sc.cmd(':CHANnel1:OFFSet 0')
sc.cmd(':CHANnel2:COUPling AC'); sc.cmd(':CHANnel2:SCALe 0.2'); sc.cmd(':CHANnel2:OFFSet 0')
sc.cmd(':TIMebase:MAIN:SCALe 0.002'); sc.cmd(':RUN')
time.sleep(2)
for ch, name in ((1, '12V'), (2, '3.3V')):
    vpps, vrmss = [], []
    for i in range(5):
        vpp = float(sc.query(f':MEASure:ITEM? VPP,CHANnel{ch}'))
        vrms = float(sc.query(f':MEASure:ITEM? VRMS,CHANnel{ch}'))
        if vpp < 1e30: vpps.append(vpp)
        if vrms < 1e30: vrmss.append(vrms)
        time.sleep(0.4)
    pp = f'{min(vpps)*1000:.0f}-{max(vpps)*1000:.0f}mVpp' if vpps else 'INVALID'
    rm = f'{min(vrmss)*1000:.1f}-{max(vrmss)*1000:.1f}mVrms' if vrmss else 'INVALID'
    print(f'CH{ch} ({name}): {pp}  |  {rm}')
shot(sc, 'board3_AC_rails')

# deep capture on 3.3V for comb scan
sc.cmd(':TIMebase:MAIN:SCALe 0.01'); sc.cmd(':ACQuire:MDEPth 1M'); sc.cmd(':RUN')
time.sleep(3)
sc.cmd(':STOP')
sc.cmd(':WAVeform:SOURce CHANnel2'); sc.cmd(':WAVeform:MODE RAW'); sc.cmd(':WAVeform:FORMat BYTE')
sc.cmd(':WAVeform:STARt 1'); sc.cmd(':WAVeform:STOP 1000000')
pre = sc.query(':WAVeform:PREamble?').split(',')
xinc = float(pre[4]); yinc, yorig, yref = float(pre[7]), float(pre[8]), float(pre[9])
raw = sc.query_block(':WAVeform:DATA?')
sc.cmd(':RUN'); sc.close()
sr = 1.0 / xinc
print(f'deep: {len(raw)} pts @ {sr/1e6:.3g}MSa/s')
v = [(b - yorig - yref) * yinc for b in raw]
mean = sum(v) / len(v); v = [x - mean for x in v]
n = len(v)

def band(f):
    coeff = 2 * math.cos(2 * math.pi * f / sr)
    s1 = s2 = 0.0
    for x in v:
        s0 = x + coeff * s1 - s2
        s2 = s1; s1 = s0
    return math.sqrt(max(s1 * s1 + s2 * s2 - coeff * s1 * s2, 0)) / n * 2

freqs = [3500, 5000, 7000, 10500, 14000, 17500, 21000, 24500, 28000, 31500, 35000, 42000, 50000, 70000, 90000, 117000, 234000]
res = {f: band(f) for f in freqs}
mx = max(res.values())
for f in freqs:
    print(f'{f/1000:7.1f}kHz  {res[f]*1000:7.3f}mV  ' + '#' * int(res[f] / mx * 40))
json.dump({str(k): val for k, val in res.items()},
          open(r'C:\development\pelletpirate\rainbow_hunt\captures\board3_AC_3V3_spectrum.json', 'w'))
