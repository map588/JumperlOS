"""
OLED live stats page
=====================
Builds a retained OLED screen with live-updating readouts and shows it as a
PERSISTENT idle page (show(persist=True)). It takes the place of the boot logo:
whenever the UI returns to idle (e.g. after leaving a menu) it reappears, it
steps aside while other content is shown instead of fighting it, and it keeps
updating in the background even after this script ends - no redraw loop needed.

Text {tokens} auto-resolve from:
  - built-in sources: {adc:N}, {gpio:N}, {dac:N}, {uptime}, {freemem}, {undo}
  - custom values you push with set_var("name", value)

Run it, then watch the top OLED. Change a GPIO or ADC input and the numbers
update on their own. Open a menu and the menu shows cleanly; leave it and the
stats page comes back.
"""

import time
import oledgui
from oledgui import Screen, Text, Line, set_var, ALIGN_CENTER, ALIGN_TOP

# Discard any screen a previous run left active so re-running doesn't leak
# handles. (The page below is intentionally left persistent on exit.)
oledgui.reset()

scr = Screen()

# Title centered along the top, with a divider line beneath it.
scr.add(Text("JUMPERLESS", font="Pragmatism", size=5,
             halign=ALIGN_CENTER, valign=ALIGN_TOP))
scr.add(Line(0, 4, 34, 4))
scr.add(Line(94, 4, 127, 4))

# Live readouts (free-positioned). These re-resolve every render tick.
scr.add(Text("{adc:0}V {adc:1}V {adc:2}V {adc:3}V", x=0, y=10, font="Pragmatism", size=5))
scr.add(Text("uptime {uptime}", x=50, y=18, font="Pragmatism", size=6))

# # # A custom value pushed from code, shown on the right.
# status = scr.add(Text("{status}", x=60, y=14, font="Pragmatism", size=6))

# persist=True: register this as the idle screen so it survives this script and
# takes the logo's place when the UI is idle. (Without persist it would only be
# a one-shot foreground show that's dropped once anything else draws.)
scr.show(persist=True)
set_var("status", "ready")

# This screen is PERSISTENT: built-in tokens ({adc:0}, {uptime}) keep updating
# on their own in the background even after this script exits. The loop below
# just demonstrates pushing a custom value; Ctrl-C stops the loop and LEAVES the
# page registered as the idle display.
#
# To take it down later, from the REPL:
#     import oledgui; oledgui.hide()
n = 0
try:
    while True:
        set_var("status", "n" + str(n))
        n = (n + 1) % 1000
        time.sleep(1)
except KeyboardInterrupt:
    print("stats page left running; call oledgui.hide() to clear it")
