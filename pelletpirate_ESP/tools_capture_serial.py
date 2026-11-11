import serial, time, sys

port, seconds, outfile = sys.argv[1], float(sys.argv[2]), sys.argv[3]
s = serial.Serial(port, 115200, timeout=1)
end = time.time() + seconds
with open(outfile, "wb") as f:
    while time.time() < end:
        data = s.read(4096)
        if data:
            f.write(data)
s.close()
print("capture done")
