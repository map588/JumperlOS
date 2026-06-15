#!/usr/bin/env python3
"""Host harness for the Jumperless USBSer3 machine backchannel.

Drives the on-device ``:bench`` command (the authoritative timing source) and
tabulates how long each read-only backchannel command takes to run. Can also
fire arbitrary backchannel verbs and time the host-side round trip.

The USBSer3 "Debug" CDC port is the 4th serial interface the Jumperless
exposes. Pass it with ``--port`` (e.g. ``/dev/cu.usbmodem...`` on macOS,
``COMx`` on Windows, ``/dev/ttyACMx`` on Linux). If omitted, the script lists
the candidate ports it can see.

Requires pyserial:  ``pip install pyserial``

Examples
--------
    # List serial ports
    python3 scripts/bench_backchannel.py

    # Run the on-device benchmark and print the table
    python3 scripts/bench_backchannel.py --port /dev/cu.usbmodem3

    # Time a single verb end-to-end from the host
    python3 scripts/bench_backchannel.py --port COM7 --cmd ":gpio:s" --reps 200
"""

import argparse
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")


def list_serial_ports():
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for p in ports:
        print(f"  {p.device:24s} {p.description}")
    print("\nThe USBSer3 'Debug' port is usually the highest-numbered "
          "Jumperless CDC interface. Re-run with --port <device>.")


def read_until(ser, sentinel, timeout=5.0):
    """Read lines until one equals ``sentinel`` (or timeout). Returns the list
    of lines seen (excluding the sentinel)."""
    lines = []
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").strip("\r")
                if line == sentinel:
                    return lines
                if line:
                    lines.append(line)
    return lines


def parse_bench_line(line):
    """Parse 'bench{verb:gpio:s,min_us:12,avg_us:15,max_us:40,bytes:55}'."""
    if not line.startswith("bench{") or not line.endswith("}"):
        return None
    body = line[len("bench{"):-1]
    fields = {}
    # verb may itself contain ':' (e.g. gpio:s), so split on commas first,
    # then split each token on the FIRST ':'.
    for tok in body.split(","):
        if ":" not in tok:
            continue
        key, val = tok.split(":", 1)
        fields[key] = val
    return fields


def run_bench(ser, which=None):
    cmd = ":bench" if not which else f":bench:{which}"
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    ser.flush()

    # Wait for bench_start, then collect bench{...} lines until bench_end.
    read_until(ser, "bench_start", timeout=10.0)
    lines = read_until(ser, "bench_end", timeout=30.0)

    rows = [parse_bench_line(l) for l in lines]
    rows = [r for r in rows if r]
    if not rows:
        print("No bench{...} lines received. Wrong port, or firmware without "
              "the :bench command?")
        for l in lines:
            print("  <", l)
        return

    print(f"{'verb':<12}{'min_us':>9}{'avg_us':>9}{'max_us':>9}{'bytes':>8}")
    print("-" * 47)
    for r in rows:
        print(f"{r.get('verb',''):<12}"
              f"{r.get('min_us',''):>9}"
              f"{r.get('avg_us',''):>9}"
              f"{r.get('max_us',''):>9}"
              f"{r.get('bytes',''):>8}")


def time_cmd(ser, cmd, reps):
    """Time host-side round trips of a single command/verb."""
    durations = []
    for _ in range(reps):
        ser.reset_input_buffer()
        t0 = time.perf_counter()
        ser.write((cmd + "\n").encode())
        ser.flush()
        # Read one response chunk (best-effort; most replies are one line).
        deadline = time.time() + 1.0
        got = b""
        while time.time() < deadline:
            chunk = ser.read(512)
            if chunk:
                got += chunk
                if b"\n" in got or b"}" in got:
                    break
        durations.append((time.perf_counter() - t0) * 1e6)
    durations.sort()
    n = len(durations)
    print(f"cmd {cmd!r}  reps={n}  "
          f"min={durations[0]:.1f}us  "
          f"median={durations[n // 2]:.1f}us  "
          f"max={durations[-1]:.1f}us  (host round-trip, includes USB latency)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="USBSer3 (Debug) serial port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--cmd", help="Time a single verb instead of running :bench")
    ap.add_argument("--reps", type=int, default=100,
                    help="Repetitions for --cmd timing (default 100)")
    ap.add_argument("--only", help="Bench only this verb (e.g. gpio:s)")
    args = ap.parse_args()

    if not args.port:
        list_serial_ports()
        return

    with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
        time.sleep(0.2)  # let the CDC port settle
        if args.cmd:
            time_cmd(ser, args.cmd, args.reps)
        else:
            run_bench(ser, args.only)


if __name__ == "__main__":
    main()
