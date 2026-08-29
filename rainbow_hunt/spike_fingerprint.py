"""Deep capture on SCLK: find all spikes, measure rate + inter-arrival rhythm."""
import sys, time
sys.path.insert(0, r'C:\development\pelletpirate\rainbow_hunt')
from scope_harness import Scope

sc = Scope()
sc.cmd(':CHANnel1:PROBe 10'); sc.cmd(':CHANnel1:SCALe 0.5'); sc.cmd(':CHANnel1:OFFSet -1.5')
sc.cmd(':ACQuire:MDEPth 10M')
sc.cmd(':TIMebase:MAIN:SCALe 0.002')      # 20ms window
sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(':TRIGger:EDGE:SOURce CHANnel1')
sc.cmd(':TRIGger:SWEep AUTO'); sc.cmd(':RUN')
time.sleep(3)
sc.cmd(':STOP')
sc.cmd(':WAVeform:SOURce CHANnel1'); sc.cmd(':WAVeform:MODE RAW'); sc.cmd(':WAVeform:FORMat BYTE')
sc.cmd(':WAVeform:STARt 1'); sc.cmd(':WAVeform:STOP 10000000')
pre = sc.query(':WAVeform:PREamble?').split(',')
xinc = float(pre[4]); yinc, yorig, yref = float(pre[7]), float(pre[8]), float(pre[9])
raw = sc.query_block(':WAVeform:DATA?')
sc.cmd(':RUN'); sc.close()
sr = 1.0 / xinc
n = len(raw)
print(f'{n} pts @ {sr/1e6:.0f}MSa/s ({n/sr*1000:.1f}ms window)')
open(r'C:\development\pelletpirate\rainbow_hunt\captures\sclk_fingerprint.bin', 'wb').write(raw)

# byte-domain thresholds (avoid float conversion of 10M pts twice)
def volts_to_byte(v):
    return v / yinc + yorig + yref
HI = volts_to_byte(2.31); LO = volts_to_byte(0.99); MID = volts_to_byte(1.65)
MAXW = int(60e-9 * sr)   # spikes are < 60ns

events = []
i = 1
state_hi = raw[0] > MID
while i < n:
    b = raw[i]
    if state_hi and b < HI:
        # candidate downward spike from high: how long until back above HI?
        j = i
        while j < n and raw[j] < HI:
            j += 1
        w = j - i
        if w <= MAXW and j < n:
            depth = min(raw[i:j])
            events.append((i / sr, 'down', w / sr * 1e9, (depth - yorig - yref) * yinc))
        else:
            state_hi = False   # legit falling edge
        i = j
        continue
    if not state_hi and b > LO:
        j = i
        while j < n and raw[j] > LO:
            j += 1
        w = j - i
        if w <= MAXW and j < n:
            peak = max(raw[i:j])
            events.append((i / sr, 'up', w / sr * 1e9, (peak - yorig - yref) * yinc))
        else:
            state_hi = True    # legit rising edge
        i = j
        continue
    i += 1

print(f'{len(events)} spike events in {n/sr*1000:.1f}ms -> rate {len(events)/(n/sr):.0f}/s')
for t, kind, w, v in events[:15]:
    print(f'  t={t*1000:8.3f}ms {kind:>4} width={w:5.1f}ns excursion={v:.2f}V')
if len(events) > 2:
    gaps = [(events[k+1][0] - events[k][0]) * 1e6 for k in range(len(events) - 1)]
    gaps_sorted = sorted(gaps)
    print(f'inter-arrival us: min {gaps_sorted[0]:.1f} / median {gaps_sorted[len(gaps)//2]:.1f} / max {gaps_sorted[-1]:.1f}')
    from collections import Counter
    binned = Counter(round(g / 50) * 50 for g in gaps)
    print('spacing histogram (50us bins):', dict(sorted(binned.items())[:12]))
