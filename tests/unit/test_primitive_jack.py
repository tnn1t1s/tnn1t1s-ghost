import pytest
from panelkit.geometry import Vec
from panelkit.primitives.jack import jack


def _circles(g):
    return [c for c in g.xml().iter() if c.tag == "circle"]


def test_jack_ring_diameter(footprints, theme):
    fp = footprints["PJ301MPort"]
    ring = next(c for c in _circles(jack(Vec(10, 10), fp, theme))
                if c.get("stroke") == theme.color("rule"))
    assert 2 * float(ring.get("r")) == pytest.approx(fp.visual_d_mm, abs=0.05)
    assert float(ring.get("cx")) == pytest.approx(10)


def test_jack_white_face(footprints, theme):
    fp = footprints["PJ301MPort"]
    face = next(c for c in _circles(jack(Vec(10, 10), fp, theme))
                if c.get("fill") == theme.color("knob_face"))
    assert float(face.get("r")) == pytest.approx(fp.visual_r, abs=0.05)
