"""CmapFont: svgpanel.Font that resolves characters through the font cmap.

svgpanel.Font.render does ``self.glyphs.get(ch)`` where ``glyphs`` is keyed by
glyph NAME, so any character whose glyph name differs from the character (every
digit: '1' -> 'one') silently renders as a blank 3-em space. We resolve
char -> glyph name via ``getBestCmap`` first. The mm math matches svgpanel
exactly so output stays consistent with the vendored library.
"""
from __future__ import annotations
from fontTools.misc.transform import DecomposedTransform
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

from .._svgpanel import Font


class CmapFont(Font):
    def __init__(self, filename: str) -> None:
        super().__init__(filename)
        self.cmap = self.ttfont.getBestCmap()

    def _glyph(self, ch: str):
        name = self.cmap.get(ord(ch))
        return self.glyphs.get(name) if name else None

    def render(self, text: str, xpos: float, ypos: float, points: float) -> str:
        mmPerEm = (25.4 / 72) * points
        mmPerUnit = mmPerEm / self.ttfont["head"].unitsPerEm
        x = xpos
        y = ypos + mmPerUnit * (self.ttfont["head"].yMax + self.ttfont["head"].yMin / 2)
        spen = SVGPathPen(self.glyphs)
        for ch in text:
            glyph = self._glyph(ch)
            if glyph:
                tran = DecomposedTransform(translateX=x, translateY=y,
                                           scaleX=mmPerUnit, scaleY=-mmPerUnit).toTransform()
                glyph.draw(TransformPen(spen, tran))
                x += mmPerUnit * glyph.width
            else:
                x += mmPerEm / 3
        return str(spen.getCommands())

    def measure(self, text: str, points: float) -> tuple[float, float]:
        mmPerEm = (25.4 / 72) * points
        mmPerUnit = mmPerEm / self.ttfont["head"].unitsPerEm
        x = 0.0
        y = mmPerUnit * (self.ttfont["head"].yMax - self.ttfont["head"].yMin)
        for ch in text:
            glyph = self._glyph(ch)
            x += mmPerUnit * glyph.width if glyph else mmPerEm / 3
        return (x, y)
