"""Anchor: the invisible named shape SvgHelper binds a live widget to.

SvgHelper places the widget at the shape's bounding-box centre, so the anchor is
a tiny rect centred on the control. Its id is the binding key (role.group.name).
Drawn invisibly; the visible art (dial/jack ring) is a separate element, so art
style is decoupled from binding geometry.
"""
from __future__ import annotations
from ..geometry import Vec
from .base import rect

ANCHOR_SIZE_MM = 1.0   # arbitrary; only the centre matters to SvgHelper


def anchor(center: Vec, eid: str):
    half = ANCHOR_SIZE_MM / 2.0
    return rect(center.x - half, center.y - half, ANCHOR_SIZE_MM, ANCHOR_SIZE_MM,
                fill="none", eid=eid)
