"""Ghost mark + ghost separator (the suite logo).

ghost(): the glyph — domed top, scalloped bottom, two eye cutouts.
ghost_separator(): the glyph centred with short flanking rules (used under the
module name).
"""
from __future__ import annotations
from .._svgpanel import Element
from ..config.theme import Theme
from .base import circle, group, line


def ghost(cx: float, cy: float, height: float, body_color: str, eye_color: str):
    wv = height * 0.82                 # body width
    l, r = cx - wv / 2, cx + wv / 2
    top = cy - height / 2
    body_y = cy + height / 2 - wv * 0.28
    dome = wv / 2
    d = [f"M{l:.3f},{body_y:.3f}",
         f"L{l:.3f},{top + dome:.3f}",
         f"A{dome:.3f},{dome:.3f} 0 0 1 {r:.3f},{top + dome:.3f}",
         f"L{r:.3f},{body_y:.3f}"]
    n = 4                              # scalloped bottom (bumps point down)
    step = (r - l) / n
    for i in range(n):
        x1 = r - (i + 1) * step
        d.append(f"A{step / 2:.3f},{step / 2:.3f} 0 0 1 {x1:.3f},{body_y:.3f}")
    d.append("Z")
    g = group()
    g.append(Element("path").setAttrib("d", " ".join(d)).setAttrib("fill", body_color))
    er = wv * 0.13
    for ex in (cx - wv * 0.2, cx + wv * 0.2):
        g.append(circle(ex, cy - height * 0.04, er, fill=eye_color))
    return g


def ghost_separator(cx: float, y: float, theme: Theme, span_mm: float = 13.0):
    """Ghost glyph centred, flanked by rules so the whole separator spans
    `span_mm` (the glyph sits in the central gap)."""
    gh = theme.motifs["ghost"]
    h = gh["height_mm"]
    gap = (h * 0.82) / 2 + gh.get("gap_pad_mm", 0.8)   # half glyph width + configurable pad
    flank = max(span_mm / 2 - gap, 0.0)
    g = group()
    g.append(ghost(cx, y, h, theme.color("label"), theme.color("bg_bot")))
    g.append(line(cx - gap - flank, y, cx - gap, y,
                  theme.color("rule"), theme.strokes.rule_mm))
    g.append(line(cx + gap, y, cx + gap + flank, y,
                  theme.color("rule"), theme.strokes.rule_mm))
    return g
