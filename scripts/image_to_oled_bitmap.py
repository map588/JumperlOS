#!/usr/bin/env python3
"""
Convert an image file to OLED bitmap format for Jumperless startup logo.

The output format is:
- 4 bytes header: width (2 bytes little-endian), height (2 bytes little-endian)
- Bitmap data: each byte represents 8 horizontal pixels, MSB first (Adafruit GFX format)

Usage:
    python image_to_oled_bitmap.py input_image.png output.bin [--width 128] [--height 32]

The same conversion is available in JumperIDE: use Tools → "Convert PNG to OLED bitmap"
or open any .bin in the bitmap viewer and click "Import PNG" to upload a 128×32 image.
"""

import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: PIL (Pillow) not installed. Install it with:")
    print("  pip install Pillow")
    sys.exit(1)


def image_to_bitmap(image_path, output_path, target_width=128, target_height=32, threshold=128, invert=False):
    """
    Convert an image to OLED bitmap format.
    
    Args:
        image_path: Path to input image file
        output_path: Path to output binary file
        target_width: Target width in pixels (default 128)
        target_height: Target height in pixels (default 32)
        threshold: Brightness threshold for black/white conversion (0-255, default 128)
        invert: If True, invert the image colors (default False)
    """
    try:
        # Open image
        img = Image.open(image_path)
        print(f"Original image size: {img.size}")
        print(f"Original image mode: {img.mode}")
        
        # Handle transparency by compositing onto appropriate background
        # For OLED displays: 1=white/on, 0=black/off
        # Always composite transparent areas as black (matches OLED's natural black)
        # The invert will happen after this, converting black→white if needed
        if img.mode in ('RGBA', 'LA', 'PA'):
            # Always use black background - transparent areas should be black (off pixels)
            background = Image.new('RGB', img.size, (0, 0, 0))
            # Composite image onto black background
            if img.mode == 'RGBA':
                background.paste(img, mask=img.split()[3])  # Use alpha channel as mask
            else:
                background.paste(img, mask=img.split()[-1])  # Use last channel as mask
            img = background
            print(f"Composited transparent image onto black background")
        elif img.mode == 'P':
            # Convert palette mode to RGB
            img = img.convert('RGB')
        
        # Resize to target dimensions
        img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)
        img = img.convert('L')  # Convert to grayscale
        
        # Invert if requested
        if invert:
            from PIL import ImageOps
            img = ImageOps.invert(img)
            print("Image colors inverted")
        
        # Convert to black and white based on threshold
        pixels = img.load()
        bitmap_data = []
        
        # Process image row by row
        # Adafruit GFX format: MSB is leftmost pixel, LSB is rightmost pixel
        for y in range(target_height):
            for x in range(0, target_width, 8):
                byte_val = 0
                for bit in range(8):
                    if x + bit < target_width:
                        # Check pixel brightness
                        pixel_val = pixels[x + bit, y]
                        # If pixel is bright (above threshold), set bit to 1
                        # Use MSB-first: bit 7 is leftmost pixel, bit 0 is rightmost
                        if pixel_val >= threshold:
                            byte_val |= (1 << (7 - bit))
                bitmap_data.append(byte_val)
        
        # Write output file
        with open(output_path, 'wb') as f:
            # Write header: width (little-endian), height (little-endian)
            f.write(target_width.to_bytes(2, byteorder='little'))
            f.write(target_height.to_bytes(2, byteorder='little'))
            # Write bitmap data
            f.write(bytes(bitmap_data))
        
        print(f"Successfully converted {image_path} to {output_path}")
        print(f"Output size: {target_width}x{target_height} pixels, {len(bitmap_data)} bytes")
        print(f"\nTo use this bitmap as startup image, add to your config:")
        print(f'  [top_oled]')
        print(f'  startup_message = {output_path}')
        
    except Exception as e:
        print(f"Error converting image: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Convert image to OLED bitmap format for Jumperless',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert PNG to 128x32 bitmap
  python image_to_oled_bitmap.py logo.png /logo.bin
  
  # Convert with custom dimensions
  python image_to_oled_bitmap.py logo.png /logo.bin --width 64 --height 32
  
  # Adjust brightness threshold
  python image_to_oled_bitmap.py logo.png /logo.bin --threshold 200
        """
    )
    
    parser.add_argument('input', help='Input image file (PNG, JPG, etc.)')
    parser.add_argument('output', help='Output bitmap file (typically /*.bin)')
    parser.add_argument('--width', type=int, default=128, help='Target width in pixels (default: 128)')
    parser.add_argument('--height', type=int, default=32, help='Target height in pixels (default: 32)')
    parser.add_argument('--threshold', type=int, default=128, 
                       help='Brightness threshold for black/white conversion (0-255, default: 128)')
    parser.add_argument('--invert', action='store_true', 
                       help='Invert colors (use for black-text-on-white images to get white-text-on-black OLED)')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not Path(args.input).exists():
        print(f"Error: Input file '{args.input}' not found")
        sys.exit(1)
    
    if args.width <= 0 or args.width > 128:
        print(f"Error: Width must be between 1 and 128")
        sys.exit(1)
    
    if args.height <= 0 or args.height > 64:
        print(f"Error: Height must be between 1 and 64")
        sys.exit(1)
    
    if args.threshold < 0 or args.threshold > 255:
        print(f"Error: Threshold must be between 0 and 255")
        sys.exit(1)
    
    image_to_bitmap(args.input, args.output, args.width, args.height, args.threshold, args.invert)


if __name__ == '__main__':
    main()

