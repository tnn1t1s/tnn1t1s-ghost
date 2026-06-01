"""Pure geometry helpers (millimetres). No I/O, no SVG, no config."""
from __future__ import annotations
import math
from dataclasses import dataclass


@dataclass(frozen=True)
class Vec:
    x: float
    y: float

    def __add__(self, o: "Vec") -> "Vec":
        return Vec(self.x + o.x, self.y + o.y)

    def __sub__(self, o: "Vec") -> "Vec":
        return Vec(self.x - o.x, self.y - o.y)

    def scale(self, k: float) -> "Vec":
        return Vec(self.x * k, self.y * k)

    def as_tuple(self) -> tuple[float, float]:
        return (self.x, self.y)


@dataclass(frozen=True)
class Box:
    """Axis-aligned box by its edges (mm)."""
    x0: float
    y0: float
    x1: float
    y1: float

    @classmethod
    def from_center(cls, c: Vec, w: float, h: float) -> "Box":
        return cls(c.x - w / 2, c.y - h / 2, c.x + w / 2, c.y + h / 2)

    @property
    def w(self) -> float:
        return self.x1 - self.x0

    @property
    def h(self) -> float:
        return self.y1 - self.y0

    @property
    def center(self) -> Vec:
        return Vec((self.x0 + self.x1) / 2, (self.y0 + self.y1) / 2)

    def contains(self, c: Vec, pad: float = 0.0) -> bool:
        return (self.x0 - pad <= c.x <= self.x1 + pad
                and self.y0 - pad <= c.y <= self.y1 + pad)

    def contains_box(self, b: "Box", pad: float = 1e-6) -> bool:
        return (b.x0 >= self.x0 - pad and b.y0 >= self.y0 - pad
                and b.x1 <= self.x1 + pad and b.y1 <= self.y1 + pad)


def fencepost(start: float, count: int, pitch: float) -> list[float]:
    """count positions starting at `start`, spaced by `pitch`."""
    return [start + i * pitch for i in range(count)]


def cell_centers(lo: float, hi: float, n: int) -> list[float]:
    """n cell-centred positions across [lo, hi] (symmetric, inset by half a cell)."""
    if n <= 0:
        return []
    return [lo + (hi - lo) * (i + 0.5) / n for i in range(n)]


def grouped_cell_centers(lo: float, hi: float, group_sizes: list[int],
                         gap: float = 0.6) -> list[list[float]]:
    """Cell centres across [lo, hi] for several groups, with `gap` extra
    slot-widths inserted between groups. Returns one list of centres per group.
    Reduces to cell_centers() spacing when there is a single group."""
    sizes = [s for s in group_sizes if s > 0]
    if not sizes:
        return []
    total_slots = sum(sizes) + gap * (len(sizes) - 1)
    slot = (hi - lo) / total_slots
    out: list[list[float]] = []
    pos = 0.0
    for gsize in sizes:
        out.append([lo + (pos + i + 0.5) * slot for i in range(gsize)])
        pos += gsize + gap
    return out


def circle_gap(a: Vec, ra: float, b: Vec, rb: float) -> float:
    """Edge-to-edge gap between two circles (negative = overlap)."""
    return math.hypot(a.x - b.x, a.y - b.y) - ra - rb
