"""Layout policy config (positions only). Generic defaults + family overrides."""
from __future__ import annotations
import pathlib
from dataclasses import dataclass

from .. import units
from .loaders import build, deep_merge, load_yaml

DEFAULTS = pathlib.Path(__file__).resolve().parent.parent / "data" / "layout.defaults.yaml"


@dataclass(frozen=True)
class Margins:
    top_mm: float
    bottom_mm: float
    side_mm: float


@dataclass(frozen=True)
class Screws:
    rows_y_mm: list
    inset_mm: float
    keepout_r_mm: float


@dataclass(frozen=True)
class Bands:
    title_height_mm: float
    io_height_mm: float


@dataclass(frozen=True)
class Grid:
    default_columns: int
    min_gutter_mm: float
    label_clearance_mm: float
    cell_pad_mm: float
    param_band_pad_mm: float
    label_align: str = "column"   # "column" | "pair"
    canonical_rows: int = 3       # shared row pitch so 2x2 and 3x3 grids align row-for-row
    group_title_pad_mm: float = 6.0   # vertical band reserved for per-column group titles


@dataclass(frozen=True)
class Header:
    brand_y_mm: float
    rule_y_mm: float
    name_y_mm: float
    ghost_y_mm: float
    collapsed_name_y_mm: float = 5.0    # name baseline when brand is hidden


@dataclass(frozen=True)
class IO:
    chevron_y_mm: float
    row_y_mm: float
    label_gap_mm: float = 2.0   # io labels hug their jacks (smaller than param clearance)


@dataclass(frozen=True)
class LayoutConfig:
    height_mm: float
    margins: Margins
    screws: Screws
    bands: Bands
    grid: Grid
    header: Header
    io: IO


def load_layout(default_path: str | pathlib.Path = DEFAULTS,
                override_path: str | pathlib.Path | None = None) -> LayoutConfig:
    doc = load_yaml(default_path)
    if override_path:
        doc = deep_merge(doc, load_yaml(override_path))
    height = doc["panel"]["height_mm"]
    if abs(height - units.PANEL_HEIGHT_MM) > 1e-9:
        raise ValueError(f"layout panel.height_mm {height} != units.PANEL_HEIGHT_MM "
                         f"{units.PANEL_HEIGHT_MM}")
    return LayoutConfig(
        height_mm=height,
        margins=build(Margins, doc["margins"], ctx="layout.margins: "),
        screws=build(Screws, doc["screws"], ctx="layout.screws: "),
        bands=build(Bands, doc["bands"], ctx="layout.bands: "),
        grid=build(Grid, doc["grid"], ctx="layout.grid: "),
        header=build(Header, doc["header"], ctx="layout.header: "),
        io=build(IO, doc["io"], ctx="layout.io: "),
    )
