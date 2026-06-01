from panelkit.components import Control, control
from panelkit.geometry import Vec


def test_control_has_named_anchor_and_path_label_no_text(footprints, theme, fontbook):
    c = Control("param.main.scale", "dial", Vec(10, 30), footprints["RoundBlackKnob"], "SCALE")
    xml = control(c, theme, fontbook, label_gap_mm=4.2).xml()
    ids = [e.get("id") for e in xml.iter() if e.get("id")]
    assert "param.main.scale" in ids
    # labels are vector paths, never <text> (NanoSVG ignores <text>)
    assert not any(e.tag.endswith("text") for e in xml.iter())
    assert any(e.tag == "path" for e in xml.iter())


def test_jack_control_no_label_ok(footprints, theme, fontbook):
    c = Control("out.main.audio", "jack", Vec(10, 30), footprints["PJ301MPort"])
    xml = control(c, theme, fontbook, label_gap_mm=4.2).xml()
    assert "out.main.audio" in [e.get("id") for e in xml.iter() if e.get("id")]
