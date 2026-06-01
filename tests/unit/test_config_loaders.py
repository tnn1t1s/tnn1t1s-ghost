import pytest
from panelkit.config.footprints import Footprint, load_footprints
from panelkit.config.loaders import build


def test_footprints_measured(footprints):
    assert footprints["RoundBlackKnob"].visual_d_mm == pytest.approx(9.60)
    assert footprints["PJ301MPort"].hitbox_d_mm == pytest.approx(9.00)
    assert footprints["Screw"].keepout_r_mm == pytest.approx(3.4)
    assert footprints["RoundBlackKnob"].visual_r == pytest.approx(4.80)


def test_build_rejects_unknown_key():
    with pytest.raises(ValueError, match="unknown keys"):
        build(Footprint, {"name": "X", "visual_d_mm": 1, "hitbox_d_mm": 1, "bogus": 2})


def test_build_rejects_missing_key():
    with pytest.raises(ValueError, match="missing required"):
        build(Footprint, {"name": "X", "visual_d_mm": 1})


def test_layout_loads_and_asserts_height(layout):
    assert layout.height_mm == pytest.approx(128.5)
    assert layout.grid.default_columns == 3
    assert layout.margins.side_mm == pytest.approx(5.5)   # ghost override applied


def test_theme_tokens_and_fonts(theme):
    assert theme.color("pewter").startswith("#")
    assert "label" in theme.fonts
    assert theme.fonts["title"].abs_path().exists()
    with pytest.raises(KeyError):
        theme.color("no_such_token")
