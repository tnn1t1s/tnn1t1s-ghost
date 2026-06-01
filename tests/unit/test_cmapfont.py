"""The digit gate: prove CmapFont renders every character of TNN1T1S (incl. the
digits), and document that plain svgpanel.Font drops glyph-name-mismatched chars.
"""
from panelkit._svgpanel import Font as PlainFont
from panelkit.text import CmapFont


def test_cmapfont_renders_all_glyphs(fontbook):
    font = fontbook.role("brand").font
    assert isinstance(font, CmapFont)
    d = font.render("TNN1T1S", 0, 0, 30)
    # one moveto per glyph minimum (7 chars) -> digits are not dropped
    assert d.count("M") >= 7


def test_digit_renders(fontbook):
    font = fontbook.role("brand").font
    assert "M" in font.render("1", 0, 0, 30)


def test_measure_counts_digits(fontbook):
    font = fontbook.role("brand").font
    assert font.measure("TNN1T1S", 30)[0] > font.measure("TNNTS", 30)[0]


def test_plainfont_drops_digit_documents_bug(fontbook):
    # svgpanel.Font keys glyphs by name; '1' -> glyph 'one', so .get('1') misses.
    plain = PlainFont(str(fontbook.role("brand").font.filename))
    assert "M" not in plain.render("1", 0, 0, 30)   # blank -> motivates CmapFont
