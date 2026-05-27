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

header_path = project_dir / "include" / "FirmwareVersion.generated.h"
header_path.write_text(
    "// Auto-generated from VERSION at build time. Do not edit.\n"
    "#pragma once\n"
    f'#define FIRMWARE_VERSION "{version}"\n',
    encoding="utf-8",
)
print(f"Firmware version from VERSION: {version}")
