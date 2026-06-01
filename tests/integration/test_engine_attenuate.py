"""End-to-end: the declarative Attenuate spec compiles to a panel with the
expected bound anchors, vector-only labels, and a vertically-centred param row."""
import pytest
from panelkit.cli import ROOT
from panelkit.dsl import load_panel_spec
from panelkit.layout.engine import build_panel


def _all_ids(els):
    ids = set()
    for e in els:
        for n in e.xml().iter():
            if n.get("id"):
                ids.add(n.get("id"))
    return ids


def _find(els, eid):
    for e in els:
        for n in e.xml().iter():
            if n.get("id") == eid:
                return n
    return None


def test_attenuate_anchors_present(theme, layout, footprints, fontbook):
    spec = load_panel_spec(ROOT / "specs/panels/Attenuate.panel.yaml")
    els = build_panel(spec, theme, layout, footprints, fontbook)
    assert {"param.main.scale", "cv.main.scale",
            "in.main.signal", "out.main.signal"} <= _all_ids(els)


def test_attenuate_no_text_elements(theme, layout, footprints, fontbook):
    spec = load_panel_spec(ROOT / "specs/panels/Attenuate.panel.yaml")
    els = build_panel(spec, theme, layout, footprints, fontbook)
    assert all(not n.tag.endswith("text") for e in els for n in e.xml().iter())


def test_param_row_vertically_centred(theme, layout, footprints, fontbook):
    from panelkit import units
    from panelkit.components import header
    spec = load_panel_spec(ROOT / "specs/panels/Attenuate.panel.yaml")
    els = build_panel(spec, theme, layout, footprints, fontbook)
    # band starts below the (possibly collapsed) header — derive its bottom the
    # same way the engine does, honouring the header's brand flag.
    brand = next(s.brand for s in spec.sections if s.kind == "header")
    _, ghost_y = header(units.hp_to_mm(spec.hp), spec.name, theme, fontbook, layout, brand)
    band_top = ghost_y + layout.grid.param_band_pad_mm
    band_bot = layout.io.chevron_y_mm - layout.grid.param_band_pad_mm
    centre = (band_top + band_bot) / 2
    a = _find(els, "param.main.scale")
    cy = float(a.get("y")) + float(a.get("height")) / 2
    assert cy == pytest.approx(centre, abs=0.05)   # single row sits at band centre
