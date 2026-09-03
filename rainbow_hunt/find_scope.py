"""Sweep the /24 for SCPI port 5555 and *IDN? every responder."""
import socket, concurrent.futures

SUBNET = '192.168.1.'

def probe(ip):
    try:
        s = socket.create_connection((ip, 5555), timeout=0.4)
    except Exception:
        return None
    try:
        s.settimeout(3)
        s.sendall(b'*IDN?\n')
        idn = s.recv(4096).decode(errors='replace').strip()
    except Exception:
        idn = '(port open, no IDN reply)'
    finally:
        s.close()
    return ip, idn

with concurrent.futures.ThreadPoolExecutor(max_workers=64) as ex:
    for r in ex.map(probe, (SUBNET + str(h) for h in range(1, 255))):
        if r:
            print(f'{r[0]:<16} {r[1]}')
