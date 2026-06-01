"""FontBook: resolve theme font roles to loaded CmapFonts (cached by file)."""
from __future__ import annotations
from dataclasses import dataclass

from ..config.theme import Theme
from .cmapfont import CmapFont


@dataclass(frozen=True)
class RoleFont:
    font: CmapFont
    size_pt: float
    tracking: float


class FontBook:
    def __init__(self, theme: Theme) -> None:
        self._theme = theme
        self._by_file: dict[str, CmapFont] = {}
        self._roles: dict[str, RoleFont] = {}
        for role, fr in theme.fonts.items():
            path = str(fr.abs_path())
            if path not in self._by_file:
                self._by_file[path] = CmapFont(path)
            self._roles[role] = RoleFont(self._by_file[path], fr.size_pt, fr.tracking)

    def role(self, name: str) -> RoleFont:
        if name not in self._roles:
            raise KeyError(f"theme {self._theme.name!r} has no font role {name!r}")
        return self._roles[name]
