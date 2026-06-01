import pytest
from panelkit import units


def test_px_mm_roundtrip():
    assert units.px_to_mm(75) == pytest.approx(25.4)
    assert units.mm_to_px(5.08) == pytest.approx(15.0)
    assert units.px_to_mm(units.mm_to_px(12.34)) == pytest.approx(12.34, abs=1e-9)


def test_constants():
    assert units.HP_MM == 5.08
    assert units.PANEL_HEIGHT_MM == 128.5
    assert units.SVG_DPI == 75.0


def test_hp_to_mm():
    assert units.hp_to_mm(16) == pytest.approx(81.28)
