"""Measured VCV component footprints (loaded from data/footprints.yaml)."""
from __future__ import annotations
import pathlib
from dataclasses import dataclass

from .loaders import build, load_yaml

DATA = pathlib.Path(__file__).resolve().parent.parent / "data" / "footprints.yaml"


@dataclass(frozen=True)
class Footprint:
    name: str
    visual_d_mm: float
    hitbox_d_mm: float
    keepout_r_mm: float = 0.0

    @property
    def visual_r(self) -> float:
        return self.visual_d_mm / 2.0

    @property
    def hitbox_r(self) -> float:
        return self.hitbox_d_mm / 2.0


def load_footprints(path: str | pathlib.Path = DATA) -> dict[str, Footprint]:
    doc = load_yaml(path)
    out: dict[str, Footprint] = {}
    for name, attrs in doc["footprints"].items():
        out[name] = build(Footprint, {"name": name, **attrs}, ctx="footprints: ")
    return out
