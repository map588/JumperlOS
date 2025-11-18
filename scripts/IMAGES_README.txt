================================================================================
                        JUMPERLESS /IMAGES FOLDER
================================================================================

This folder contains images for OLED displays and breadboard LEDs.

FOLDER STRUCTURE:
--------------------------------------------------------------------------------
/images/
├── oled/              # OLED display images (.bin format, monochrome)
├── breadboard/        # Breadboard LED images (.bin format, RGB565)
└── README.txt         # This file

OLED IMAGES:
--------------------------------------------------------------------------------
Format: Monochrome bitmap (1 bit per pixel)
Dimensions: Typically 128x32 or 128x64
File size: 512 bytes (128x32) or 1024 bytes (128x64)

Files in oled/:
  - jogo.bin              Original Jumperless logo
  - jumperless_text.bin   "Jumperless" text logo
  - *.bin                 Custom images

Usage:
  1. View in "OLED Images" app (Menu → Apps → OLED Images)
  2. Set as startup: [top_oled] startup_message = logo.bin

BREADBOARD LED IMAGES:
--------------------------------------------------------------------------------
Format: RGB565 color (16-bit per pixel)
Dimensions: 30 columns × 14 rows
File size: 840 bytes per frame

Files in breadboard/:
  - startup_frame*.bin    Startup animation frames
  - *.bin                 Custom images
  - *_anim.bin            Complete animations with header

Usage:
  Display via custom code using displayBreadboardImage() function

CREATING NEW IMAGES:
--------------------------------------------------------------------------------

OLED Images:
  python3 scripts/image_to_oled_bitmap.py input.png oled/output.bin

Breadboard Images:
  python3 scripts/image_to_breadboard.py input.png breadboard/output.bin

See IMAGE_SYSTEMS_GUIDE.md for complete documentation.

TIPS:
--------------------------------------------------------------------------------
- OLED images work best with high contrast, simple designs
- Use --invert flag for images with dark backgrounds
- Breadboard images are automatically resized to 30x14
- Keep filenames short and descriptive

================================================================================
For more information, see IMAGE_SYSTEMS_GUIDE.md in the project root
================================================================================

