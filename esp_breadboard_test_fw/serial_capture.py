"""Capture serial output from the breadboard for N seconds (default 75).

Opens with DTR/RTS deasserted so it never nudges the chip into download
mode.  --reset does a clean run-mode reset (RTS pulse with DTR low) so
capture starts from boot.  Usage: serial_capture.py [PORT] [SECONDS] [--reset]
"""
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 75.0
do_reset = "--reset" in sys.argv


def open_port():
    s = serial.Serial(baudrate=115200, timeout=1)
    # DTR asserted = "terminal connected" (USB-Serial-JTAG won't TX without it);
    # RTS deasserted so EN is never yanked.  Same steady state as idf.py monitor.
    s.dtr = True
    s.rts = False
    s.port = port
    s.open()
    return s


end = time.time() + seconds
first = True
while time.time() < end:
    try:
        s = open_port()
        try:
            if do_reset and first:
                s.rts = True   # EN low with DTR low: plain reset, BOOT untouched
                time.sleep(0.2)
                s.rts = False  # run
                first = False
            while time.time() < end:
                data = s.read(4096)
                if data:
                    sys.stdout.write(data.decode("utf-8", errors="replace"))
                    sys.stdout.flush()
        finally:
            s.close()
    except (serial.SerialException, PermissionError, OSError) as e:
        print(f"[capture] {e!r}, retrying...", file=sys.stderr)
        time.sleep(0.5)
