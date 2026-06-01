"""Low-level SVG shape Elements (built on the vendored svgpanel Element).

Primitives compose these; nothing here knows about footprints, layout, or
control semantics. Presentation attributes (not CSS style) for NanoSVG.
"""
from __future__ import annotations
from .._svgpanel import Element


def group(eid: str = "") -> Element:
    return Element("g", eid)


def circle(cx: float, cy: float, r: float, *, fill: str = "none",
           stroke: str = "", sw: float = 0.0, eid: str = "") -> Element:
    e = Element("circle", eid)
    e.setAttribFloat("cx", cx).setAttribFloat("cy", cy).setAttribFloat("r", r)
    e.setAttrib("fill", fill)
    if stroke:
        e.setAttrib("stroke", stroke).setAttribFloat("stroke-width", sw)
    return e


def line(x0: float, y0: float, x1: float, y1: float, stroke: str, sw: float,
         cap: str = "round") -> Element:
    e = Element("line")
    e.setAttribFloat("x1", x0).setAttribFloat("y1", y0)
    e.setAttribFloat("x2", x1).setAttribFloat("y2", y1)
    e.setAttrib("stroke", stroke).setAttribFloat("stroke-width", sw)
    e.setAttrib("stroke-linecap", cap)
    return e


def rect(x: float, y: float, w: float, h: float, *, fill: str = "none",
         stroke: str = "", sw: float = 0.0, rx: float = 0.0, eid: str = "") -> Element:
    e = Element("rect", eid)
    e.setAttribFloat("x", x).setAttribFloat("y", y)
    e.setAttribFloat("width", w).setAttribFloat("height", h)
    if rx:
        e.setAttribFloat("rx", rx)
    e.setAttrib("fill", fill)
    if stroke:
        e.setAttrib("stroke", stroke).setAttribFloat("stroke-width", sw)
    return e


def path(d: str, *, fill: str = "", stroke: str = "", sw: float = 0.0,
         eid: str = "") -> Element:
    e = Element("path", eid).setAttrib("d", d)
    e.setAttrib("fill", fill)
    if stroke:
        e.setAttrib("stroke", stroke).setAttribFloat("stroke-width", sw)
    return e
