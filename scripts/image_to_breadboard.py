#!/usr/bin/env python3
"""
Convert images to Jumperless breadboard LED format.

The breadboard has 14 rows x 30 columns of RGB LEDs with specific lines skipped.
Format: RGB565 (16-bit color) stored as little-endian uint16

Usage:
    python image_to_breadboard.py input.png output.bin [--preview]
"""

import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
    import struct
except ImportError:
    print("Error: PIL (Pillow) not installed. Install it with:")
    print("  pip install Pillow")
    sys.exit(1)


# Breadboard LED layout
# Lines to skip (1 = skip, 0 = display)
SKIP_LINES = [1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0]
PHYSICAL_ROWS = 21  # Total physical rows
DISPLAY_ROWS = 14   # Rows that actually have LEDs
DISPLAY_COLS = 30   # Columns


def rgb888_to_rgb565(r, g, b):
    """Convert 24-bit RGB to 16-bit RGB565."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5


def image_to_breadboard(image_path, output_path, preview=False):
    """
    Convert image to breadboard LED format.
    
    Args:
        image_path: Path to input image
        output_path: Path to output binary file
        preview: If True, show ASCII preview
    """
    try:
        # Load and resize image
        img = Image.open(image_path)
        print(f"Original image: {img.size} ({img.mode})")
        
        # Resize to 30x14 (actual display dimensions)
        img = img.resize((DISPLAY_COLS, DISPLAY_ROWS), Image.Resampling.LANCZOS)
        img = img.convert('RGB')
        
        pixels = img.load()
        
        # Convert to RGB565 format
        led_data = []
        
        for y in range(DISPLAY_ROWS):
            row_data = []
            for x in range(DISPLAY_COLS):
                r, g, b = pixels[x, y]
                rgb565 = rgb888_to_rgb565(r, g, b)
                row_data.append(rgb565)
            led_data.append(row_data)
        
        # Write binary file
        # Format: array of uint16 (little-endian RGB565)
        with open(output_path, 'wb') as f:
            for row in led_data:
                for pixel in row:
                    f.write(struct.pack('<H', pixel))  # Little-endian uint16
        
        total_pixels = DISPLAY_ROWS * DISPLAY_COLS
        file_size = total_pixels * 2  # 2 bytes per pixel
        
        print(f"\n✓ Converted to {DISPLAY_COLS}x{DISPLAY_ROWS} breadboard format")
        print(f"  Output: {output_path}")
        print(f"  Size: {file_size} bytes ({total_pixels} pixels)")
        
        if preview:
            print("\nASCII Preview (brightness):")
            print("=" * (DISPLAY_COLS + 2))
            for row in led_data:
                line = "|"
                for pixel in row:
                    # Extract RGB components from RGB565
                    r = ((pixel >> 11) & 0x1F) * 255 // 31
                    g = ((pixel >> 5) & 0x3F) * 255 // 63
                    b = (pixel & 0x1F) * 255 // 31
                    brightness = (r + g + b) // 3
                    
                    # ASCII shades
                    if brightness > 200:
                        line += "█"
                    elif brightness > 150:
                        line += "▓"
                    elif brightness > 100:
                        line += "▒"
                    elif brightness > 50:
                        line += "░"
                    else:
                        line += " "
                line += "|"
                print(line)
            print("=" * (DISPLAY_COLS + 2))
        
        return True
        
    except Exception as e:
        print(f"Error: {e}")
        return False


def create_animation(image_paths, output_path, frame_delay_ms=100):
    """
    Create animation from multiple images.
    
    Format:
        Header: 4 bytes
            - uint16: number of frames
            - uint16: delay between frames (milliseconds)
        Frames: Each frame is DISPLAY_ROWS * DISPLAY_COLS * 2 bytes
    """
    frames = []
    
    print(f"Creating animation from {len(image_paths)} frames...")
    
    for i, img_path in enumerate(image_paths):
        print(f"  Processing frame {i+1}/{len(image_paths)}: {img_path}")
        
        img = Image.open(img_path)
        img = img.resize((DISPLAY_COLS, DISPLAY_ROWS), Image.Resampling.LANCZOS)
        img = img.convert('RGB')
        pixels = img.load()
        
        frame_data = []
        for y in range(DISPLAY_ROWS):
            for x in range(DISPLAY_COLS):
                r, g, b = pixels[x, y]
                rgb565 = rgb888_to_rgb565(r, g, b)
                frame_data.append(rgb565)
        
        frames.append(frame_data)
    
    # Write animation file
    with open(output_path, 'wb') as f:
        # Write header
        f.write(struct.pack('<H', len(frames)))  # Number of frames
        f.write(struct.pack('<H', frame_delay_ms))  # Delay in ms
        
        # Write all frames
        for frame in frames:
            for pixel in frame:
                f.write(struct.pack('<H', pixel))
    
    frame_size = DISPLAY_ROWS * DISPLAY_COLS * 2
    total_size = 4 + len(frames) * frame_size
    
    print(f"\n✓ Created animation with {len(frames)} frames")
    print(f"  Frame size: {frame_size} bytes")
    print(f"  Total size: {total_size} bytes")
    print(f"  Frame delay: {frame_delay_ms}ms")
    print(f"  Total duration: {len(frames) * frame_delay_ms / 1000:.1f}s")


def main():
    parser = argparse.ArgumentParser(
        description='Convert images to Jumperless breadboard LED format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert single image
  python image_to_breadboard.py logo.png breadboard_logo.bin --preview
  
  # Create animation from multiple frames
  python image_to_breadboard.py frame*.png animation.bin --animation --delay 100

Breadboard LED Layout:
  - 14 visible rows × 30 columns
  - RGB565 color format (16-bit)
  - Some physical rows are skipped (see source for details)
        """
    )
    
    parser.add_argument('input', nargs='+', help='Input image file(s)')
    parser.add_argument('output', help='Output binary file')
    parser.add_argument('--preview', action='store_true', help='Show ASCII preview')
    parser.add_argument('--animation', action='store_true', 
                       help='Create animation from multiple images')
    parser.add_argument('--delay', type=int, default=100,
                       help='Frame delay in milliseconds (for animations)')
    
    args = parser.parse_args()
    
    # Check input files exist
    for img_path in args.input:
        if not Path(img_path).exists():
            print(f"Error: Input file '{img_path}' not found")
            sys.exit(1)
    
    if args.animation and len(args.input) > 1:
        create_animation(args.input, args.output, args.delay)
    elif len(args.input) == 1:
        success = image_to_breadboard(args.input[0], args.output, args.preview)
        sys.exit(0 if success else 1)
    else:
        print("Error: Either specify one image or use --animation with multiple images")
        sys.exit(1)


if __name__ == '__main__':
    main()

