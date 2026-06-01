"""Jack primitive: panel art for a port.

Ring sized to the measured PJ301M footprint so the live port lands on it, with a
white face placeholder (same contrast/debug rationale as the dial). No ticks.
"""
from __future__ import annotations
from ..config.footprints import Footprint
from ..config.theme import Theme
from ..geometry import Vec
from .base import circle, group


def jack(center: Vec, footprint: Footprint, theme: Theme):
    r = footprint.visual_r
    g = group()
    g.append(circle(center.x, center.y, r, fill=theme.color("knob_face")))
    g.append(circle(center.x, center.y, r, stroke=theme.color("rule"), sw=theme.strokes.ring_mm))
    return g
