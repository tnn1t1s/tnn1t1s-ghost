"""`use:` fragment includes keep panel specs DRY (shared header, etc.)."""
import pytest
from panelkit.cli import ROOT
from panelkit.dsl import load_panel_spec


def test_header_fragment_resolves():
    # both panels reference the shared `header` fragment, not an inline copy
    for module in ("Attenuate", "Kck"):
        spec = load_panel_spec(ROOT / "panelkit/specs/panels" / f"{module}.panel.yaml")
        hdr = next(s for s in spec.sections if s.kind == "header")
        assert hdr.brand is False        # comes from panelkit/specs/fragments.yaml


def test_unknown_fragment_raises(tmp_path):
    p = tmp_path / "Bad.panel.yaml"
    p.write_text("module: Bad\nname: BAD\nhp: 4\nsections:\n  - use: nope\n")
    with pytest.raises(KeyError, match="unknown fragment"):
        load_panel_spec(p)
