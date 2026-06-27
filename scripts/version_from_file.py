"""
PlatformIO Pre-Build Script: Firmware Version from VERSION file

Reads the project VERSION file and generates include/FirmwareVersion.generated.h.
"""

Import("env")
from pathlib import Path

project_dir = Path(env["PROJECT_DIR"])
version = (project_dir / "VERSION").read_text(encoding="utf-8").strip()

if not version:
    raise SystemExit("VERSION file is empty")

# The OG (RP2040) firmware shares the V5 version cadence (the x.x.x tail stays in
# lockstep) but uses a major version of 1 instead of 5, so the two firmwares are
# easy to tell apart at a glance (V5 5.7.0.12 -> OG 1.7.0.12). Detect the OG
# build by its PlatformIO env name (jumperless_og / jumperless_og_debug).
pioenv = str(env["PIOENV"])
if pioenv.startswith("jumperless_og"):
    parts = version.split(".")
    parts[0] = "1"
    version = ".".join(parts)
    print(f"OG build: firmware version remapped to {version}")

header_path = project_dir / "include" / "FirmwareVersion.generated.h"
header_path.write_text(
    "// Auto-generated from VERSION at build time. Do not edit.\n"
    "#pragma once\n"
    f'#define FIRMWARE_VERSION "{version}"\n',
    encoding="utf-8",
)
print(f"Firmware version from VERSION: {version}")
