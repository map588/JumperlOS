#!/usr/bin/env python3
"""
Generate DEFLATE-compressed startup frames from src/Images.h.

Outputs: src/Images_deflate.h

Format:
- Each frame becomes a DeflateBlob { data pointer, length, window_bits }.
- Compressed stream is raw DEFLATE (no zlib/gzip wrapper).
- Uncompressed payload is the frame's uint32_t array packed little-endian.
"""

from __future__ import annotations

import argparse
import re
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple


@dataclass(frozen=True)
class Frame:
    index: int
    pixels_u32: List[int]


FRAME_RE = re.compile(
    r"const\s+uint32_t\s+__in_flash\(\)\s+startupFrame(\d+)\s*\[\]\s*(?:PROGMEM\s*)?=\s*\{(?P<body>.*?)\};",
    re.DOTALL,
)

HEX_RE = re.compile(r"0x[0-9a-fA-F]+")


def parse_frames(images_h_text: str) -> List[Frame]:
    frames: List[Frame] = []
    for m in FRAME_RE.finditer(images_h_text):
        idx = int(m.group(1))
        body = m.group("body")
        vals = [int(h, 16) for h in HEX_RE.findall(body)]
        frames.append(Frame(index=idx, pixels_u32=vals))
    frames.sort(key=lambda f: f.index)
    return frames


def pack_u32_le(values: List[int]) -> bytes:
    out = bytearray()
    for v in values:
        out += struct.pack("<I", v & 0xFFFFFFFF)
    return bytes(out)


def compress_raw_deflate(data: bytes, level: int, wbits: int) -> bytes:
    # For raw DEFLATE, wbits must be negative. Magnitude selects LZ77 window bits.
    comp = zlib.compressobj(level=level, wbits=-wbits)
    return comp.compress(data) + comp.flush()


def format_c_array(name: str, blob: bytes, cols: int = 16) -> str:
    parts = [f"0x{b:02x}" for b in blob]
    lines = []
    for i in range(0, len(parts), cols):
        lines.append("  " + ", ".join(parts[i : i + cols]))
    inner = ",\n".join(lines)
    return f"static const uint8_t {name}[] PROGMEM = {{\n{inner}\n}};\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--images-h", default="src/Images.h")
    ap.add_argument("--out", default="src/Images_deflate.h")
    ap.add_argument("--level", type=int, default=9)
    ap.add_argument("--wbits", type=int, default=12, help="DEFLATE window bits (8-15)")
    ap.add_argument("--expected-u32-len", type=int, default=32 * 21, help="Expected uint32_t count per frame")
    args = ap.parse_args()

    if not (8 <= args.wbits <= 15):
        raise SystemExit("--wbits must be 8..15")

    images_h = Path(args.images_h)
    out_h = Path(args.out)

    text = images_h.read_text(encoding="utf-8", errors="replace")
    frames = parse_frames(text)
    if not frames:
        raise SystemExit(f"No startupFrameN arrays found in {images_h}")

    # Only include frames 0..44 in order (what Graphics.cpp expects).
    by_idx = {f.index: f for f in frames}
    missing = [i for i in range(45) if i not in by_idx]
    if missing:
        raise SystemExit(f"Missing frames: {missing}")

    # Build header content.
    out_lines: List[str] = []
    out_lines.append("#pragma once\n")
    out_lines.append("#include <Arduino.h>\n")
    out_lines.append("#include <stdint.h>\n")
    out_lines.append("\n")
    out_lines.append("typedef struct {\n")
    out_lines.append("  const uint8_t *data;\n")
    out_lines.append("  uint16_t len;\n")
    out_lines.append("  uint8_t wbits;\n")
    out_lines.append("} DeflateBlob;\n")
    out_lines.append("\n")
    out_lines.append(f"static const uint16_t STARTUP_FRAME_U32_LEN = {args.expected_u32_len};\n")
    out_lines.append(f"static const uint16_t STARTUP_FRAME_RAW_BYTES = {args.expected_u32_len} * 4;\n")
    out_lines.append("\n")

    blob_names: List[str] = []
    for i in range(45):
        f = by_idx[i]
        if len(f.pixels_u32) != args.expected_u32_len:
            raise SystemExit(
                f"startupFrame{i} length is {len(f.pixels_u32)} u32, expected {args.expected_u32_len}"
            )
        raw = pack_u32_le(f.pixels_u32)
        comp = compress_raw_deflate(raw, level=args.level, wbits=args.wbits)
        cname = f"startupFrame{i}_deflate"
        blob_names.append(cname)
        out_lines.append(format_c_array(cname, comp))
        out_lines.append("\n")

    out_lines.append("static const DeflateBlob startupFrameDeflateBlobs[45] PROGMEM = {\n")
    for i, cname in enumerate(blob_names):
        out_lines.append(f"  {{ {cname}, (uint16_t)sizeof({cname}), (uint8_t){args.wbits} }},\n")
    out_lines.append("};\n")
    out_lines.append("\n")

    out_h.write_text("".join(out_lines), encoding="utf-8")
    print(f"Wrote {out_h} ({out_h.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

