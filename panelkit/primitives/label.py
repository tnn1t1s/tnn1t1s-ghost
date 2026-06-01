"""Label primitive: a line of text rendered as a vector path (never <text>,
which NanoSVG ignores). Alignment via svgpanel's TextItem; glyphs via CmapFont.
"""
from __future__ import annotations
from .._svgpanel import HorizontalAlignment, TextItem, VerticalAlignment
from ..text.fonts import RoleFont


def label(x: float, y: float, text: str, rolefont: RoleFont, color: str,
          halign: HorizontalAlignment = HorizontalAlignment.Center,
          valign: VerticalAlignment = VerticalAlignment.Middle,
          size_pt: float | None = None):
    item = TextItem(text, rolefont.font, size_pt or rolefont.size_pt)
    return item.toPath(x, y, halign, valign, style=f"fill:{color}")
