#!/usr/bin/env python3
"""Build the classic-909 demo patches — one self-contained loop per style.

Each demo is the full GHOST kit through GHOST MIX, on the canonical Hora lane
routing (see build_lessons.py / reference_ghost_hora_routing), with a
genre-appropriate tempo on the clock. Patterns are grounded in documented 909
breakdowns (Attack, Aulart, drum-patterns.com, filter house / minimal techno refs).

Grids are written in 1-indexed 16-step form and repeated across the 2-bar
(32-step) Hora page. Clock TEMPO = log2(BPM/60) (verified for SlimeChild Clock).

NOTE (swing): #chicago/#revolution/#minimal are notated straight here. The Hora
has no exposed shuffle param in its data, so swing is a TODO (probe the Hora
shuffle param or apply it in Rack); the straight versions are still recognizable.
"""
import math
import os

import build_lessons as L   # reuse load_base/assemble_gmix/constants/helpers

DEMODIR = os.path.join(L.REPO, "patches/909-demos")


def bars(steps16):
    """1-indexed 16-step list -> 0-indexed, repeated across both bars (32 steps)."""
    z = [s - 1 for s in steps16]
    return z + [s + 16 for s in z]


# Per-voice grids (1-indexed, 16-step). Track constants come from build_lessons.
DEMOS = {
    "01-house": dict(bpm=122, grid={
        L.KICK_TRACK:   [1, 5, 9, 13],
        L.CLAP_TRACK:   [5, 13],
        L.CHH_TRACK:    [1, 5, 9, 13],
        L.OHH_TRACK:    [3, 7, 11, 15],
        L.ACCENT_TRACK: [1],
    }),
    "02-detroit": dict(bpm=130, grid={
        L.KICK_TRACK:   [1, 5, 9, 13],
        L.CLAP_TRACK:   [5, 13],
        L.CHH_TRACK:    [2, 4, 6, 8, 10, 12, 14, 16],
        L.OHH_TRACK:    [3, 7, 11, 15],
        L.RIM_TRACK:    [4, 12],
        L.ACCENT_TRACK: [1, 5, 9, 13],
    }),
    "03-chicago-jack": dict(bpm=120, grid={      # swing TODO
        L.KICK_TRACK:   [1, 5, 9, 13],
        L.CLAP_TRACK:   [5, 13],
        L.CHH_TRACK:    [1, 3, 5, 7, 9, 11, 13, 15],
        L.OHH_TRACK:    [3, 7, 11, 15],
        L.RIM_TRACK:    [7, 15],
        L.ACCENT_TRACK: [1],
    }),
    "04-hypnotic": dict(bpm=140, grid={
        L.KICK_TRACK:    [1, 5, 9, 13],
        L.RIDE_TRACK:    [1, 3, 5, 7, 9, 11, 13, 15],
        L.RIM_TRACK:     [5, 13],
        L.TOM_LO_TRACK:  [7, 15],
        L.OHH_TRACK:     [15],
        L.ACCENT_TRACK:  [1, 9],
    }),
    "05-tribal": dict(bpm=135, grid={
        L.KICK_TRACK:    [1, 5, 9, 13],
        L.TOM_LO_TRACK:  [3, 11],
        L.TOM_MID_TRACK: [7, 14],
        L.TOM_HI_TRACK:  [5, 16],
        L.RIDE_TRACK:    [1, 3, 5, 7, 9, 11, 13, 15],
        L.CHH_TRACK:     [1, 3, 5, 7, 9, 11, 13, 15],
        L.RIM_TRACK:     [6, 12],
        L.ACCENT_TRACK:  [1],
    }),
    "06-filter-house": dict(bpm=123, grid={    # filter house; swing TODO
        L.KICK_TRACK:    [1, 5, 9, 13],
        L.CLAP_TRACK:    [5, 13],
        L.CHH_TRACK:     [1, 2, 8, 9, 14, 16],
        L.OHH_TRACK:     [3, 7, 11, 15],
        L.RIM_TRACK:     [3, 7, 11, 15],
        L.TOM_MID_TRACK: [3, 10, 14],
        L.ACCENT_TRACK:  [1],
    }),
    "07-minimal": dict(bpm=130, grid={           # minimal techno; swing TODO
        L.KICK_TRACK:   [1, 5, 9, 13],
        L.RIM_TRACK:    [4, 7, 11, 14, 16],
        L.CHH_TRACK:    [3, 11],
        L.OHH_TRACK:    [15],
        L.CLAP_TRACK:   [13],
        L.ACCENT_TRACK: [1],
    }),
    "08-electro-88": dict(bpm=128, grid={        # electro / electro; broken kick
        L.KICK_TRACK:   [1, 2, 4, 7, 11, 15, 16],
        L.CLAP_TRACK:   [5, 13],
        L.CHH_TRACK:    [3, 6, 9, 13, 14],
        L.TOM_LO_TRACK: [8, 11, 15],
        L.ACCENT_TRACK: [1],
    }),
}


# The SlimeChild clock's doc formula (log2(BPM/60)) does NOT predict the musical
# tempo here -- with the donor's MULT=16 + Hora's clock division it's way off.
# Calibrated against two operator ear-points: TEMPO 1.222 -> ~50 BPM, TEMPO 1.811
# -> ~132 BPM. Those fit a line BPM = 139.2*TEMPO - 120 over the 120-140 band the
# demos live in. Invert to get TEMPO from a target BPM.
def tempo_for_bpm(bpm):
    return (bpm + 120.1) / 139.2

def set_tempo(data, bpm):
    clk = L.by_model(data, "SlimeChild-Substation-Clock")
    for p in clk["params"]:
        if p["id"] == 0:
            p["value"] = tempo_for_bpm(bpm)


def build(name, bpm, grid):
    data = L.load_base()
    L.drop_module(data, "Bogaudio-UMix")       # GHOST MIX replaces UMix
    L.add_toms(data)
    L.add_chhohh(data)
    L.add_crashride(data)
    L.add_ghostmix(data)
    L.reset_routing(data)
    set_tempo(data, bpm)
    patterns = {track: bars(steps) for track, steps in grid.items()}
    L.assemble_gmix(data, patterns)
    L.save_vcv(data, os.path.join(DEMODIR, f"{name}.vcv"))


if __name__ == "__main__":
    os.makedirs(DEMODIR, exist_ok=True)
    for name, spec in DEMOS.items():
        build(name, spec["bpm"], spec["grid"])
