"""Minimal panel assembly (svgpanel-backed). Full Layout rendering arrives in M7;
for now this places a list of pre-built Elements onto a Panel with a themed
background, which is enough to validate primitives in isolation.
"""
from __future__ import annotations
from ._svgpanel import Element, LinearGradient, Panel
from .config.theme import Theme


def background(panel: Panel, theme: Theme) -> None:
    defs = Element("defs")
    defs.append(LinearGradient("bg", 0, 0, 0, panel.mmHeight,
                               theme.color("bg_top"), theme.color("bg_bot")))
    panel.append(defs)
    r = Element("rect")
    r.setAttribFloat("width", panel.mmWidth).setAttribFloat("height", panel.mmHeight)
    r.setAttrib("fill", "url(#bg)")
    panel.append(r)


def panel_svg(hp: int, elements: list, theme: Theme | None = None) -> str:
    p = Panel(hp)
    if theme is not None:
        background(p, theme)
    for e in elements:
        p.append(e)
    return p.svg()
