from panelkit.primitives.chevron import chevron_rule
from panelkit.primitives.ghost import ghost, ghost_separator


def test_chevron_dips_at_centre(theme):
    e = chevron_rule(20, 8, 10, theme)
    pts = [tuple(map(float, p.split(","))) for p in e.attrib["points"].split()]
    assert len(pts) == 5
    ys = [p[1] for p in pts]
    assert max(ys) == ys[2]            # centre point is lowest (largest y)
    assert ys[0] == ys[-1] == 10       # ends level


def test_ghost_has_body_and_two_eyes(theme):
    xml = ghost(20, 20, 4.0, theme.color("label"), theme.color("bg_bot")).xml()
    assert sum(1 for e in xml.iter() if e.tag == "path") == 1
    assert sum(1 for e in xml.iter() if e.tag == "circle") == 2


def test_ghost_separator_has_flanking_rules(theme):
    xml = ghost_separator(20, 26, theme).xml()
    assert sum(1 for e in xml.iter() if e.tag == "line") == 2
