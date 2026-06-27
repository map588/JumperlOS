#!/usr/bin/env python3
"""Reproduce the crash: drive the status menu over serial, capture output."""
import sys
import time

import serial

PORT = "/dev/cu.usbmodemJLV5port1"


def read_all(ser, duration=1.0):
    out = b""
    end = time.time() + duration
    while time.time() < end:
        try:
            chunk = ser.read(4096)
        except (serial.SerialException, OSError) as e:
            out += f"\n<<PORT ERROR: {e}>>\n".encode()
            return out, True
        if chunk:
            out += chunk
            end = time.time() + 0.3  # extend while data flows
    return out, False


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "scroll"
    ser = serial.Serial(PORT, 115200, timeout=0.1)
    time.sleep(0.3)
    ser.reset_input_buffer()

    log = open("/tmp/menufx_repro.log", "wb")

    def step(name, data, wait=1.0):
        print(f"--- {name} ---", flush=True)
        log.write(f"\n--- {name} ---\n".encode())
        if data:
            try:
                ser.write(data)
            except (serial.SerialException, OSError) as e:
                print(f"WRITE FAILED: {e}")
                return True
        out, dead = read_all(ser, wait)
        log.write(out)
        log.flush()
        sys.stdout.write(out.decode(errors="replace"))
        sys.stdout.flush()
        return dead

    if step("open + flush", b"", 0.8):
        return
    if step("send D (status menu)", b"D", 1.5):
        return
    if step("send Enter", b"\r", 1.5):
        return

    if mode == "scroll":
        # just scroll up and down many times, never run anything
        for i in range(40):
            key = b"j" if (i // 10) % 2 == 0 else b"k"
            if step(f"scroll {i+1} ({key.decode()})", key, 0.3):
                return
    elif mode == "idle":
        # open the menu and just sit there
        for i in range(30):
            if step(f"idle {i+1}s", b"", 1.0):
                return
    elif mode == "menufx":
        for i in range(13):
            if step(f"down {i+1}", b"j", 0.25):
                return
        if step("ENTER Menu FX", b"\r", 3.0):
            return
        # exercise the tuner: nav, descend, ascend, cycle type, replay
        for key, name in [(b"j", "nav down"), (b"j", "nav down"), (b"\r", "descend"),
                          (b"j", "nav down"), (b"h", "ascend"), (b"t", "cycle type"),
                          (b"r", "replay"), (b"k", "nav up")]:
            if step(f"tuner: {name}", key, 0.8):
                return
        if step("quit tuner", b"q", 2.0):
            return
        if step("quit status menu", b"q", 1.5):
            return
    print("\n=== survived ===")


if __name__ == "__main__":
    main()
