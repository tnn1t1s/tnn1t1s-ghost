"""Single seam onto the vendored svgpanel library (vendor/svgpanel/svgpanel.py).

The vendored file is kept byte-for-byte pristine (MIT, easy upstream sync); all
panelkit code imports svgpanel symbols from here so the path insertion lives in
exactly one place.
"""
import pathlib
import sys

_VENDOR = pathlib.Path(__file__).resolve().parent.parent / "vendor" / "svgpanel"
if str(_VENDOR) not in sys.path:
    sys.path.insert(0, str(_VENDOR))

from svgpanel import (  # noqa: E402,F401
    Panel, Element, TextItem, TextPath, Font, LinearGradient, BorderRect,
    HorizontalAlignment, VerticalAlignment, Move, Line,
)
