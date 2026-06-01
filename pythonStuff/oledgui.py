"""
oledgui - object-oriented OLED screen layout for Jumperless
===========================================================

A friendly object-oriented wrapper over the native retained-screen API
(oled_screen, oled_add_text, oled_add_shape, oled_set, oled_set_var, ...).

    from oledgui import Screen, Text, Rect, set_var

    scr = Screen()
    scr.add(Text("ADC0: {adc:0} V", x=0, y=0, font="Pragmatism", size=10))
    scr.add(Rect(0, 0, 128, 32))            # border
    scr.show()

Text may contain {token} templates that update automatically in the
background:
  - built-in sources: {gpio:N}, {adc:N}, {dac:N}, {uptime}, {millis},
    {freemem}, {undo}
  - custom values pushed from code: set_var("name", value) -> {name}

Screens can be saved/loaded as JSON (one self-contained file per screen):
  scr.save("mylayout")            # -> /screens/mylayout.json
  scr = load_screen("mylayout")

This module is self-contained: it uses only the native flat functions from
the built-in `jumperless` module, so `import oledgui` works directly.
"""

from jumperless import (
    oled_screen as _screen_new,
    oled_screen_free as _screen_free,
    oled_screen_clear as _screen_clear,
    oled_screen_show as _screen_show,
    oled_screen_hide as _screen_hide,
    oled_screen_reset as _screen_reset,
    oled_add_text as _add_text,
    oled_add_shape as _add_shape,
    oled_set as _set,
    oled_set_var as set_var,
    oled_screen_save as _screen_save,
    oled_screen_load as _screen_load,
)

# Horizontal alignment
ALIGN_LEFT = 0
ALIGN_CENTER = 1
ALIGN_RIGHT = 2
# Vertical alignment
ALIGN_TOP = 0
ALIGN_MIDDLE = 1
ALIGN_BOTTOM = 2
# FREE: use the element's absolute x (for H) or y (for V) on that axis, so it
# can slide continuously while the other axis stays anchored.
ALIGN_FREE = 3
# Shape kinds
SHAPE_LINE = 0
SHAPE_RECT = 1
SHAPE_FILLED_RECT = 2


class _Element:
    """Base for screen elements. `handle` is assigned when added to a Screen."""
    def __init__(self):
        self.handle = -1
        self._screen = None

    def _apply(self, prop, value):
        if self.handle >= 0:
            _set(self.handle, prop, value)

    def set(self, prop, value):
        self._apply(prop, value)
        return self

    @property
    def x(self):
        return self._x
    @x.setter
    def x(self, v):
        self._x = v
        self._apply("x", v)

    @property
    def y(self):
        return self._y
    @y.setter
    def y(self, v):
        self._y = v
        self._apply("y", v)

    @property
    def z(self):
        return self._z
    @z.setter
    def z(self, v):
        self._z = v
        self._apply("z", v)

    @property
    def visible(self):
        return self._visible
    @visible.setter
    def visible(self, v):
        self._visible = bool(v)
        self._apply("visible", 1 if v else 0)


class Text(_Element):
    """A text element. `text` may contain {token} templates that auto-update.

    Position is absolute (x, y) unless halign/valign are given, in which case
    the element is anchored (e.g. halign=ALIGN_CENTER centers it horizontally).
    """
    def __init__(self, text, x=0, y=0, font="Pragmatism", size=8,
                 halign=None, valign=None, z=0):
        super().__init__()
        self._text = text
        self._x = x
        self._y = y
        self._font = font
        self._size = size
        self._halign = halign
        self._valign = valign
        self._z = z
        self._visible = True

    def _create(self, screen_handle):
        ha = -1 if self._halign is None else self._halign
        va = -1 if self._valign is None else self._valign
        self.handle = _add_text(screen_handle, self._text, x=self._x, y=self._y,
                                font=self._font, size=self._size,
                                halign=ha, valign=va, z=self._z)
        return self.handle

    @property
    def text(self):
        return self._text
    @text.setter
    def text(self, v):
        self._text = v
        self._apply("text", v)

    @property
    def font(self):
        return self._font
    @font.setter
    def font(self, v):
        self._font = v
        self._apply("font", v)

    @property
    def size(self):
        return self._size
    @size.setter
    def size(self, v):
        self._size = v
        self._apply("size", v)

    def anchor(self, halign, valign):
        """Switch to anchored positioning. Either axis may be ALIGN_FREE to use
        the element's absolute x/y on that axis (continuous positioning)."""
        self._halign = halign
        self._valign = valign
        self._apply("halign", halign)
        self._apply("valign", valign)
        self._apply("anchor", 1)
        return self

    @property
    def box(self):
        return getattr(self, "_box", False)
    @box.setter
    def box(self, v):
        self._box = bool(v)
        self._apply("box", 1 if v else 0)


class Shape(_Element):
    """A shape element: line, rectangle outline, or filled rectangle.

    LINE draws from (x, y) to (x + w, y + h). RECT / FILLED_RECT are w x h.
    """
    def __init__(self, kind=SHAPE_RECT, x=0, y=0, w=0, h=0, filled=False, z=0):
        super().__init__()
        self._kind = kind
        self._x = x
        self._y = y
        self._w = w
        self._h = h
        self._filled = filled
        self._z = z
        self._visible = True

    def _create(self, screen_handle):
        self.handle = _add_shape(screen_handle, kind=self._kind, x=self._x, y=self._y,
                                 w=self._w, h=self._h,
                                 filled=1 if self._filled else 0, z=self._z)
        return self.handle

    @property
    def w(self):
        return self._w
    @w.setter
    def w(self, v):
        self._w = v
        self._apply("w", v)

    @property
    def h(self):
        return self._h
    @h.setter
    def h(self, v):
        self._h = v
        self._apply("h", v)


def Line(x, y, x2, y2, z=0):
    """Convenience: a line from (x, y) to (x2, y2)."""
    return Shape(SHAPE_LINE, x=x, y=y, w=x2 - x, h=y2 - y, z=z)


def Rect(x, y, w, h, filled=False, z=0):
    """Convenience: a rectangle (outline unless filled=True)."""
    kind = SHAPE_FILLED_RECT if filled else SHAPE_RECT
    return Shape(kind, x=x, y=y, w=w, h=h, filled=filled, z=z)


class Screen:
    """A retained OLED screen. Add elements, then show() to make it the active
    display. The background render service keeps {token} text up to date."""
    def __init__(self):
        self.handle = _screen_new()
        if self.handle <= 0:
            raise RuntimeError("out of OLED screen handles")
        self.elements = []

    def add(self, element):
        """Add an element (Text/Shape). Returns the element for chaining."""
        element._create(self.handle)
        element._screen = self
        self.elements.append(element)
        return element

    def text(self, *args, **kwargs):
        """Shortcut: create + add a Text in one call; returns the Text."""
        return self.add(Text(*args, **kwargs))

    def shape(self, *args, **kwargs):
        """Shortcut: create + add a Shape in one call; returns the Shape."""
        return self.add(Shape(*args, **kwargs))

    def clear(self):
        """Remove all elements from the screen."""
        _screen_clear(self.handle)
        self.elements = []

    def show(self, persist=False):
        """Make this the active screen (starts live rendering).

        persist=False (default): a one-shot foreground show. The screen is
        displayed and live-updates while it's on top, but it does NOT come back
        on its own once other content (a menu, a toast, ...) draws over it, and
        it's torn down when the Python script/REPL session ends.

        persist=True: the screen takes the place of the boot logo as the idle
        display. Whenever the UI returns to idle (e.g. leaving a menu) it is
        redrawn automatically, it steps aside while other content is shown
        instead of fighting it, and it survives the script that created it.
        Call oledgui.hide() to take it down."""
        _screen_show(self.handle, 1 if persist else 0)
        return self

    def hide(self):
        """Stop showing this screen and forget any persistent idle registration
        so it won't reappear when the UI next returns to idle."""
        _screen_hide()

    def save(self, name):
        """Save this screen to /screens/<name>.json. Returns True on success."""
        return _screen_save(self.handle, name)

    def free(self):
        """Release the screen handle."""
        if self.handle > 0:
            _screen_free(self.handle)
            self.handle = -1


def load_screen(name):
    """Load /screens/<name>.json into a new Screen. Returns Screen or None."""
    h = _screen_load(name)
    if h <= 0:
        return None
    s = Screen.__new__(Screen)
    s.handle = h
    s.elements = []
    return s


def hide():
    """Hide any active screen (module-level convenience)."""
    _screen_hide()


def reset():
    """Free EVERY screen and blank the panel. Call at the start of a script to
    cleanly discard screens a previous run left active (re-running otherwise
    leaks screen handles until the pool is exhausted)."""
    _screen_reset()


__all__ = [
    "Screen", "Text", "Shape", "Line", "Rect", "load_screen", "set_var", "hide", "reset",
    "ALIGN_LEFT", "ALIGN_CENTER", "ALIGN_RIGHT", "ALIGN_FREE",
    "ALIGN_TOP", "ALIGN_MIDDLE", "ALIGN_BOTTOM",
    "SHAPE_LINE", "SHAPE_RECT", "SHAPE_FILLED_RECT",
]
