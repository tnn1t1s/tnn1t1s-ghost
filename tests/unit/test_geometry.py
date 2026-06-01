import pytest
from panelkit.geometry import Vec, Box, fencepost, cell_centers, circle_gap


def test_fencepost():
    assert fencepost(0, 4, 10) == [0, 10, 20, 30]


def test_cell_centers_symmetric():
    c = cell_centers(0, 60, 3)
    assert c == pytest.approx([10, 30, 50])
    # symmetric about the midpoint
    mid = 30
    assert c[0] - 0 == pytest.approx(60 - c[-1])


def test_cell_centers_empty():
    assert cell_centers(0, 10, 0) == []


def test_circle_gap():
    a, b = Vec(0, 0), Vec(10, 0)
    assert circle_gap(a, 3, b, 3) == pytest.approx(4.0)      # 10 - 3 - 3
    assert circle_gap(a, 6, b, 6) == pytest.approx(-2.0)     # overlap


def test_box_center_and_contains():
    b = Box.from_center(Vec(10, 20), 4, 6)
    assert b.center == Vec(10, 20)
    assert b.w == 4 and b.h == 6
    assert b.contains(Vec(10, 20))
    assert not b.contains(Vec(13, 20))
    inner = Box.from_center(Vec(10, 20), 2, 2)
    assert b.contains_box(inner)
    assert not b.contains_box(Box.from_center(Vec(10, 20), 5, 5))
