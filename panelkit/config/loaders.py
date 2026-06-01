"""Generic YAML -> dataclass loading with strict validation."""
from __future__ import annotations
import dataclasses
import pathlib
from typing import Any, Type, TypeVar

import yaml

T = TypeVar("T")


def load_yaml(path: str | pathlib.Path) -> dict:
    return yaml.safe_load(pathlib.Path(path).read_text())


def build(cls: Type[T], data: dict, ctx: str = "") -> T:
    """Construct a (flat) dataclass, rejecting unknown and missing keys."""
    fields = {f.name: f for f in dataclasses.fields(cls)}
    unknown = set(data) - set(fields)
    if unknown:
        raise ValueError(f"{ctx}{cls.__name__}: unknown keys {sorted(unknown)}")
    kwargs: dict[str, Any] = {}
    for name, f in fields.items():
        if name in data:
            kwargs[name] = data[name]
        elif f.default is dataclasses.MISSING and f.default_factory is dataclasses.MISSING:
            raise ValueError(f"{ctx}{cls.__name__}: missing required key {name!r}")
    return cls(**kwargs)


def deep_merge(base: dict, override: dict) -> dict:
    out = dict(base)
    for k, v in override.items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = deep_merge(out[k], v)
        else:
            out[k] = v
    return out
