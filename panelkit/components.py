"""Component layer: a Control composes primitives (art + label + named anchor)
at a resolved center. Pure; positions are decided by the caller/layout engine.
"""
from __future__ import annotations
from dataclasses import dataclass

from .config.footprints import Footprint
from .config.theme import Theme
from .geometry import Vec
from .primitives.anchor import anchor
from .primitives.base import group
from .primitives.chevron import chevron_rule
from .primitives.dial import dial, outer_radius
from .primitives.ghost import ghost_separator
from .primitives.jack import jack
from .primitives.label import label
from .text.fonts import FontBook


@dataclass(frozen=True)
class Control:
    eid: str                 # SvgHelper binding id, e.g. "param.main.scale"
    shape: str               # "dial" | "jack"
    center: Vec
    footprint: Footprint
    text: str = ""           # label above the control


def art_radius(c: Control, theme: Theme) -> float:
    """Outermost art extent of a control (mm) — for label placement / bounds."""
    return outer_radius(c.footprint, theme) if c.shape == "dial" else c.footprint.visual_r


def control(c: Control, theme: Theme, fontbook: FontBook, label_gap_mm: float):
    g = group()
    art = dial if c.shape == "dial" else jack
    g.append(art(c.center, c.footprint, theme))
    if c.text:
        ly = c.center.y - art_radius(c, theme) - label_gap_mm
        g.append(label(c.center.x, ly, c.text, fontbook.role("label"), theme.color("label")))
    g.append(anchor(c.center, c.eid))
    return g


def control_row(y, panel_w, theme, fontbook, layout, knob_fp, jack_fp,
                text, param_id, cv_id=None, pad_mm=1.5):
    """Reusable param row: [dial | label | jack] across the panel at height y.
    Dial left, label centred in the gap, optional CV jack right."""
    g = group()
    side = layout.margins.side_mm
    kr = outer_radius(knob_fp, theme)      # dial+ticks extent (keeps ticks off the edge)
    vr = knob_fp.visual_r                  # ring edge (label may sit over the thin tick zone)
    jr = jack_fp.visual_r
    dial_cx = side + kr + pad_mm
    gap_lo = dial_cx + vr + pad_mm
    jack_cx = panel_w - side - jr - pad_mm
    gap_hi = (jack_cx - jr - pad_mm) if cv_id else (panel_w - side)
    label_cx = (gap_lo + gap_hi) / 2
    # auto-fit the label into the available gap (no overlap)
    rf = fontbook.role("label")
    avail = gap_hi - gap_lo
    w = rf.font.measure(text, rf.size_pt)[0]
    size_pt = rf.size_pt if w <= avail else rf.size_pt * avail / w
    g.append(dial(Vec(dial_cx, y), knob_fp, theme))
    g.append(anchor(Vec(dial_cx, y), param_id))
    g.append(label(label_cx, y, text, rf, theme.color("label"), size_pt=size_pt))
    if cv_id:
        g.append(jack(Vec(jack_cx, y), jack_fp, theme))
        g.append(anchor(Vec(jack_cx, y), cv_id))
    return g


def header(panel_w, name, theme, fontbook, layout, show_brand=True):
    """Reusable header: optional brand wordmark + chevron, then module name and
    ghost separator. When show_brand is False the wordmark/chevron are omitted and
    the name+ghost shift up to reclaim the space. Returns (elements, ghost_y) so
    the caller can start the param band right below the separator."""
    cx = panel_w / 2
    h = layout.header
    chev_half = theme.motifs["chevron"]["span_frac"] * panel_w / 2
    ghost_span = theme.motifs["ghost"]["span_frac"] * panel_w
    name_pt = fontbook.role("brand").size_pt * theme.motifs["name_scale"]
    els = []
    if show_brand:
        name_y, ghost_y = h.name_y_mm, h.ghost_y_mm
        els.append(label(cx, h.brand_y_mm, theme.brand, fontbook.role("brand"), theme.color("label")))
        els.append(chevron_rule(cx, chev_half, h.rule_y_mm, theme))
    else:
        name_y = h.collapsed_name_y_mm               # small top margin
        ghost_y = name_y + (h.ghost_y_mm - h.name_y_mm)   # keep name->ghost gap
    els.append(label(cx, name_y, name, fontbook.role("title"), theme.color("pewter"), size_pt=name_pt))
    els.append(ghost_separator(cx, ghost_y, theme, span_mm=ghost_span))
    return els, ghost_y


def mixer_cell(colcx, y, jack_fp, theme, fontbook, layout, text, in_id, mute_id,
               jack_dx=4.0, mute_dx=4.5):
    """Mixer channel strip: instrument label above an [input jack | mute switch]
    pair, centred on `colcx` at height y. The mute switch is a live CKSS widget
    that draws itself, so only an anchor is emitted for it (no painted art)."""
    g = group()
    jr = jack_fp.visual_r
    jack_c = Vec(colcx - jack_dx, y)
    mute_c = Vec(colcx + mute_dx, y)
    g.append(jack(jack_c, jack_fp, theme))
    g.append(anchor(jack_c, in_id))
    g.append(anchor(mute_c, mute_id))
    # Label ABOVE the jack with the same chevron rule the knob/param cells use, so
    # the mixer reads as part of the series and each label clearly points down at
    # its jack (the chevron ties label -> port, removing between-rows ambiguity).
    ly = y - jr - layout.grid.label_clearance_mm
    g.append(label(jack_c.x, ly, text, fontbook.role("label"), theme.color("label")))
    g.append(chevron_rule(jack_c.x, jr * 1.5, ly + 1.6, theme,
                          color=theme.color("dim"), sw=theme.strokes.ring_mm))
    return g


def io_cell(cx, y, theme, fontbook, jack_fp, label_text, jack_id, label_gap_mm, sub=None):
    """Reusable I/O tuple. Labels stack *below* the jack so they never collide
    with the chevron rule above: [jack, role below, (voice sub below that)]."""
    g = group()
    jr = jack_fp.visual_r
    g.append(jack(Vec(cx, y), jack_fp, theme))
    g.append(anchor(Vec(cx, y), jack_id))
    rf = fontbook.role("label")
    line = rf.size_pt * 25.4 / 72.0          # one text line, mm
    g.append(label(cx, y + jr + label_gap_mm, label_text, rf, theme.color("label")))
    if sub:
        g.append(label(cx, y + jr + label_gap_mm + line, sub, rf, theme.color("dim")))
    return g


def pair_width(knob_fp, jack_fp, theme, gap_mm=1.5):
    """Width (mm) of a [dial + gap + CV jack] cell unit."""
    return 2 * outer_radius(knob_fp, theme) + gap_mm + 2 * jack_fp.visual_r


def param_cell(colcx, y, knob_fp, jack_fp, theme, fontbook, layout,
               text, param_id, cv_id, gap_mm=1.5):
    """Reusable grid cell: [label above] / [dial + small CV jack to its right],
    centred on `colcx` at height y."""
    g = group()
    ko = outer_radius(knob_fp, theme)      # knob+ticks radius
    jr = jack_fp.visual_r
    unit_w = pair_width(knob_fp, jack_fp, theme, gap_mm)
    left = colcx - unit_w / 2
    knob_cx = left + ko
    jack_cx = left + unit_w - jr
    pair_cx = (knob_cx + jack_cx) / 2     # geometric centre of the dial+jack pair
    # label_align: "column" -> centred on the column (lines middle col up with the
    # ghost); "pair" -> centred on the dial+jack pair.
    label_cx = colcx if layout.grid.label_align == "column" else pair_cx
    ly = y - ko - layout.grid.label_clearance_mm
    g.append(label(label_cx, ly, text, fontbook.role("label"), theme.color("label")))
    g.append(chevron_rule(label_cx, unit_w / 2 * 0.85, ly + 1.6, theme,
                          color=theme.color("dim"), sw=theme.strokes.ring_mm))
    g.append(dial(Vec(knob_cx, y), knob_fp, theme))
    g.append(anchor(Vec(knob_cx, y), param_id))
    g.append(jack(Vec(jack_cx, y), jack_fp, theme))
    g.append(anchor(Vec(jack_cx, y), cv_id))
    return g
