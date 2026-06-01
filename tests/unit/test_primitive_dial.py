import math
import pytest
from panelkit.geometry import Vec
from panelkit.primitives.dial import dial, outer_radius


def _iter(elem):
    return list(elem.xml().iter())


def _circles(elem):
    return [e for e in _iter(elem) if e.tag == "circle"]


def _lines(elem):
    return [e for e in _iter(elem) if e.tag == "line"]


def test_ring_diameter_matches_footprint(footprints, theme):
    fp = footprints["RoundBlackKnob"]
    g = dial(Vec(20, 30), fp, theme)
    ring = next(c for c in _circles(g) if c.get("stroke") == theme.color("rule"))
    assert 2 * float(ring.get("r")) == pytest.approx(fp.visual_d_mm, abs=0.05)
    assert float(ring.get("cx")) == pytest.approx(20)
    assert float(ring.get("cy")) == pytest.approx(30)


def test_white_knob_face(footprints, theme):
    fp = footprints["RoundBlackKnob"]
    g = dial(Vec(20, 30), fp, theme)
    face = next(c for c in _circles(g) if c.get("fill") == theme.color("knob_face"))
    assert float(face.get("r")) == pytest.approx(fp.visual_r, abs=0.05)
    assert face.get("stroke") in (None, "")


def test_two_tier_ticks(footprints, theme):
    g = dial(Vec(0, 0), footprints["RoundBlackKnob"], theme)
    div = theme.motifs["divider"]
    lines = _lines(g)
    n = round(div["sweep_deg"] / div["minor_step_deg"]) + 1
    minor = [ln for ln in lines if ln.get("stroke") == theme.color(div["minor"]["color"])]
    major = [ln for ln in lines if ln.get("stroke") == theme.color(div["major"]["color"])]
    cardinal = [ln for ln in lines if ln.get("stroke") == theme.color(div["cardinal"]["color"])]
    assert len(cardinal) == len(div["cardinal"]["degs"])   # 9/12/3 o'clock
    assert len(minor) + len(major) + len(cardinal) == n
    assert len(minor) > len(major) > 0                     # minor grid denser than majors


def test_art_within_outer_radius(footprints, theme):
    fp = footprints["RoundBlackKnob"]
    c = Vec(20, 30)
    bound = outer_radius(fp, theme) + theme.strokes.ring_mm
    for ln in _lines(dial(c, fp, theme)):
        for (ax, ay) in (("x1", "y1"), ("x2", "y2")):
            d = math.hypot(float(ln.get(ax)) - c.x, float(ln.get(ay)) - c.y)
            assert d <= bound + 1e-6
