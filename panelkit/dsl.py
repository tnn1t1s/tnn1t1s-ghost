"""Panel DSL: a declarative *.panel.yaml describing a module's layout intent.

No coordinates — sections + controls only. A human edits this file (and the
theme/layout YAML for spacing) and re-renders with `panelkit render <Module>`;
no code or LLM needed.

Example:
    module: Attenuate
    name: GHOST ATTN
    hp: 10
    sections:
      - kind: header
      - kind: param_rows
        rows:
          - { label: SCALE, param: param.main.scale, cv: cv.main.scale }
      - kind: io_row
        cells:
          - { label: IN,  id: in.main.signal }
          - { label: OUT, id: out.main.signal }
"""
from __future__ import annotations
import pathlib
from dataclasses import dataclass, field

import yaml


@dataclass
class Row:
    label: str
    param: str
    cv: str | None = None


@dataclass
class Cell:
    label: str
    id: str
    sub: str | None = None
    mute: str | None = None    # mixer: anchor id for the channel's mute switch


@dataclass
class Section:
    kind: str
    rows: list[Row] = field(default_factory=list)
    cells: list[Cell] = field(default_factory=list)
    cell_groups: list = field(default_factory=list)   # io_row: list of cell-groups (triggers/accents/outputs)
    group_gap: float = 0.6                            # io_row: extra slot-widths between groups
    columns: int = 1
    groups: list = field(default_factory=list)   # param_grid: one title per column (e.g. CLAP, RIM)
    brand: bool = True          # header: show the TNN1T1S wordmark + chevron


@dataclass
class PanelSpec:
    module: str
    name: str
    hp: int
    sections: list[Section]


_FRAGMENTS_CACHE: dict | None = None


def _fragments() -> dict:
    """Shared section fragments (panelkit/specs/fragments.yaml), loaded once."""
    global _FRAGMENTS_CACHE
    if _FRAGMENTS_CACHE is None:
        pkg = pathlib.Path(__file__).resolve().parents[0]
        p = pkg / "specs" / "fragments.yaml"
        _FRAGMENTS_CACHE = yaml.safe_load(p.read_text()) if p.exists() else {}
    return _FRAGMENTS_CACHE


def _resolve(section: dict) -> dict:
    """Expand a `use: <name>` reference to a shared fragment; inline keys override."""
    if "use" not in section:
        return section
    name = section["use"]
    frag = _fragments().get(name)
    if frag is None:
        raise KeyError(f"unknown fragment {name!r}; known: {sorted(_fragments())}")
    merged = dict(frag)
    merged.update({k: v for k, v in section.items() if k != "use"})
    return merged


def load_panel_spec(path: str | pathlib.Path) -> PanelSpec:
    doc = yaml.safe_load(pathlib.Path(path).read_text())
    sections = []
    for raw in doc["sections"]:
        s = _resolve(raw)
        sections.append(Section(
            kind=s["kind"],
            rows=[Row(**r) for r in s.get("rows", s.get("params", []))],
            cells=[Cell(**c) for c in s.get("cells", [])],
            cell_groups=[[Cell(**c) for c in grp] for grp in s.get("groups", [])
                         if isinstance(grp, list)],
            group_gap=float(s.get("group_gap", 0.6)),
            columns=int(s.get("columns", 1)),
            groups=list(s.get("groups", [])) if s.get("kind") != "io_row" else [],
            brand=bool(s.get("brand", True)),
        ))
    return PanelSpec(doc["module"], doc["name"], int(doc["hp"]), sections)
