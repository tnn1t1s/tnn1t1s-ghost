"""The single source of truth for physical units and px<->mm conversion.

Every dimension in panelkit is in millimetres. SVG user units in VCV panels are
pixels at 75 DPI; mm = px * 25.4 / 75. Nothing else in the codebase converts
units — this is the one seam (the "huge dials" bug was a units/footprint bug).
"""

# Eurorack physical facts
HP_MM = 5.08              # one horizontal pitch unit
PANEL_HEIGHT_MM = 128.5   # standard 3U panel height
SVG_DPI = 75.0            # VCV/NanoSVG render DPI for panel SVGs
MM_PER_INCH = 25.4


def px_to_mm(px: float) -> float:
    return px * MM_PER_INCH / SVG_DPI


def mm_to_px(mm: float) -> float:
    return mm * SVG_DPI / MM_PER_INCH


def hp_to_mm(hp: int) -> float:
    return hp * HP_MM
