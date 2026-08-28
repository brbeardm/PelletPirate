"""PelletPirate rainbow-hunt scope harness — Rigol DHO814 @ 192.168.1.188 (raw SCPI, port 5555).
Usage: python scope_harness.py <command> [args]
Commands:
  idn                        identify scope
  envelope <ch>              preset: AC coupling, 50mV/div, 2ms/div (120Hz ripple view)
  hash <ch>                  preset: AC coupling, 20mV/div, 10us/div (switching hash view)
  burst <ch>                 preset: normal trigger on hash bursts, 100us/div
  correlate                  preset: CH1+CH2+CH3 (12V/3.3V/LED+), common trigger CH2
  shot <label>               screenshot -> captures/<label>.png
  wave <ch> <label>          waveform CSV -> captures/<label>.csv (+ Vpp/Vrms/freq printed)
  meas <ch>                  print Vpp / Vrms / Vavg / frequency for channel
  raw "<scpi>"               send arbitrary command (query if ends with ?)
"""
import socket, sys, os, time, struct

IP, PORT = '192.168.1.188', 5555
CAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'captures')
os.makedirs(CAP, exist_ok=True)

class Scope:
    def __init__(self, ip=IP, port=PORT):
        self.s = socket.create_connection((ip, port), timeout=10)
        self.s.settimeout(15)
    def cmd(self, c):
        self.s.sendall((c + '\n').encode())
        time.sleep(0.05)
    def query(self, c):
        self.s.sendall((c + '\n').encode())
        buf = b''
        while not buf.endswith(b'\n'):
            chunk = self.s.recv(65536)
            if not chunk: break
            buf += chunk
        return buf.decode(errors='replace').strip()
    def query_block(self, c):
        """Read an IEEE-488.2 definite-length block (#Nlllll<data>)."""
        self.s.sendall((c + '\n').encode())
        head = b''
        while len(head) < 2:
            head += self.s.recv(2 - len(head))
        assert head[0:1] == b'#', f'not a block: {head!r}'
        ndig = int(head[1:2])
        lenb = b''
        while len(lenb) < ndig:
            lenb += self.s.recv(ndig - len(lenb))
        total = int(lenb)
        data = b''
        while len(data) < total:
            chunk = self.s.recv(min(65536, total - len(data)))
            if not chunk: break
            data += chunk
        # trailing newline
        try:
            self.s.settimeout(2); self.s.recv(16)
        except Exception:
            pass
        self.s.settimeout(15)
        return data
    def close(self):
        try: self.s.close()
        except Exception: pass

def setup_common(sc, ch, scale, timebase):
    sc.cmd(f':CHANnel{ch}:DISPlay ON')
    sc.cmd(f':CHANnel{ch}:COUPling AC')
    sc.cmd(f':CHANnel{ch}:BWLimit 20M')      # 20MHz BW limit OFF for hash: caller overrides
    sc.cmd(f':CHANnel{ch}:PROBe 1')          # assume probe switch at 1x for rail work
    sc.cmd(f':CHANnel{ch}:SCALe {scale}')
    sc.cmd(f':CHANnel{ch}:OFFSet 0')
    sc.cmd(f':TIMebase:MAIN:SCALe {timebase}')
    sc.cmd(':RUN')

def preset_envelope(sc, ch):
    setup_common(sc, ch, 0.05, 0.002)        # 50mV/div, 2ms/div
    sc.cmd(f':CHANnel{ch}:BWLimit 20M')      # limit ON: see the 120Hz envelope w/o hash
    sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(f':TRIGger:EDGE:SOURce CHANnel{ch}')
    sc.cmd(':TRIGger:SWEep AUTO')
    print(f'envelope preset on CH{ch}: AC, 50mV/div, 2ms/div, BW-limit 20MHz, auto trigger')

def preset_hash(sc, ch):
    setup_common(sc, ch, 0.02, 0.00001)      # 20mV/div, 10us/div
    sc.cmd(f':CHANnel{ch}:BWLimit OFF')      # full bandwidth: this is the hash view
    sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(f':TRIGger:EDGE:SOURce CHANnel{ch}')
    sc.cmd(':TRIGger:SWEep AUTO')
    print(f'hash preset on CH{ch}: AC, 20mV/div, 10us/div, full BW, auto trigger')

def preset_burst(sc, ch):
    setup_common(sc, ch, 0.02, 0.0001)       # 20mV/div, 100us/div
    sc.cmd(f':CHANnel{ch}:BWLimit OFF')
    sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(f':TRIGger:EDGE:SOURce CHANnel{ch}')
    sc.cmd(':TRIGger:SWEep NORMal')
    sc.cmd(f':TRIGger:EDGE:LEVel 0.03')      # 30mV: catches hash bursts; adjust live
    print(f'burst preset on CH{ch}: normal trigger @30mV — bursts at 120Hz spacing = conducted PS1 envelope')

def preset_correlate(sc):
    for ch, scale in ((1, 0.05), (2, 0.02), (3, 0.05)):
        setup_common(sc, ch, scale, 0.002)
        sc.cmd(f':CHANnel{ch}:BWLimit OFF')
    sc.cmd(':TRIGger:MODE EDGE'); sc.cmd(':TRIGger:EDGE:SOURce CHANnel2')
    sc.cmd(':TRIGger:SWEep AUTO')
    print('correlate preset: CH1=12V(J7.1) CH2=3.3V(J7.2) CH3=LED+(J7.3), trigger CH2, 2ms/div')

def shot(sc, label):
    data = sc.query_block(':DISPlay:DATA? PNG')
    p = os.path.join(CAP, f'{label}.png')
    open(p, 'wb').write(data)
    print(f'saved {p} ({len(data)} bytes)')

def wave(sc, ch, label):
    sc.cmd(':STOP')
    sc.cmd(f':WAVeform:SOURce CHANnel{ch}')
    sc.cmd(':WAVeform:MODE NORMal')
    sc.cmd(':WAVeform:FORMat BYTE')
    pre = sc.query(':WAVeform:PREamble?').split(',')
    xinc, xorig = float(pre[4]), float(pre[5])
    yinc, yorig, yref = float(pre[7]), float(pre[8]), float(pre[9])
    raw = sc.query_block(':WAVeform:DATA?')
    p = os.path.join(CAP, f'{label}.csv')
    with open(p, 'w') as f:
        f.write('t_s,volts\n')
        for i, b in enumerate(raw):
            f.write(f'{xorig + i*xinc:.9g},{(b - yorig - yref) * yinc:.6g}\n')
    volts = [(b - yorig - yref) * yinc for b in raw]
    print(f'saved {p} ({len(raw)} pts) | Vpp={max(volts)-min(volts)*1:.4f} check-vs-scope-meas below')
    sc.cmd(':RUN')

def meas(sc, ch):
    for item in ('VPP', 'VRMS', 'VAVG', 'FREQuency'):
        v = sc.query(f':MEASure:ITEM? {item},CHANnel{ch}')
        print(f'  CH{ch} {item}: {v}')

if __name__ == '__main__':
    a = sys.argv[1:]
    sc = Scope()
    try:
        if not a or a[0] == 'idn':
            print(sc.query('*IDN?'))
        elif a[0] == 'envelope': preset_envelope(sc, int(a[1]))
        elif a[0] == 'hash': preset_hash(sc, int(a[1]))
        elif a[0] == 'burst': preset_burst(sc, int(a[1]))
        elif a[0] == 'correlate': preset_correlate(sc)
        elif a[0] == 'shot': shot(sc, a[1])
        elif a[0] == 'wave': wave(sc, int(a[1]), a[2])
        elif a[0] == 'meas': meas(sc, int(a[1]))
        elif a[0] == 'raw':
            c = a[1]
            print(sc.query(c) if c.endswith('?') else sc.cmd(c) or 'sent')
        else:
            print(__doc__)
    finally:
        sc.close()
