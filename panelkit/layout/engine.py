"""Layout engine: PanelSpec + configs -> list of SVG Elements.

This is the only place band geometry is decided. Sections declare intent;
the engine derives positions from the layout config (header band, param band,
I/O band) and the footprint registry, composing the reusable components. No
module-specific coordinates live anywhere.
"""
from __future__ import annotations
import math

from .. import units
from ..components import (control_row, header, io_cell, mixer_cell, pair_width,
                          param_cell)
from ..config.footprints import Footprint
from ..config.layout import LayoutConfig
from ..config.theme import Theme
from ..dsl import PanelSpec
from ..geometry import cell_centers, grouped_cell_centers
from ..primitives.chevron import chevron_rule
from ..primitives.dividers import dotted_vline
from ..primitives.label import label
from ..text.fonts import FontBook


def build_panel(spec: PanelSpec, theme: Theme, layout: LayoutConfig,
                footprints: dict[str, Footprint], fontbook: FontBook) -> list:
    W = units.hp_to_mm(spec.hp)
    cx = W / 2
    knob = footprints["RoundBlackKnob"]
    port = footprints["PJ301MPort"]
    chev_half = theme.motifs["chevron"]["span_frac"] * W / 2
    side = layout.margins.side_mm
    els: list = []
    header_bottom = layout.header.ghost_y_mm   # param band starts below the header

    for sec in spec.sections:
        if sec.kind == "header":
            hel, header_bottom = header(W, spec.name, theme, fontbook, layout, sec.brand)
            els += hel

        elif sec.kind == "param_rows":
            top = header_bottom + layout.grid.param_band_pad_mm
            bot = layout.io.chevron_y_mm - layout.grid.param_band_pad_mm
            for y, row in zip(cell_centers(top, bot, len(sec.rows)), sec.rows):
                els.append(control_row(y, W, theme, fontbook, layout, knob, port,
                                       row.label, row.param, row.cv))

        elif sec.kind == "param_grid":
            cols = sec.columns
            top = header_bottom + layout.grid.param_band_pad_mm
            bot = layout.io.chevron_y_mm - layout.grid.param_band_pad_mm
            # optional per-column group titles (CLAP / RIM ...) float in the
            # headroom above row 0 -- they do NOT shift the rows, so a grid with
            # group titles still aligns row-for-row with a plain grid beside it.
            title_y = top
            n_rows = math.ceil(len(sec.rows) / cols)
            # Place rows on a shared canonical pitch (top-anchored) so a 2-row
            # module aligns row-for-row with a 3-row module next to it -- the
            # short module just leaves its bottom row(s) empty, never centred.
            grid_rows = max(n_rows, layout.grid.canonical_rows)
            row_ys = cell_centers(top, bot, grid_rows)[:n_rows]
            # grid spans the header chevron width; outer pairs sit flush to its
            # ends (edge-justified), inner columns evenly between.
            gx0 = W / 2 - chev_half
            gx1 = W / 2 + chev_half
            uw = pair_width(knob, port, theme)
            if cols > 1:
                lo, hi = gx0 + uw / 2, gx1 - uw / 2
                col_cxs = [lo + (hi - lo) * i / (cols - 1) for i in range(cols)]
            else:
                col_cxs = [(gx0 + gx1) / 2]
            for c in range(1, cols):                      # dividers midway between pairs
                x = (col_cxs[c - 1] + col_cxs[c]) / 2
                els.append(dotted_vline(x, top - 2, bot + 2, theme.color("frame")))
            for c, title in enumerate(sec.groups[:cols]):
                els.append(label(col_cxs[c], title_y, title,
                                 fontbook.role("label"), theme.color("pewter")))
            for i, p in enumerate(sec.rows):
                r, c = divmod(i, cols)
                els.append(param_cell(col_cxs[c], row_ys[r], knob, port, theme,
                                      fontbook, layout, p.label, p.param, p.cv))

        elif sec.kind == "mixer":
            cols = sec.columns
            top = header_bottom + layout.grid.param_band_pad_mm
            bot = layout.io.chevron_y_mm - layout.grid.param_band_pad_mm
            n = len(sec.cells)
            n_rows = math.ceil(n / cols)
            row_ys = cell_centers(top, bot, n_rows)
            # column centres: a channel strip is ~16mm wide (jack + switch), so
            # inset the columns by half that to keep both inside the side margins.
            cw_half = 8.0
            lo, hi = side + cw_half, W - side - cw_half
            col_cxs = ([lo + (hi - lo) * i / (cols - 1) for i in range(cols)]
                       if cols > 1 else [W / 2])
            for c in range(1, cols):                      # dividers between columns
                x = (col_cxs[c - 1] + col_cxs[c]) / 2
                els.append(dotted_vline(x, top - 2, bot + 2, theme.color("frame")))
            for i, cell in enumerate(sec.cells):
                r, c = divmod(i, cols)
                els.append(mixer_cell(col_cxs[c], row_ys[r], port, theme, fontbook,
                                      layout, cell.label, cell.id, cell.mute))

        elif sec.kind == "io_row":
            els.append(chevron_rule(cx, chev_half, layout.io.chevron_y_mm, theme))
            # grouped (triggers / accents / outputs) with a wider gap between
            # groups, or a single evenly-spaced row of cells.
            if sec.cell_groups:
                groups = sec.cell_groups
                centres = grouped_cell_centers(side, W - side,
                                               [len(g) for g in groups], sec.group_gap)
                placed = [(x, c) for xs, g in zip(centres, groups) for x, c in zip(xs, g)]
            else:
                placed = list(zip(cell_centers(side, W - side, len(sec.cells)), sec.cells))
            for x, cell in placed:
                els.append(io_cell(x, layout.io.row_y_mm, theme, fontbook, port,
                                   cell.label, cell.id, layout.io.label_gap_mm,
                                   sub=cell.sub))

        else:
            raise ValueError(f"unknown section kind {sec.kind!r}")

    return els
