# OLED Custom Bitmap - Setup Guide

## Quick Troubleshooting: "Bitmap Error" or Filename Displayed

### Issue: Seeing filename text or "Bitmap Error" instead of your image

**Root Cause:** The bitmap file hasn't been copied to the Jumperless filesystem yet.

**Solution:**

1. **Create your bitmap file**
   ```bash
   cd /Users/kevinsanto/Documents/GitHub/JumperlOS/scripts
   python3 image_to_oled_bitmap.py JText.png jogotext.bin
   ```

2. **Copy to Jumperless filesystem**
   
   **Option A: Via USB Mass Storage Mode**
   ```
   - Enter USB mode on Jumperless: [menu] → [u] → [m] → [usb mode]
   - Jumperless will appear as "JUMPERLESS" drive
   - Copy jogotext.bin to the root of the drive
   - Safely eject the drive
   - Exit USB mode: [Ctrl+Q] or wait for timeout
   ```

   **Option B: Via Serial Terminal**
   ```
   - Use eKilo editor or file manager on Jumperless
   - Upload file through terminal
   ```

3. **Verify file is on device**
   ```
   - Run File Manager app
   - Check if jogotext.bin is listed in root directory
   ```

4. **Set in config**
   ```ini
   [top_oled]
   startup_message = jogotext.bin
   ```

5. **Restart or reconnect OLED**
   ```
   - Reboot device OR
   - Disconnect/reconnect OLED from menu
   ```

### File Path Reference

**Config says:** `startup_message = jogotext.bin`

**System looks for:**
- First tries: `/jogotext.bin` (root directory)
- Paths without `/` are auto-prefixed with `/`

**Where to place files:**
```
Jumperless Filesystem (on-device):
/
├── jogotext.bin          ← Place here for startup_message = jogotext.bin
├── config.txt
├── images/
│   └── logo.bin          ← Place here for startup_message = images/logo.bin
└── ...

NOT here:
~/Documents/GitHub/JumperlOS/scripts/jogotext.bin  ← This is on your computer!
```

### Verification Commands

**Check if file exists on device:**
```
1. Enter File Manager (menu → Apps → File Manager)
2. Look for jogotext.bin in file list
3. If not there, file needs to be uploaded
```

**Check current config:**
```
1. View config.txt on device
2. Look for [top_oled] section
3. Check startup_message value
```

## Complete Workflow Example

```bash
# 1. Create bitmap on your computer
cd ~/Documents/GitHub/JumperlOS/scripts
python3 image_to_oled_bitmap.py JText.png jogotext.bin

# 2. Enter USB mode on Jumperless
#    (from serial terminal: menu → u → m → enter USB mode)

# 3. Copy file to Jumperless drive
cp jogotext.bin /Volumes/JUMPERLESS/

# 4. Edit config on Jumperless drive
nano /Volumes/JUMPERLESS/config.txt
# Add: startup_message = jogotext.bin under [top_oled]

# 5. Safely eject drive

# 6. Exit USB mode and reboot Jumperless
```

## Why You See "Bitmap Error"

At startup, the code flow is:

```cpp
if (startup_message = "jogotext.bin") {
    if (has .bin extension) {  // ✓ YES
        if (file exists on device) {  // ✗ NO - not copied yet!
            load_and_display();
        } else {
            display_default_jogo_logo();  // ✓ NOW - fallback to default
        }
    }
}
```

**Before fix:** Showed "Bitmap Error" when file not found
**After fix:** Falls back to default Jogo logo silently

## Testing

To verify your bitmap file is correct before uploading:

```bash
# Visualize bitmap
python3 << 'EOF'
with open('jogotext.bin', 'rb') as f:
    w = int.from_bytes(f.read(2), 'little')
    h = int.from_bytes(f.read(2), 'little')
    print(f"Dimensions: {w}x{h}")
    for y in range(min(10, h)):
        row = ""
        for x in range(0, w, 8):
            byte = f.read(1)[0]
            for bit in range(8):
                if x + bit < w:
                    row += "█" if (byte >> (7-bit)) & 1 else " "
        print(row)
EOF
```

## Summary

✅ **If you see the filename text:** File path detection is working, but file isn't on device yet  
✅ **If you see default Jogo logo:** Normal fallback when file not found  
✅ **If you see your image:** Success! File is loaded correctly  

**Next step:** Copy `jogotext.bin` to the Jumperless filesystem (root directory).

