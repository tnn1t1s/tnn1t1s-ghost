"""Chevron rule: a horizontal separator that dips to a downward V at its centre
(the recurring Ghost separator motif). One connected polyline.
"""
from __future__ import annotations
from .._svgpanel import Element
from ..config.theme import Theme


def chevron_rule(cx: float, half_w: float, y: float, theme: Theme,
                 color: str | None = None, sw: float | None = None):
    notch = theme.motifs["chevron"]["notch_mm"]
    color = color or theme.color("rule")
    sw = theme.strokes.rule_mm if sw is None else sw
    pts = (f"{cx - half_w:.3f},{y:.3f} {cx - notch:.3f},{y:.3f} "
           f"{cx:.3f},{y + notch * 0.7:.3f} {cx + notch:.3f},{y:.3f} "
           f"{cx + half_w:.3f},{y:.3f}")
    e = Element("polyline").setAttrib("points", pts)
    e.setAttrib("fill", "none").setAttrib("stroke", color)
    e.setAttribFloat("stroke-width", sw)
    e.setAttrib("stroke-linejoin", "round").setAttrib("stroke-linecap", "round")
    return e
