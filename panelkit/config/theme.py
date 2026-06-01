"""Theme config: appearance tokens (palette, fonts, strokes, motif params).

Decides APPEARANCE only; positions live in the layout config. Primitives must
reference palette tokens, never hex literals.
"""
from __future__ import annotations
import pathlib
from dataclasses import dataclass

from .loaders import build, load_yaml

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class FontRole:
    file: str            # path relative to repo root
    size_pt: float
    tracking: float = 0.0

    def abs_path(self) -> pathlib.Path:
        return REPO_ROOT / self.file


@dataclass(frozen=True)
class Strokes:
    ring_mm: float
    rule_mm: float
    frame_mm: float


@dataclass(frozen=True)
class Theme:
    name: str
    brand: str
    palette: dict
    fonts: dict          # role -> FontRole
    strokes: Strokes
    motifs: dict

    def color(self, token: str) -> str:
        if token not in self.palette:
            raise KeyError(f"theme {self.name!r} has no palette token {token!r}")
        return self.palette[token]


def load_theme(path: str | pathlib.Path) -> Theme:
    doc = load_yaml(path)
    fonts = {role: build(FontRole, spec, ctx=f"theme.fonts.{role}: ")
             for role, spec in doc["fonts"].items()}
    for role, fr in fonts.items():
        if not fr.abs_path().exists():
            raise FileNotFoundError(f"theme font {role!r}: {fr.abs_path()} not found")
    return Theme(
        name=doc["name"],
        brand=doc.get("brand", "TNN1T1S"),
        palette=doc["palette"],
        fonts=fonts,
        strokes=build(Strokes, doc["strokes"], ctx="theme.strokes: "),
        motifs=doc.get("motifs", {}),
    )
