"""Dotted divider lines (e.g. the vertical rules between grid columns)."""
from __future__ import annotations
from .base import group, line


def dotted_vline(x: float, y0: float, y1: float, color: str,
                 gap: float = 1.6, dash: float = 0.6, sw: float = 0.22):
    g = group()
    y = y0
    while y < y1:
        g.append(line(x, y, x, min(y + dash, y1), color, sw, cap="butt"))
        y += gap
    return g
