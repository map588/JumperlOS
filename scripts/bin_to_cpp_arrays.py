#!/usr/bin/env python3
"""
Convert all .bin OLED bitmap files in example_images/oled/ to C++ arrays
for embedding in oled.cpp and provisioning at firmware startup.

Reads the 4-byte header (width LE, height LE) from each .bin file, then
outputs:
  - A block of C++ array definitions for oled.cpp
  - Matching extern declarations for oled.h
  - provisionEmbeddedFile() calls for configManager.cpp

Usage:
    python bin_to_cpp_arrays.py [--input-dir DIR] [--output FILE]

    # Default: reads from example_images/oled/, prints to stdout
    python bin_to_cpp_arrays.py

    # Write generated code to a file
    python bin_to_cpp_arrays.py --output generated_bitmaps.cpp

    # Custom input directory
    python bin_to_cpp_arrays.py --input-dir /path/to/bins
"""

import argparse
import os
import struct
import sys
from pathlib import Path


BYTES_PER_LINE = 12  # Match existing format in oled.cpp


def sanitize_name(filename: str) -> str:
    """
    Convert a .bin filename to a valid C identifier.
    e.g. 'eevblog.bin' -> 'eevblog_bin'
         'jogo32h.bin' -> 'jogo32h_bin'
         'my-image.bin' -> 'my_image_bin'
    """
    name = Path(filename).stem  # strip .bin
    # Replace non-alphanumeric chars with underscore
    safe = ""
    for ch in name:
        if ch.isalnum() or ch == '_':
            safe += ch
        else:
            safe += '_'
    # Ensure it doesn't start with a digit
    if safe and safe[0].isdigit():
        safe = "_" + safe
    return safe + "_bin"


def infer_dimensions(data_size: int):
    """
    Infer width x height from raw bitmap byte count.
    Common OLED sizes: 128x32 (512), 128x64 (1024), 64x32 (256).
    Also handles 128x31 (496) which appears in some images.
    """
    known = {
        512:  (128, 32),
        1024: (128, 64),
        256:  (64, 32),
        496:  (128, 31),
    }
    if data_size in known:
        return known[data_size]
    # Try 128-wide first, then 64-wide
    for w in (128, 64):
        h = (data_size * 8) // w
        if (w * h + 7) // 8 == data_size and h > 0:
            return (w, h)
    return None


def has_valid_header(data: bytes) -> bool:
    """
    Heuristic: does the file start with a plausible 4-byte header?
    A valid header has a reasonable width (32-256, multiple of 8)
    and height (1-128), and the remaining body matches w*h/8.
    """
    if len(data) < 5:
        return False
    w, h = struct.unpack_from('<HH', data, 0)
    if w == 0 or h == 0:
        return False
    if w % 8 != 0 or w > 256 or h > 128:
        return False
    expected_body = (w * h + 7) // 8
    return expected_body == len(data) - 4


def read_bin_file(filepath: str):
    """
    Read a .bin OLED bitmap file.
    Handles both headered (4-byte width/height LE prefix) and raw files.
    Returns (width, height, final_bytes) where final_bytes always has header.
    """
    with open(filepath, 'rb') as f:
        data = f.read()

    if len(data) < 4:
        raise ValueError(f"{filepath}: too small ({len(data)} bytes)")

    if has_valid_header(data):
        width, height = struct.unpack_from('<HH', data, 0)
        return width, height, data  # already has header
    else:
        # Raw bitmap — infer dimensions
        dims = infer_dimensions(len(data))
        if dims is None:
            raise ValueError(
                f"{filepath}: {len(data)} bytes, cannot auto-detect dimensions. "
                "Use an image with a standard OLED size or add a 4-byte header.")
        width, height = dims
        print(f"  (headerless file, inferred {width}x{height})")
        # Prepend 4-byte header so the embedded array matches the runtime format
        header = struct.pack('<HH', width, height)
        return width, height, header + data


def format_cpp_array(var_name: str, data: bytes, width: int, height: int,
                     filename: str) -> str:
    """
    Format binary data as a C++ PROGMEM array matching oled.cpp style.
    """
    lines = []
    lines.append(f"// {filename} - {width}x{height} image "
                 f"({len(data)} bytes with header)")
    lines.append(f"const unsigned char {var_name}[] PROGMEM = {{")

    for i in range(0, len(data), BYTES_PER_LINE):
        chunk = data[i:i + BYTES_PER_LINE]
        hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
        # Add trailing comma except potentially on last line
        if i + BYTES_PER_LINE < len(data):
            hex_vals += ","
        lines.append(f"    {hex_vals}")

    lines.append("};")
    lines.append(f"const unsigned int {var_name}_len = {len(data)};")
    return "\n".join(lines)


def generate_all(input_dir: str):
    """
    Process every .bin in input_dir sorted alphabetically.
    Returns (cpp_arrays, extern_decls, provision_calls).
    """
    bin_dir = Path(input_dir)
    if not bin_dir.is_dir():
        print(f"Error: {input_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    bin_files = sorted(bin_dir.glob("*.bin"))
    if not bin_files:
        print(f"No .bin files found in {input_dir}", file=sys.stderr)
        sys.exit(1)

    cpp_blocks = []
    extern_lines = []
    provision_lines = []

    for bf in bin_files:
        fname = bf.name
        var_name = sanitize_name(fname)
        print(f"Processing {fname} -> {var_name}")

        width, height, raw = read_bin_file(str(bf))
        cpp_blocks.append(format_cpp_array(var_name, raw, width, height, fname))
        extern_lines.append(f"extern const unsigned char {var_name}[];")
        extern_lines.append(f"extern const unsigned int {var_name}_len;")
        provision_lines.append(
            f'    provisionEmbeddedFile("images/{fname}", {var_name}, {var_name}_len);'
        )

    cpp_arrays = "\n\n".join(cpp_blocks)
    extern_decls = "\n".join(extern_lines)
    provision_calls = "\n".join(provision_lines)

    return cpp_arrays, extern_decls, provision_calls


def main():
    default_dir = os.path.join(os.path.dirname(__file__),
                               "example_images", "oled")

    parser = argparse.ArgumentParser(
        description="Convert .bin OLED bitmaps to C++ PROGMEM arrays",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
The output has three sections you can paste into the firmware:

  1) C++ array definitions  -> paste into src/oled.cpp (bottom of file)
  2) extern declarations    -> paste into src/oled.h
  3) provisionEmbeddedFile  -> paste into provisionFirmwareFiles() in
                               src/configManager.cpp
        """,
    )
    parser.add_argument("--input-dir", default=default_dir,
                        help=f"Directory with .bin files (default: {default_dir})")
    parser.add_argument("--output", "-o", default=None,
                        help="Write output to file instead of stdout")

    args = parser.parse_args()
    cpp_arrays, extern_decls, provision_calls = generate_all(args.input_dir)

    separator = "=" * 72
    output = f"""
// {separator}
// AUTO-GENERATED by bin_to_cpp_arrays.py
// Source: {args.input_dir}
// {separator}

// ---- Paste into src/oled.cpp (after existing embedded images) ----

{cpp_arrays}

// ---- Paste into src/oled.h (extern declarations) ----

// Embedded binary images for filesystem provisioning (generated)
{extern_decls}

// ---- Paste into provisionFirmwareFiles() in src/configManager.cpp ----

{provision_calls}
"""

    if args.output:
        with open(args.output, 'w') as f:
            f.write(output)
        print(f"\nWritten to {args.output}")
    else:
        print(output)


if __name__ == "__main__":
    main()
