#!/usr/bin/env python3
"""
Extract bitmap arrays from C++ source files to binary format.

This script finds bitmap arrays in source code and saves them as binary files.
Useful for converting embedded bitmaps to filesystem-stored images.

Usage:
    python extract_cpp_bitmap.py input.cpp output.bin --array-name jogo32h
"""

import sys
import re
import argparse
from pathlib import Path


def extract_bitmap_from_source(source_file, array_name):
    """
    Extract a specific bitmap array from C++ source file.
    
    Returns:
        tuple: (width, height, bitmap_data) or None if not found
    """
    with open(source_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find the array declaration
    # Match: const unsigned char array_name [] = { ... };
    pattern = rf'const\s+unsigned\s+char\s+{re.escape(array_name)}\s*\[\s*\]\s*(?:PROGMEM\s*)?=\s*\{{([^}}]+)\}}'
    
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        return None
    
    array_content = match.group(1)
    
    # Extract hex values
    hex_values = re.findall(r'0x([0-9A-Fa-f]{2})', array_content)
    
    if not hex_values:
        return None
    
    bitmap_data = [int(v, 16) for v in hex_values]
    
    # Try to find dimensions in comments
    width = None
    height = None
    comment_width = None
    comment_height = None
    
    # Look for comments before the array like: // 'name', 128x32px
    comment_pattern = rf'//.*?(\d+)x(\d+)'
    comment_match = re.search(comment_pattern, content[:match.start()], re.IGNORECASE)
    
    if comment_match:
        comment_width = int(comment_match.group(1))
        comment_height = int(comment_match.group(2))
    
    # Calculate actual dimensions from data size
    data_size = len(bitmap_data)
    if data_size == 512:
        width, height = 128, 32
    elif data_size == 1024:
        width, height = 128, 64
    elif data_size == 256:
        width, height = 64, 32
    elif data_size == 496:
        width, height = 128, 31
    else:
        # Try to infer from comment if available
        if comment_width and comment_height:
            expected = (comment_width * comment_height + 7) // 8
            if expected == data_size:
                width, height = comment_width, comment_height
    
    # Warn if comment dimensions don't match actual data
    if comment_width and comment_height and width and height:
        if comment_width != width or comment_height != height:
            print(f"⚠ Warning: Comment says {comment_width}x{comment_height}, but data is {data_size} bytes")
            print(f"  Actual dimensions: {width}x{height}")
    
    return (width, height, bitmap_data)


def save_bitmap(output_file, width, height, bitmap_data, add_header=True):
    """Save bitmap to binary file with optional header."""
    with open(output_file, 'wb') as f:
        if add_header and width and height:
            # Write 4-byte header (width, height as 16-bit little-endian)
            f.write(width.to_bytes(2, byteorder='little'))
            f.write(height.to_bytes(2, byteorder='little'))
            print(f"Added header: {width}x{height}")
        
        f.write(bytes(bitmap_data))
    
    print(f"Saved {len(bitmap_data)} bytes to {output_file}")
    if width and height:
        expected = (width * height + 7) // 8
        if len(bitmap_data) == expected:
            print(f"✓ Size matches {width}x{height} dimensions")


def main():
    parser = argparse.ArgumentParser(
        description='Extract bitmap arrays from C++ source to binary files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Extract jogo32h from oled.cpp
  python extract_cpp_bitmap.py ../src/oled.cpp jogo32h.bin --array-name jogo32h
  
  # Extract without header (raw binary)
  python extract_cpp_bitmap.py ../src/oled.cpp jogo32h.bin --array-name jogo32h --no-header
        """
    )
    
    parser.add_argument('input', help='Input C++ source file')
    parser.add_argument('output', help='Output binary file')
    parser.add_argument('--array-name', required=True, help='Name of the bitmap array to extract')
    parser.add_argument('--no-header', action='store_true', help='Do not add dimension header')
    
    args = parser.parse_args()
    
    if not Path(args.input).exists():
        print(f"Error: Input file '{args.input}' not found")
        sys.exit(1)
    
    print(f"Searching for array '{args.array_name}' in {args.input}...")
    
    result = extract_bitmap_from_source(args.input, args.array_name)
    
    if not result:
        print(f"Error: Could not find array '{args.array_name}' in source file")
        sys.exit(1)
    
    width, height, bitmap_data = result
    
    if width and height:
        print(f"Found bitmap: {width}x{height}, {len(bitmap_data)} bytes")
    else:
        print(f"Found bitmap: {len(bitmap_data)} bytes (dimensions unknown)")
    
    save_bitmap(args.output, width, height, bitmap_data, add_header=not args.no_header)
    
    print(f"\n✓ Success! Bitmap extracted to {args.output}")


if __name__ == '__main__':
    main()

