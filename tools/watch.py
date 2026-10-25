#!/usr/bin/env python3
"""Talk to the Artemis dev console over USB.

  watch.py [-p PORT] cmd <command...>     run a console command, print its output
  watch.py [-p PORT] shot <out.png>       save a screenshot (scaled 3x)
  watch.py [-p PORT] monitor [seconds]    print the log output

Needs pyserial (bundled with ESP-IDF's python env) and Pillow for screenshots.
"""
import argparse
import base64
import glob
import sys
import time

import serial


def find_port():
    ports = glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/ttyACM*')
    if not ports:
        sys.exit('no watch found on USB')
    return ports[0]


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.1
    s.dtr = True  # pyserial applies DTR before RTS on open; DTR=0 with RTS=1 resets the chip
    s.rts = False
    s.open()
    return s


def run(s, command, timeout=15.0):
    s.reset_input_buffer()
    s.write(b'\n')  # first byte also makes the watch keep USB alive (no light sleep)
    time.sleep(0.05)
    s.write(command.encode() + b'\n')
    out = b''
    end = time.time() + timeout
    while time.time() < end:
        out += s.read(4096)
        lines = out.decode('utf-8', 'replace').splitlines()
        if any(l.strip() == 'OK' or l.startswith('ERR') for l in lines):
            break
    return out.decode('utf-8', 'replace')


def screenshot(s, path, scale=3):
    from PIL import Image
    run(s, 'wake')  # the UI thread is parked while the screen is off
    text = run(s, 'shot', timeout=20)
    lines = text.splitlines()
    try:
        start = next(i for i, l in enumerate(lines) if l.startswith('SHOT '))
        end = next(i for i, l in enumerate(lines) if l.strip() == 'ENDSHOT')
    except StopIteration:
        sys.exit('no screenshot in output:\n' + text[-500:])
    _, w, h = lines[start].split()
    w, h = int(w), int(h)
    data = base64.b64decode(''.join(lines[start + 1:end]))
    img = Image.new('RGB', (w, h))
    px = img.load()
    for i in range(w * h):
        v = (data[2 * i] << 8) | data[2 * i + 1]  # LV_COLOR_16_SWAP: big-endian RGB565
        r, g, b = (v >> 11) & 0x1f, (v >> 5) & 0x3f, v & 0x1f
        px[i % w, i // w] = (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)
    img.resize((w * scale, h * scale), Image.NEAREST).save(path)
    print(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-p', '--port')
    ap.add_argument('action', choices=['cmd', 'shot', 'monitor'])
    ap.add_argument('rest', nargs='*')
    a = ap.parse_args()
    s = open_port(a.port or find_port())
    if a.action == 'cmd':
        print(run(s, ' '.join(a.rest)), end='')
    elif a.action == 'shot':
        screenshot(s, a.rest[0] if a.rest else 'shot.png')
    else:
        end = time.time() + float(a.rest[0] if a.rest else 30)
        while time.time() < end:
            b = s.read(4096)
            if b:
                sys.stdout.write(b.decode('utf-8', 'replace'))
                sys.stdout.flush()


if __name__ == '__main__':
    main()
