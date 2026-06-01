"""Dial primitive: the panel art for a knob.

- White knob-face disc fills the ring (high-contrast placeholder where the live
  knob sits; the opaque widget covers it in Rack).
- Ring diameter == measured knob footprint, so the live widget lands on it.
- Two-tier tick scale (measured from the reference): minor ticks every
  `minor_step_deg`, major ticks every `major.every`-th, and longest/brightest
  cardinal ticks at 9/12/3 o'clock. Sweep min(7:00)..max(5:00), empty 5->7.
  All parameters come from the theme's divider motif.
"""
from __future__ import annotations
import math

from ..config.footprints import Footprint
from ..config.theme import Theme
from ..geometry import Vec
from .base import circle, group, line


def _is_cardinal(deg: float, cardinals: list[float], tol: float = 0.6) -> bool:
    return any(abs(deg - c) <= tol for c in cardinals)


def _tick(g, c: Vec, a_rad: float, r0: float, length: float, color: str, sw: float):
    x0, y0 = c.x + math.cos(a_rad) * r0, c.y + math.sin(a_rad) * r0
    x1, y1 = c.x + math.cos(a_rad) * (r0 + length), c.y + math.sin(a_rad) * (r0 + length)
    g.append(line(x0, y0, x1, y1, color, sw, cap="butt"))   # square ends (match reference)


def dial(center: Vec, footprint: Footprint, theme: Theme):
    d = theme.motifs["divider"]
    minor, major, card = d["minor"], d["major"], d["cardinal"]
    sweep, step = d["sweep_deg"], d["minor_step_deg"]
    r = footprint.visual_r
    r0 = r + d["tick_gap_mm"]

    g = group()
    g.append(circle(center.x, center.y, r, fill=theme.color("knob_face")))      # white face
    g.append(circle(center.x, center.y, r, stroke=theme.color("rule"), sw=theme.strokes.ring_mm))

    n = round(sweep / step) + 1
    for i in range(n):
        deg = -sweep / 2 + i * step          # from top dead centre (0 = 12 o'clock)
        a = math.radians(deg - 90)           # -90 -> up
        if _is_cardinal(deg, card["degs"]):
            _tick(g, center, a, r0, card["len_mm"], theme.color(card["color"]), card["sw_mm"])
        elif i % major["every"] == 0:
            _tick(g, center, a, r0, major["len_mm"], theme.color(major["color"]), major["sw_mm"])
        else:
            _tick(g, center, a, r0, minor["len_mm"], theme.color(minor["color"]), minor["sw_mm"])
    return g


def outer_radius(footprint: Footprint, theme: Theme) -> float:
    d = theme.motifs["divider"]
    longest = max(d["minor"]["len_mm"], d["major"]["len_mm"], d["cardinal"]["len_mm"])
    return footprint.visual_r + d["tick_gap_mm"] + longest
