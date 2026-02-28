"""Simple Jumperless port detection helper

This standalone script replicates the core of the port-detection logic from
`JumperlessWokwiBridge.py` in a much smaller, self-contained form.  It can be
invoked by an LLM or user to enumerate all serial ports, flag which ones look
like Jumperless devices, read their USB interface descriptor (when available),
and compute the CDC index if the HWID contains an ``MI_`` field.  The results
are printed to stdout and also saved to ``jumperless_ports.json`` for the
remainder of the session.

By running this script in your local environment (which has access to the USB
serial ports), the LLM can then read back the JSON file to know which physical
port names correspond to which functions (main, python_repl, passthrough,
etc.).

Example usage::

    python jumperless_skill/port_identify.py

"""

import json
import re
import sys

import serial
import serial.tools.list_ports


# ---------------------------------------------------------------------------
# helper functions (trimmed down from WokwiBridge)
# ---------------------------------------------------------------------------

def parse_hardware_id(hwid, desc='unknown'):
    """Parse a pyserial hwid string into (vid, pid).

    The format differs by platform; examples:
      "USB VID:PID=2E8A:ACAB SER=012345"  (macOS/Linux)
      "USB VID:PID=2E8A&PID=ACAB&MI_00"   (Windows)
    Returns (vid_hex, pid_hex) as strings or (None, None) if not found.
    """
    if not hwid:
        return None, None
    # try common patterns
    m = re.search(r"VID[:=]([0-9A-Fa-f]{4})", hwid)
    if m:
        vid = m.group(1)
    else:
        vid = None
    m = re.search(r"PID[:=]([0-9A-Fa-f]{4})", hwid)
    if m:
        pid = m.group(1)
    else:
        pid = None
    return vid, pid


def is_jumperless_device(desc, pid, interface=None):
    """Rough heuristic to recognise a Jumperless port."""
    if desc and "jumperless" in desc.lower():
        return True
    if pid in ["ACAB", "1312"]:
        return True
    return False


def get_usb_interface_string(port_obj):
    """Return the USB interface string for a pyserial port object.

    On Linux/macOS pyserial exposes ``port_obj.interface`` if the kernel
    provides it.  On Windows it is not available; this helper returns ``None``
    there and the caller can decide to ignore it.
    """
    return getattr(port_obj, 'interface', None)


def cdc_index_from_hwid(hwid):
    """If the hwid contains ``MI_XX`` (Windows), return the CDC index.

    Each CDC function uses two USB interfaces; the index = MI_ number // 2.
    Returns ``None`` if no MI_ token is found.
    """
    if not hwid:
        return None
    hwid_upper = hwid.upper()
    if 'MI_' in hwid_upper:
        try:
            mi_pos = hwid_upper.find('MI_')
            mi_num = int(hwid_upper[mi_pos + 3:mi_pos + 5])
            return mi_num // 2
        except (ValueError, IndexError):
            pass
    return None


# ---------------------------------------------------------------------------
# main script logic
# ---------------------------------------------------------------------------

def _query_enq(port_name, timeout=1.0):
    """Send ENQ to a port and return any textual response line."""
    try:
        with serial.Serial(port_name, 115200, timeout=timeout) as ser:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.05)
            ser.write(b"\x05\n")  # ENQ
            ser.flush()
            time.sleep(0.2)
            buf = b''
            start = time.time()
            while time.time() - start < timeout:
                if ser.in_waiting > 0:
                    buf += ser.read(ser.in_waiting)
                    time.sleep(0.01)
                else:
                    time.sleep(0.01)
            return buf.decode('utf-8', errors='ignore').strip()
    except Exception:
        return ''

def _query_firmware(port_name, timeout=1.0):
    """Send '?' to port to see if it's the main firmware port."""
    try:
        with serial.Serial(port_name, 115200, timeout=timeout) as ser:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.05)
            ser.write(b'?')
            ser.flush()
            time.sleep(0.5)
            buf = b''
            start = time.time()
            while time.time() - start < timeout:
                if ser.in_waiting > 0:
                    buf += ser.read(ser.in_waiting)
                    time.sleep(0.01)
                else:
                    time.sleep(0.01)
            return buf.decode('utf-8', errors='ignore').strip()
    except Exception:
        return ''


def gather_ports():
    """Enumerate serial ports and build a mapping dictionary."""
    # first, list all ports so we can identify the main port
    ports = list(serial.tools.list_ports.comports())
    main_port = None
    print(f"Found {len(ports)} serial ports:")
    print(ports)
    for p in ports:
        dev = getattr(p, 'device', None)
        if not dev:
            continue
        resp = _query_firmware(dev)
        if resp and "Jumperless firmware" in resp:
            main_port = dev
            break

    result = {}
    for p in ports:
        device = getattr(p, 'device', None)
        desc = getattr(p, 'description', None)
        hwid = getattr(p, 'hwid', None)
        interface = getattr(p, 'interface', None)

        vid, pid = parse_hardware_id(hwid, desc)
        is_jl = is_jumperless_device(desc, pid, interface)
        usb_func = get_usb_interface_string(p) or ''
        cdc_idx = cdc_index_from_hwid(hwid)
        enq_resp = ''

        if is_jl:
            if device == main_port:
                # query the main port for CDC function listings
                enq_resp = _query_enq(device)
                for line in enq_resp.splitlines():
                    if line.strip().startswith('CDC') and ':' in line:
                        usb_func = line.split(':',1)[1].strip()
                        break
            elif not usb_func and not main_port:
                # if we haven't found main, still try ENQ hope it'll help
                enq_resp = _query_enq(device)

        result[device] = {
            'description': desc,
            'hwid': hwid,
            'interface': interface,
            'vid': vid,
            'pid': pid,
            'is_jumperless': bool(is_jl),
            'usb_function': usb_func,
            'cdc_index': cdc_idx,
            'enq_response': enq_resp,
        }
    return result


def main():
    mapping = gather_ports()
    print(json.dumps(mapping, indent=2))
    # choose path relative to this script's location
    try:
        import os
        script_dir = os.path.dirname(os.path.abspath(__file__))
        out_path = os.path.join(script_dir, 'jumperless_ports.json')
        with open(out_path, 'w') as f:
            json.dump(mapping, f, indent=2)
        print(f"Saved mapping to {out_path}")
    except Exception as e:
        print(f"Could not save JSON file: {e}")
    return mapping


if __name__ == '__main__':
    main()
