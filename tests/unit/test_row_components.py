"""Reusable layout components: the [dial, label, jack] param row and the
[label, jack, IN/OUT] I/O cell. Used by every module."""
from panelkit import units
from panelkit.components import control_row, io_cell


def _ids(elem):
    return [e.get("id") for e in elem.xml().iter() if e.get("id")]


def test_control_row_has_param_and_cv_anchors(footprints, theme, fontbook, layout):
    W = units.hp_to_mm(8)
    g = control_row(64.0, W, theme, fontbook, layout,
                    footprints["RoundBlackKnob"], footprints["PJ301MPort"],
                    "SCALE", "param.main.scale", "cv.main.scale")
    ids = _ids(g)
    assert "param.main.scale" in ids and "cv.main.scale" in ids
    assert not any(e.tag.endswith("text") for e in g.xml().iter())


def test_control_row_dial_left_of_jack_no_overlap(footprints, theme, fontbook, layout):
    W = units.hp_to_mm(8)
    g = control_row(64.0, W, theme, fontbook, layout,
                    footprints["RoundBlackKnob"], footprints["PJ301MPort"],
                    "SCALE", "param.main.scale", "cv.main.scale")
    rects = {e.get("id"): e for e in g.xml().iter() if e.get("id")}
    px = float(rects["param.main.scale"].get("x"))
    jx = float(rects["cv.main.scale"].get("x"))
    assert px < jx   # dial left of jack


def test_io_cell_has_anchor_and_one_label(footprints, theme, fontbook):
    g = io_cell(20.0, 111.0, theme, fontbook, footprints["PJ301MPort"],
                "IN", "in.main.signal", label_gap_mm=4.2)
    assert "in.main.signal" in _ids(g)
    assert sum(1 for e in g.xml().iter() if e.tag == "path") == 1   # single label below jack
