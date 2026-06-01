import xml.etree.ElementTree as ET
import pytest
from panelkit.geometry import Vec
from panelkit.primitives.dial import dial
from panelkit.primitives.anchor import anchor
from panelkit.render import panel_svg


def _by_id(root, eid):
    return next(e for e in root.iter() if e.get("id") == eid)


def test_one_dial_panel(footprints, theme):
    fp = footprints["RoundBlackKnob"]
    c = Vec(20.0, 40.0)
    svg = panel_svg(8, [dial(c, fp, theme), anchor(c, "param.main.tune")], theme)
    root = ET.fromstring(svg)

    assert root.get("width", "").endswith("mm")
    assert root.get("viewBox").startswith("0 0 40.64 128.5")

    a = _by_id(root, "param.main.tune")
    cx = float(a.get("x")) + float(a.get("width")) / 2
    cy = float(a.get("y")) + float(a.get("height")) / 2
    assert (cx, cy) == pytest.approx((20.0, 40.0))   # SvgHelper binds here
