#!/usr/bin/env python3
"""Build the Detroit techno construction lesson series from one committed donor.

Source of truth is a committed patch (git HEAD:02-add-bassdrum.vcv) used only
for its module dicts + the known-good Hora baseline params/data. ALL routing
and gate patterns are constructed from scratch here, so the build never depends
on the volatile VCV autosave.

Wiring convention (every lesson):

  Hora lane (output id = track + 2) -> voice input

  | role           | track | out | -> voice input            |
  |----------------|-------|-----|---------------------------|
  | ACCENT (shared)|   1   |  3  | each voice's TOTAL_ACC    |
  | KICK trig      |   2   |  4  | Kck TRIG (in 0)           |
  | CLAP trig      |   7   |  9  | RimClap CLAP_TRIG (in 0)  |
  | RIM trig       |   8   | 10  | RimClap RIM_TRIG (in 1)   |

ACCENT is its own Hora lane patched into every voice's TOTAL_ACC. CTRL only
sets the accent *level* over its expander bus; it cannot pass a per-step
trigger (expander latency), so the trigger must come through Hora. CTRL's bus
also only reaches voices that are physically contiguous, so CTRL + every voice
present is packed edge-to-edge from x=0 (CTRL 8HP, Kck 16HP, RimClap 12HP).

Lessons (layers of the groove):
  01 rimshot + accent : rim metronome + downbeat accent
  02 add bassdrum     : + four-on-the-floor kick (accent into kick too)
  03 add claps        : + offbeat clap, with the full syncopated groove
"""
import json, os, subprocess, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LDIR = os.path.join(REPO, "patches/909-lessons")
DONOR_REF = "HEAD:patches/909-lessons/02-add-bassdrum.vcv"

# ---- routing convention ----
ACCENT_TRACK, KICK_TRACK, CLAP_TRACK, RIM_TRACK = 1, 2, 7, 8
# Toms use the Hora's tom lanes, mapped descending: out6=HIGH, out7=MID, out8=LOW
TOM_HI_TRACK, TOM_MID_TRACK, TOM_LO_TRACK = 4, 5, 6
# Hats on the Hora's hat lanes: out13=closed (CHH), out14=open (OHH)
CHH_TRACK, OHH_TRACK = 11, 12
def out_of(track): return track + 2          # Hora: output id = track + 2

# voice I/O ids
KCK_TRIG, KCK_TOTAL_ACC = 0, 11
RC_CLAP_TRIG, RC_RIM_TRIG, RC_TOTAL_ACC = 0, 1, 2
RC_CLAP_OUT, RC_RIM_OUT = 0, 1
TM_LO_TRIG, TM_MID_TRIG, TM_HI_TRIG, TM_TOTAL_ACC = 0, 1, 2, 4
TM_LO_OUT, TM_MID_OUT, TM_HI_OUT = 0, 1, 2
HH_CHH_TRIG, HH_OHH_TRIG, HH_TOTAL_ACC = 0, 1, 3
HH_CHH_OUT, HH_OHH_OUT = 0, 1
CLOCK_OUT, HORA_CLOCK_IN = 1, 2

TOMS_ID = 9101000000000007
CHHOHH_ID = 9101000000000008

# patterns (32-step page)
QUARTERS  = [0, 4, 8, 12, 16, 20, 24, 28]    # four-on-floor / metronome
DOWNBEATS = [0, 16]                          # one accent per bar
GROOVE = {                                    # the operator's hand-wired groove
    ACCENT_TRACK: [0, 4, 8, 12, 20, 28],
    KICK_TRACK:   [0, 4, 8, 12, 16, 20, 24, 28],
    CLAP_TRACK:   [4, 12],
    RIM_TRACK:    [0, 1, 7, 11, 12, 14],
}
# lesson 04 toms: the operator's hand-drawn three-tom groove (from autosave).
TOMS = {
    TOM_HI_TRACK:  [7, 10, 23, 28],
    TOM_MID_TRACK: [2, 13, 15],
    TOM_LO_TRACK:  [1, 6, 11, 15],
}
# lesson 05 hats: the operator's hand-drawn pattern (from autosave) -- a
# sparse closed-hat figure with one open-hat ring.
HATS = {
    CHH_TRACK: [2, 6, 10],
    OHH_TRACK: [14],
}

# top-row layout: CTRL + voices must be contiguous for the accent bus
ROW0_HP = {"Tr909Ctrl": 8, "Kck": 16, "RimClap": 12, "Toms": 20, "ChhOhh": 14}
ROW0_ORDER = ["Tr909Ctrl", "Kck", "RimClap", "Toms", "ChhOhh"]


def load_base():
    raw = subprocess.run(["git", "-C", REPO, "show", DONOR_REF],
                         check=True, stdout=subprocess.PIPE).stdout
    d = tempfile.mkdtemp()
    p = os.path.join(d, "donor.vcv")
    with open(p, "wb") as f:
        f.write(raw)
    subprocess.run(["tar", "--use-compress-program=unzstd", "-xf", p, "-C", d],
                   check=True)
    with open(os.path.join(d, "patch.json")) as f:
        return json.load(f)


def save_vcv(data, path):
    d = tempfile.mkdtemp()
    with open(os.path.join(d, "patch.json"), "w") as f:
        json.dump(data, f, indent=2)
    subprocess.run(["tar", "--use-compress-program=zstd", "-cf",
                    os.path.abspath(path), "-C", d, "./patch.json"], check=True)
    print(f"wrote {os.path.relpath(path, REPO)}")


def by_model(data, model):
    return next((m for m in data["modules"] if m["model"] == model), None)


def reset_routing(data):
    """Strip every Hora gate track and every cable except clock->Hora."""
    hd = by_model(data, "Drumsequencer")["data"]
    for t in range(1, 13):
        hd[f"gates{t}P1"] = [0] * 32
    clk, hora = by_model(data, "SlimeChild-Substation-Clock"), by_model(data, "Drumsequencer")
    data["cables"] = [dict(id=1, outputModuleId=clk["id"], outputId=CLOCK_OUT,
                           inputModuleId=hora["id"], inputId=HORA_CLOCK_IN,
                           color="#909c3e")]


def set_track(hd, track, steps):
    g = [0] * 32
    for s in steps:
        g[s] = 1
        hd[f"proba_level {s+1}_{track}"] = 1.0
    hd[f"gates{track}P1"] = g


def cable(data, om, oi, im, ii, color="#0c8e15"):
    nid = max((c["id"] for c in data["cables"]), default=0) + 1
    data["cables"].append(dict(id=nid, outputModuleId=om, outputId=oi,
                               inputModuleId=im, inputId=ii, color=color))


def drop_module(data, model):
    m = by_model(data, model)
    if not m:
        return
    mid = m["id"]
    data["modules"] = [x for x in data["modules"] if x["id"] != mid]
    data["cables"] = [c for c in data["cables"]
                      if c["outputModuleId"] != mid and c["inputModuleId"] != mid]


def pack_ctrl_row(data):
    x = 0
    for model in ROW0_ORDER:
        m = by_model(data, model)
        if not m:
            continue
        m["pos"] = [x, 0]
        x += ROW0_HP[model]


def add_toms(data):
    """Inject a GHOST Toms module on the contiguous CTRL row."""
    data["modules"].append(dict(id=TOMS_ID, plugin="tnn1t1s-ghost",
                                model="Toms", version="2.0.0", pos=[36, 0]))


def add_chhohh(data):
    """Inject a GHOST ChhOhh (hats) module on the contiguous CTRL row."""
    data["modules"].append(dict(id=CHHOHH_ID, plugin="tnn1t1s-ghost",
                                model="ChhOhh", version="2.0.0", pos=[56, 0]))


def assemble(data, patterns, has_kick, has_clap, has_toms=False, has_hats=False):
    hd = by_model(data, "Drumsequencer")["data"]
    h = by_model(data, "Drumsequencer")["id"]
    ctrl = by_model(data, "Tr909Ctrl")["id"]  # noqa: kept for clarity / future use
    rc = by_model(data, "RimClap")["id"]
    um = by_model(data, "Bogaudio-UMix")["id"]
    au = by_model(data, "AudioInterface2")["id"]

    for track, steps in patterns.items():
        set_track(hd, track, steps)

    # accent lane -> every present voice's TOTAL_ACC
    cable(data, h, out_of(ACCENT_TRACK), rc, RC_TOTAL_ACC, "#f3b0c2")
    # rim
    cable(data, h, out_of(RIM_TRACK), rc, RC_RIM_TRIG, "#3398dc")
    cable(data, rc, RC_RIM_OUT, um, 1)
    # clap
    if has_clap:
        cable(data, h, out_of(CLAP_TRACK), rc, RC_CLAP_TRIG, "#f9c130")
        cable(data, rc, RC_CLAP_OUT, um, 2)
    # kick
    if has_kick:
        k = by_model(data, "Kck")["id"]
        cable(data, h, out_of(KICK_TRACK), k, KCK_TRIG, "#c91847")
        cable(data, h, out_of(ACCENT_TRACK), k, KCK_TOTAL_ACC, "#f3b0c2")
        cable(data, k, 0, um, 0)
    # toms (three independent voices, shared accent)
    if has_toms:
        t = by_model(data, "Toms")["id"]
        cable(data, h, out_of(TOM_HI_TRACK),  t, TM_HI_TRIG,  "#7a4ec2")   # out6 -> HIGH
        cable(data, h, out_of(TOM_MID_TRACK), t, TM_MID_TRIG, "#7a4ec2")   # out7 -> MID
        cable(data, h, out_of(TOM_LO_TRACK),  t, TM_LO_TRIG,  "#7a4ec2")   # out8 -> LOW
        cable(data, h, out_of(ACCENT_TRACK),  t, TM_TOTAL_ACC, "#f3b0c2")
        cable(data, t, TM_LO_OUT,  um, 3)
        cable(data, t, TM_MID_OUT, um, 4)
        cable(data, t, TM_HI_OUT,  um, 5)
    # hats (closed + open, shared accent)
    if has_hats:
        hh = by_model(data, "ChhOhh")["id"]
        cable(data, h, out_of(CHH_TRACK), hh, HH_CHH_TRIG, "#2aa198")
        cable(data, h, out_of(OHH_TRACK), hh, HH_OHH_TRIG, "#2aa198")
        cable(data, h, out_of(ACCENT_TRACK), hh, HH_TOTAL_ACC, "#f3b0c2")
        cable(data, hh, HH_CHH_OUT, um, 6)
        cable(data, hh, HH_OHH_OUT, um, 7)
    # mix -> audio
    cable(data, um, 0, au, 0, "#d8d8d8")
    cable(data, um, 0, au, 1, "#d8d8d8")
    pack_ctrl_row(data)


def build_01():
    data = load_base()
    drop_module(data, "Kck")
    reset_routing(data)
    assemble(data, {RIM_TRACK: QUARTERS, ACCENT_TRACK: DOWNBEATS},
             has_kick=False, has_clap=False)
    save_vcv(data, os.path.join(LDIR, "01-rimshot-accent.vcv"))


def build_02():
    data = load_base()
    reset_routing(data)
    assemble(data, {RIM_TRACK: QUARTERS, KICK_TRACK: QUARTERS,
                    ACCENT_TRACK: DOWNBEATS},
             has_kick=True, has_clap=False)
    save_vcv(data, os.path.join(LDIR, "02-add-bassdrum.vcv"))


def build_03():
    data = load_base()
    reset_routing(data)
    assemble(data, dict(GROOVE), has_kick=True, has_clap=True)
    save_vcv(data, os.path.join(LDIR, "03-add-claps.vcv"))


def build_04():
    data = load_base()
    add_toms(data)
    reset_routing(data)
    patterns = dict(GROOVE)
    patterns.update(TOMS)
    assemble(data, patterns, has_kick=True, has_clap=True, has_toms=True)
    save_vcv(data, os.path.join(LDIR, "04-add-toms.vcv"))


def build_05():
    data = load_base()
    add_toms(data)
    add_chhohh(data)
    reset_routing(data)
    patterns = dict(GROOVE)
    patterns.update(TOMS)
    patterns.update(HATS)
    assemble(data, patterns, has_kick=True, has_clap=True,
             has_toms=True, has_hats=True)
    save_vcv(data, os.path.join(LDIR, "05-add-hats.vcv"))


if __name__ == "__main__":
    build_01()
    build_02()
    build_03()
    build_04()
    build_05()
