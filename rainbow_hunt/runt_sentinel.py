"""Runt-trigger sentinel on SCLK: arm, wait for malformed pulse, capture, re-arm.
Usage: python runt_sentinel.py <minutes>"""
import sys, time
sys.path.insert(0, r'C:\development\pelletpirate\rainbow_hunt')
from scope_harness import Scope, shot

minutes = float(sys.argv[1]) if len(sys.argv) > 1 else 5
sc = Scope()

def err():
    e = sc.query(':SYSTem:ERRor?')
    return e

sc.cmd(':CHANnel1:PROBe 10'); sc.cmd(':CHANnel1:SCALe 0.5'); sc.cmd(':CHANnel1:OFFSet -1.5')
sc.cmd(':TIMebase:MAIN:SCALe 0.0000002')   # 200ns/div - zoom on the culprit pulse
sc.cmd(':ACQuire:MDEPth 10k')
sc.cmd(':TRIGger:MODE RUNT'); print('mode:', err())
sc.cmd(':TRIGger:RUNT:SOURce CHANnel1'); print('src:', err())
sc.cmd(':TRIGger:RUNT:POLarity POSitive'); print('pol:', err())
sc.cmd(':TRIGger:RUNT:WHEN NONE'); print('when:', err())
sc.cmd(':TRIGger:RUNT:ALEVel 2.31'); print('alev:', err())
sc.cmd(':TRIGger:RUNT:BLEVel 0.99'); print('blev:', err())
sc.cmd(':SINGle')
print(f'sentinel armed: runt = pulse crossing 0.99V but never reaching 2.31V. Watching {minutes} min...')

t0 = time.time()
catches = 0
while time.time() - t0 < minutes * 60:
    time.sleep(2)
    stat = sc.query(':TRIGger:STATus?')
    if stat == 'STOP':
        catches += 1
        ts = time.time() - t0
        print(f'*** CATCH #{catches} at t=+{ts:.0f}s')
        shot(sc, f'runt_catch_{catches}')
        sc.cmd(':SINGle')      # re-arm
print(f'done: {catches} runt catches in {minutes} min')
sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(':TRIGger:SWEep AUTO'); sc.cmd(':RUN')
sc.close()
