"""panelkit CLI — render a module's panel from its declarative spec.

    python -m panelkit render Attenuate

Reads panelkit/specs/panels/<Module>.panel.yaml + the Ghost theme/layout, writes
res/<Module>.svg and a preview PNG. A human edits the YAML and re-runs this;
no code changes, no LLM tokens.
"""
from __future__ import annotations
import argparse
import pathlib
import subprocess

from .config import load_footprints, load_layout, load_theme
from .dsl import load_panel_spec
from .layout.engine import build_panel
from .render import panel_svg
from .text import FontBook

ROOT = pathlib.Path(__file__).resolve().parent.parent


def render(module: str, preview: bool = True) -> pathlib.Path:
    theme = load_theme(ROOT / "panelkit/specs/themes/ghost.yaml")
    layout = load_layout(override_path=ROOT / "panelkit/specs/layout/ghost.yaml")
    footprints = load_footprints()
    fontbook = FontBook(theme)
    spec = load_panel_spec(ROOT / "panelkit/specs/panels" / f"{module}.panel.yaml")

    els = build_panel(spec, theme, layout, footprints, fontbook)
    svg = panel_svg(spec.hp, els, theme)
    out = ROOT / "res" / f"{module}.svg"
    out.write_text(svg)
    print(f"rendered {out.relative_to(ROOT)}")

    if preview:
        prev_dir = ROOT / "tools/_preview"
        prev_dir.mkdir(parents=True, exist_ok=True)
        prev = prev_dir / f"{module}.png"
        subprocess.run(["rsvg-convert", "-z", "10", "-o", str(prev), str(out)], check=True)
        print(f"preview  {prev.relative_to(ROOT)}")
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(prog="panelkit")
    sub = ap.add_subparsers(dest="cmd")
    r = sub.add_parser("render", help="render a module panel from its spec")
    r.add_argument("module")
    r.add_argument("--no-preview", action="store_true")
    args = ap.parse_args(argv)
    if args.cmd == "render":
        render(args.module, preview=not args.no_preview)
    else:
        ap.print_help()


if __name__ == "__main__":
    main()
