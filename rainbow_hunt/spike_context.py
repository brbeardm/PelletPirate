"""Trigger ON a runt spike, capture 32ms around it, look for neighbor spikes."""
import sys, time
sys.path.insert(0, r'C:\development\pelletpirate\rainbow_hunt')
from scope_harness import Scope, shot

sc = Scope()
sc.cmd(':CHANnel1:PROBe 10'); sc.cmd(':CHANnel1:SCALe 0.5'); sc.cmd(':CHANnel1:OFFSet -1.5')
sc.cmd(':ACQuire:MDEPth 10M')
sc.cmd(':TIMebase:MAIN:SCALe 0.002')
sc.cmd(':TRIGger:MODE RUNT')
sc.cmd(':TRIGger:RUNT:SOURce CHANnel1'); sc.cmd(':TRIGger:RUNT:POLarity POSitive')
sc.cmd(':TRIGger:RUNT:WHEN NONE')
sc.cmd(':TRIGger:RUNT:ALEVel 2.31'); sc.cmd(':TRIGger:RUNT:BLEVel 0.99')
sc.cmd(':SINGle')
print('armed, waiting for a spike...')
t0 = time.time()
while time.time() - t0 < 60:
    time.sleep(0.5)
    if sc.query(':TRIGger:STATus?') == 'STOP':
        print(f'caught at +{time.time()-t0:.1f}s')
        break
else:
    print('no catch in 60s'); sc.close(); sys.exit()

shot(sc, 'spike_context_view')
sc.cmd(':WAVeform:SOURce CHANnel1'); sc.cmd(':WAVeform:MODE RAW'); sc.cmd(':WAVeform:FORMat BYTE')
sc.cmd(':WAVeform:STARt 1'); sc.cmd(':WAVeform:STOP 10000000')
pre = sc.query(':WAVeform:PREamble?').split(',')
xinc = float(pre[4]); yinc, yorig, yref = float(pre[7]), float(pre[8]), float(pre[9])
raw = sc.query_block(':WAVeform:DATA?')
sc.cmd(':RUN'); sc.close()
sr = 1.0 / xinc; n = len(raw)
print(f'{n} pts @ {sr/1e6:.0f}MSa/s ({n/sr*1000:.1f}ms window)')
open(r'C:\development\pelletpirate\rainbow_hunt\captures\spike_context.bin', 'wb').write(raw)

def volts_to_byte(v): return v / yinc + yorig + yref
HI = volts_to_byte(2.31); LO = volts_to_byte(0.99); MID = volts_to_byte(1.65)
MAXW = int(60e-9 * sr)
events = []
i = 1
state_hi = raw[0] > MID
while i < n:
    b = raw[i]
    if state_hi and b < HI:
        j = i
        while j < n and raw[j] < HI: j += 1
        w = j - i
        if w <= MAXW and j < n:
            depth = min(raw[i:j])
            events.append((i / sr, 'down', w / sr * 1e9, (depth - yorig - yref) * yinc))
        else:
            state_hi = False
        i = j; continue
    if not state_hi and b > LO:
        j = i
        while j < n and raw[j] > LO: j += 1
        w = j - i
        if w <= MAXW and j < n:
            peak = max(raw[i:j])
            events.append((i / sr, 'up', w / sr * 1e9, (peak - yorig - yref) * yinc))
        else:
            state_hi = True
        i = j; continue
    i += 1

print(f'{len(events)} spikes in the 32ms window around the trigger:')
for t, kind, w, v in events[:30]:
    print(f'  t={t*1000:8.3f}ms {kind:>4} width={w:5.1f}ns excursion={v:.2f}V')
if len(events) > 2:
    gaps = [(events[k+1][0] - events[k][0]) * 1e3 for k in range(len(events) - 1)]
    print('spacing ms:', [f'{g:.3f}' for g in gaps[:20]])
