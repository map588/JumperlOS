#
# Pre-upload helper for [env:jumperless_og] -- pin the 1200bps reset to CDC0.
#
# The OG can be flashed over USB whether it is running JumperlOS
# (USB 1D50:ACAB) or the ORIGINAL factory firmware (USB ACAB:1312). Both reboot
# into BOOTSEL on a 1200bps + DTR "touch" -- BUT the factory firmware only honors
# that touch on its FIRST CDC interface (CDC0, e.g. /dev/cu.usbmodem*1). A touch
# on a later CDC (usbmodem*3) is ignored (verified on hardware: usbmodem01
# rebooted to BOOTSEL, usbmodem03 did not). The current JumperlOS core resets on
# ANY CDC, so CDC0 is the one port that works for BOTH firmwares.
#
# PlatformIO's stock autodetect (boards/jumperless_og.json "hwids") finds the
# board by VID:PID but resolves a multi-CDC device to its LAST port
# (SerialPortFinder._reveal_device_port -> candidates[-1]) = CDC1, which never
# triggers the factory reset. So here we override UPLOAD_PORT with the board's
# CDC0 (the lowest-named /dev node carrying a Jumperless VID:PID) before the
# platform's BeforeUpload runs its touch.
#
# If no such serial port exists (board already in BOOTSEL -> a mass-storage
# device, or nothing plugged in), we leave UPLOAD_PORT untouched so the platform
# picotool BOOTSEL fastpath / hwids autodetect still apply.
#
# ponytail: matches CDC0 by "lowest /dev node" (control interface 0 sorts first)
# rather than parsing USB interface descriptors -- correct for the standard
# composite layout; revisit only if a firmware reorders its CDC interfaces.

Import("env")  # noqa: F821 (injected by PlatformIO/SCons)

try:
    import serial.tools.list_ports as list_ports
except ImportError:
    list_ports = None

# (VID, PID) of every firmware that can run on the OG. Keep in sync with
# boards/jumperless_og.json "hwids".
JUMPERLESS_USB_IDS = {
    (0x1D50, 0xACAB),  # JumperlOS
    (0xACAB, 0x1312),  # original / factory Jumperless firmware
}


def _select_cdc0_port():
    if list_ports is None:
        return None
    ports = [
        p for p in list_ports.comports()
        if p.vid is not None and (p.vid, p.pid) in JUMPERLESS_USB_IDS
    ]
    if not ports:
        return None
    # CDC0's control interface is interface 0, so its /dev node sorts first.
    ports.sort(key=lambda p: p.device)
    return ports[0].device


_cdc0 = _select_cdc0_port()
if _cdc0:
    print("jumperless_og: pinning UPLOAD_PORT to CDC0 %s for 1200bps reset" % _cdc0)
    env.Replace(UPLOAD_PORT=_cdc0)
