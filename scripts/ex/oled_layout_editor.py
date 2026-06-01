"""
OLED layout editor (probe pads + clickwheel + terminal)
=======================================================
Design a 2-line OLED layout on the hardware, then save it to JSON so you can
read off the font / size / position values to bake into native code.

Controls:
  - Probe-tap a TOP-row pad (1-30)    -> move the selected text to the TOP line
  - Probe-tap a BOTTOM-row pad (31-60) -> move it to the BOTTOM line
        pad 1 / 31  = left justified
        pad 30 / 60 = right justified
        in between  = slide along that line
  - Probe-tap a TOP rail pad (+ or GND)    -> nudge the selected text UP 1 px
  - Probe-tap a BOTTOM rail pad (+ or GND) -> nudge it DOWN 1 px (hold to repeat)
  - Clickwheel turn   -> change the selected text's font SIZE
  - Clickwheel press  -> select the next text box
  - Probe CONNECT btn -> cycle the selected text's FONT
  - Probe REMOVE btn  -> save the layout to /screens/layout.json
  - Type in this terminal then Enter -> set the selected text's TEXT
  - Ctrl-C            -> quit (clears the OLED)

The currently selected box is highlighted with a rectangle.
"""

import time
import sys
import oledgui
from oledgui import *      # Screen, Text, ALIGN_*, set_var, reset, ...
from jumperless import *   # read_probe, NO_PAD, clickwheel_*, check_button, BUTTON_*, rail pads, ...

# Discard any screens a previous run left active (so re-running is clean).
oledgui.reset()

FONTS = ["Eurostile", "Pragmatism", "Jokerman", "Comic Sans",
         "Courier New", "New Science", "Berkeley Mono", "Iosevka Regular"]
SIZE_MIN, SIZE_MAX = 5, 40

# Rail pads (probe), used to nudge the selected text vertically by 1 px.
# Top rails move it up; bottom rails move it down. Values come from the probe
# pad constants (TOP_RAIL_PAD=101, BOTTOM_RAIL_PAD=102, TOP_GND_PAD=104, ...).
TOP_NUDGE_PADS = (int(TOP_RAIL_PAD), int(BOTTOM_RAIL_PAD))
BOT_NUDGE_PADS = (int(TOP_GND_PAD), int(BOTTOM_GND_PAD))
NUDGE_THROTTLE_MS = 10   # while a rail pad is held, step at most this often

scr = Screen()
boxes = [
    scr.add(Text("Text 1", x=0, y=0, font="Eurostile",  size=10, halign=ALIGN_LEFT, valign=ALIGN_TOP)),
    scr.add(Text("Text 2", x=0, y=0, font="Pragmatism", size=10, halign=ALIGN_LEFT, valign=ALIGN_BOTTOM)),
]
# One font-cycle index per box, derived from each box's starting font so this
# stays correct no matter how many boxes are in the list above (a hardcoded
# [0, 1] used to IndexError as soon as a third box was added).
font_idx = [FONTS.index(b.font) if b.font in FONTS else 0 for b in boxes]
sel = 0


def refresh_selection():
    for i, b in enumerate(boxes):
        b.box = (i == sel)


def place(box, pad):
    """Move `box` to the line + horizontal position implied by a probe pad."""
    if pad <= 30:
        local, valign = pad, ALIGN_TOP
    else:
        local, valign = pad - 30, ALIGN_BOTTOM
    if local <= 1:
        box.anchor(ALIGN_LEFT, valign)          # left justified
    elif local >= 30:
        box.anchor(ALIGN_RIGHT, valign)         # right justified
    else:
        box.x = int((local - 1) * 110 / 29)     # slide along the line
        box.anchor(ALIGN_FREE, valign)


def nudge_y(box, dy):
    """Move `box` vertically by `dy` pixels. Switches the vertical placement to
    free/absolute (keeping the current horizontal anchor) so it can step pixel
    by pixel. The first nudge off a TOP/BOTTOM line seeds y from that edge."""
    if box._valign != ALIGN_FREE:
        if box._valign == ALIGN_BOTTOM:
            box._y = 32 - box._size
        elif box._valign == ALIGN_MIDDLE:
            box._y = (32 - box._size) // 2
        else:                                    # TOP (or unset)
            box._y = 0
    y = box._y + dy
    if y < 0:
        y = 0
    elif y > 31:
        y = 31
    box.y = y                                    # absolute y
    ha = box._halign if box._halign is not None else ALIGN_LEFT
    box.anchor(ha, ALIGN_FREE)                   # keep H anchor, free V


# --- non-blocking terminal line reader (best effort) -----------------------
try:
    import select
    _poll = select.poll()
    _poll.register(sys.stdin, select.POLLIN)
except Exception:
    _poll = None
_inbuf = ""


def prompt_text():
    """Print the text-entry prompt."""
    print("Enter new text: ", end="")


def poll_line():
    """Return a typed line (without newline) on Enter, else None. Echoes typed
    characters so you can see what you're entering."""
    global _inbuf
    if _poll is None:
        return None
    try:
        if not _poll.poll(0):
            return None
        ch = sys.stdin.read(1)
    except Exception:
        return None
    if not ch:
        return None
    if ch == "\x03":                 # Ctrl-C
        raise KeyboardInterrupt
    if ch in ("\n", "\r"):
        line, _inbuf = _inbuf, ""
        print()                      # finish the prompt line
        return line
    if ch in ("\x08", "\x7f"):       # backspace
        if _inbuf:
            _inbuf = _inbuf[:-1]
            print("\b \b", end="")   # erase the last echoed char
        return None
    _inbuf += ch
    print(ch, end="")                # echo
    return None


refresh_selection()
scr.show()

clickwheel_reset_position()
last_pos = clickwheel_get_position()
lastBtn = BUTTON_NONE
lastBtnTime = 0
lastBtnTimeout = 500

lastClick = CLICKWHEEL_IDLE
lastClickTime = 0
lastClickTimeout = 500

lastNudgeTime = 0

print("Layout editor:")
print("  tap pads 1-30 (top) / 31-60 (bottom) to position the selected box")
print("  tap rail pads to nudge it vertically 1px (top rails=up, bottom rails=down)")
print("  wheel=size  press=next box  CONNECT=font  REMOVE=save  type+Enter=text  Ctrl-C=quit")
prompt_text()

try:
    while True:
        # Probe pad -> position the selected box.
        pad = read_probe(False)
        if pad != NO_PAD:
            p = int(pad)
            if 1 <= p <= 60:
                place(boxes[sel], p)
            elif (p in TOP_NUDGE_PADS or p in BOT_NUDGE_PADS):
                # Rail pads nudge the selected text vertically by 1 px
                # (top rails up, bottom rails down), throttled while held.
                if time.ticks_ms() - lastNudgeTime > NUDGE_THROTTLE_MS:
                    nudge_y(boxes[sel], -1 if p in TOP_NUDGE_PADS else 1)
                    lastNudgeTime = time.ticks_ms()

        # Clickwheel turn -> font size of selected box.
        pos = clickwheel_get_position()
        d = pos - last_pos
        if d != 0:
            last_pos = pos
            s = boxes[sel].size + d
            s = SIZE_MIN if s < SIZE_MIN else SIZE_MAX if s > SIZE_MAX else s
            boxes[sel].size = s

        # Clickwheel press -> select next box.
        if clickwheel_get_button() == CLICKWHEEL_PRESSED and lastClick != CLICKWHEEL_PRESSED:
            sel = (sel + 1) % len(boxes)
            refresh_selection()
            lastClick = CLICKWHEEL_PRESSED
            lastClickTime = time.ticks_ms()

        # Probe buttons: CONNECT cycles font, REMOVE saves.
        # force_service("ProbeButton")
        
        btn = check_button()
        if btn == BUTTON_CONNECT and lastBtn != BUTTON_CONNECT:
            font_idx[sel] = (font_idx[sel] + 1) % len(FONTS)
            boxes[sel].font = FONTS[font_idx[sel]]
            lastBtn = BUTTON_CONNECT
            lastBtnTime = time.ticks_ms()
        elif btn == BUTTON_REMOVE and lastBtn != BUTTON_REMOVE:
            if scr.save("layout"):
                print("saved -> /screens/layout.json")
                lastBtn = BUTTON_REMOVE
                lastBtnTime = time.ticks_ms()

        if time.ticks_ms() - lastBtnTime > lastBtnTimeout:
            lastBtn = BUTTON_NONE

        if time.ticks_ms() - lastClickTime > lastClickTimeout:
            lastClick = CLICKWHEEL_IDLE

        # Terminal text -> set selected box text (then re-prompt).
        line = poll_line()
        if line is not None:
            if line:
                boxes[sel].text = line
            prompt_text()

        time.sleep_ms(20)
finally:
    scr.hide()
    scr.free()
    print("editor stopped, OLED cleared")
