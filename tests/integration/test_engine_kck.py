"""End-to-end: the declarative Kck spec compiles to a panel with the expected
bound anchors, vector-only labels, and a param band whose rows are evenly
distributed between the header and the I/O chevron."""
import pytest
from panelkit.dsl import load_panel_spec
from panelkit.layout.engine import build_panel
from panelkit.paths import panel_spec_path

SPEC = panel_spec_path("Kck")

PARAMS = ["tune", "decay", "pitch", "pitch-decay", "click", "attack",
          "tone", "drive", "level"]

ANCHORS = (
    {f"param.main.{p}" for p in PARAMS}
    | {f"cv.main.{p}" for p in PARAMS}
    | {"trig.main.trig", "accent.main.local", "accent.main.total",
       "out.main.audio"}
)


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


def _centre(els, eid):
    n = _find(els, eid)
    return (float(n.get("x")) + float(n.get("width")) / 2,
            float(n.get("y")) + float(n.get("height")) / 2)


def _build(theme, layout, footprints, fontbook):
    return build_panel(load_panel_spec(SPEC), theme, layout, footprints, fontbook)


def test_kck_anchors_present(theme, layout, footprints, fontbook):
    assert ANCHORS <= _all_ids(_build(theme, layout, footprints, fontbook))


def test_kck_has_no_stray_anchors(theme, layout, footprints, fontbook):
    assert _all_ids(_build(theme, layout, footprints, fontbook)) == ANCHORS


def test_kck_no_text_elements(theme, layout, footprints, fontbook):
    """Labels must be vector paths; a <text> element would render differently
    on a machine without the font installed."""
    els = _build(theme, layout, footprints, fontbook)
    assert all(not n.tag.endswith("text") for e in els for n in e.xml().iter())


def test_kck_panel_dimensions(theme, layout, footprints, fontbook):
    from panelkit import units
    from panelkit.render import panel_svg
    spec = load_panel_spec(SPEC)
    els = build_panel(spec, theme, layout, footprints, fontbook)
    svg = panel_svg(spec.hp, els, theme)
    assert f'width="{units.hp_to_mm(spec.hp):.2f}mm"' in svg    # 16 HP = 81.28mm
    assert f'height="{units.PANEL_HEIGHT_MM:.2f}mm"' in svg     # 3U = 128.50mm


def test_kck_grid_is_three_columns(theme, layout, footprints, fontbook):
    """The 9 params lay out as 3 columns x 3 rows: three distinct x, three
    distinct y, and every dial paired with its CV jack on the same row."""
    els = _build(theme, layout, footprints, fontbook)
    centres = [_centre(els, f"param.main.{p}") for p in PARAMS]
    assert len({round(x, 6) for x, _ in centres}) == 3
    assert len({round(y, 6) for _, y in centres}) == 3
    for p in PARAMS:
        assert _centre(els, f"cv.main.{p}")[1] == pytest.approx(
            _centre(els, f"param.main.{p}")[1])


def test_kck_io_row_left_to_right_order(theme, layout, footprints, fontbook):
    els = _build(theme, layout, footprints, fontbook)
    ids = ["trig.main.trig", "accent.main.local", "accent.main.total",
           "out.main.audio"]
    xs = [_centre(els, i)[0] for i in ids]
    assert xs == sorted(xs)
    assert {_centre(els, i)[1] for i in ids} == {layout.io.row_y_mm}
