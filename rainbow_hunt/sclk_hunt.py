"""SCLK glitch hunt phase 1: high-rate captures of active bursts + idle line on AC.
Usage: python sclk_hunt.py <label>"""
import sys, time, math
sys.path.insert(0, r'C:\development\pelletpirate\rainbow_hunt')
from scope_harness import Scope, shot

label = sys.argv[1] if len(sys.argv) > 1 else 'ac'
sc = Scope()

# CH1 at 10x probe, logic scaling
sc.cmd(':CHANnel1:DISPlay ON'); sc.cmd(':CHANnel1:COUPling DC'); sc.cmd(':CHANnel1:PROBe 10')
sc.cmd(':CHANnel1:BWLimit OFF'); sc.cmd(':CHANnel1:SCALe 0.5'); sc.cmd(':CHANnel1:OFFSet -1.5')

def grab(tag, trigger_edge):
    sc.cmd(':ACQuire:MDEPth 1M')
    sc.cmd(':TIMebase:MAIN:SCALe 0.0001')       # 1ms window @ 1M -> 1GSa/s
    sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(':TRIGger:EDGE:SOURce CHANnel1')
    sc.cmd(':TRIGger:EDGE:SLOPe POSitive'); sc.cmd(':TRIGger:EDGE:LEVel 1.65')
    sc.cmd(f':TRIGger:SWEep {"NORMal" if trigger_edge else "AUTO"}')
    sc.cmd(':RUN'); time.sleep(3)
    sc.cmd(':STOP')
    sc.cmd(':WAVeform:SOURce CHANnel1'); sc.cmd(':WAVeform:MODE RAW'); sc.cmd(':WAVeform:FORMat BYTE')
    sc.cmd(':WAVeform:STARt 1'); sc.cmd(':WAVeform:STOP 1000000')
    pre = sc.query(':WAVeform:PREamble?').split(',')
    xinc = float(pre[4]); yinc, yorig, yref = float(pre[7]), float(pre[8]), float(pre[9])
    raw = sc.query_block(':WAVeform:DATA?')
    open(rf'C:\development\pelletpirate\rainbow_hunt\captures\{label}_{tag}.bin', 'wb').write(raw)
    v = [(b - yorig - yref) * yinc for b in raw]
    sr = 1.0 / xinc
    print(f'--- {tag}: {len(v)} pts @ {sr/1e6:.0f}MSa/s ({len(v)/sr*1000:.2f}ms)')
    return v, sr

def analyze(tag, v, sr):
    n = len(v)
    HI, LO = 2.31, 0.99            # HX8357 CMOS thresholds at 3.3V (0.7/0.3 VDD)
    # classify samples
    hi = [x > HI for x in v]
    lo = [x < LO for x in v]
    mid = sum(1 for x in v if LO <= x <= HI)
    print(f'  samples: {n}, in-forbidden-band(0.99-2.31V): {mid} ({100*mid/n:.2f}%)')
    # idle analysis: longest run below LO -> noise stats there
    best_start = best_len = cur_start = cur_len = 0
    for i, l in enumerate(lo):
        if l:
            if cur_len == 0: cur_start = i
            cur_len += 1
            if cur_len > best_len: best_len, best_start = cur_len, cur_start
        else:
            cur_len = 0
    if best_len > 1000:
        seg = v[best_start:best_start + best_len]
        m = sum(seg) / len(seg)
        rms = math.sqrt(sum((x - m) ** 2 for x in seg) / len(seg))
        print(f'  idle segment: {best_len/sr*1e6:.0f}us, mean {m*1000:.0f}mV, noise {rms*1000:.1f}mVrms, peak {max(seg)*1000:.0f}mV')
    # runt detection: local maxima that enter the forbidden band from low without reaching HI
    runts = 0
    i = 1
    while i < n - 1:
        if v[i] > LO and not hi[i]:
            j = i
            peak = v[i]
            while j < n - 1 and v[j] > LO:
                peak = max(peak, v[j]); j += 1
            if peak < HI and (j - i) > 2:      # excursion above LO that never reached HI
                runts += 1
                if runts <= 5:
                    print(f'  RUNT? t={i/sr*1e6:.2f}us peak={peak:.2f}V width={(j-i)/sr*1e9:.0f}ns')
            i = j
        i += 1
    print(f'  runt-like excursions: {runts}')
    # half-period stats during bursts: time between threshold crossings at 1.65V
    crossings = [i for i in range(1, n) if (v[i-1] < 1.65) != (v[i] < 1.65)]
    if len(crossings) > 10:
        gaps = [ (crossings[k+1]-crossings[k])/sr*1e9 for k in range(len(crossings)-1) ]
        gaps = [g for g in gaps if g < 1000]   # within-burst only
        if gaps:
            print(f'  {len(gaps)} half-periods: min {min(gaps):.0f}ns / median {sorted(gaps)[len(gaps)//2]:.0f}ns / max<1us {max(gaps):.0f}ns')
            narrow = sum(1 for g in gaps if g < 80)
            print(f'  suspicious narrow (<80ns): {narrow}')

for tag, trig in (('burst', True), ('idle', False)):
    v, sr = grab(tag, trig)
    analyze(tag, v, sr)

shot(sc, f'{label}_hunt_view')
sc.cmd(':RUN')
sc.close()
