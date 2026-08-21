#!/usr/bin/env python3
"""Build tom-focused demo/test patches -- style-inspired grooves that put
GHOST TOMS front and centre, on the same Hora routing as build_demos.py.

These are NOT transcriptions of any specific recording. Each pattern is
either (a) a documented, citable technique from a production tutorial, or
(b) a technique characterization inspired by an artist's well-documented
programming style, clearly labeled as such below. No pattern claims to
reproduce a specific track.

Sourcing:
  - "fill_at_the_turn":      drum-patterns.com "Techno 10 Tom Fill" -- a
                             concrete, citable 16-step transcription (mid tom
                             on steps 11/12/15/16 of the second bar).
  - "accent_contrast":       Attack Magazine's TR-909 beginner guide
                             describes alternating full-strength/"WEAK" tom
                             hits for dynamic contrast -- built here as an
                             accented/ghost alternation, which doubles as a
                             direct listening test for the Toms accent fix.
  - "mills_toms_as_lead":    style-inspired by Jeff Mills' documented
                             approach of building whole passages from toms
                             plus ride/clap, skipping kick/snare entirely
                             (MusicTech interview). Not a transcription of
                             any Mills track.
  - "descending_roll":       a generic, unattributed techno/house fill shape
                             (high->mid->low roll into the next bar).

Explicitly NOT included: a "Carl Craig" pattern. No citable source was found
describing his tom technique specifically, so no pattern here claims that
attribution.
"""
import os

import build_demos as D
import build_lessons as L

DEMODIR = os.path.join(L.REPO, "patches/demos")


def bar32(bar1_1idx=(), bar2_1idx=()):
    """Two explicit 16-step (1-indexed) bars -> one 32-step 0-indexed list.
    Use when a pattern is deliberately asymmetric bar-to-bar (e.g. a fill
    that only lands in the second bar)."""
    b1 = [s - 1 for s in bar1_1idx]
    b2 = [s - 1 + 16 for s in bar2_1idx]
    return b1 + b2


PATCHES = {
    "toms-fill-at-the-turn": dict(bpm=128, desc=(
        "Steady kick/clap/hat groove; MID TOM fill lands only in the "
        "second bar on steps 11/12/15/16 -- per drum-patterns.com's "
        "'Techno 10 Tom Fill'."),
        build=lambda: {
            L.KICK_TRACK: D.bars([1, 5, 9, 13]),
            L.CLAP_TRACK: D.bars([5, 13]),
            L.CHH_TRACK:  D.bars([1, 3, 5, 7, 9, 11, 13, 15]),
            L.TOM_MID_TRACK: bar32(bar1_1idx=(), bar2_1idx=(11, 12, 15, 16)),
            L.ACCENT_TRACK: D.bars([1, 9]),
        }),

    "toms-accent-contrast": dict(bpm=126, desc=(
        "Steady LOW TOM fill alternating accented/ghost hits -- exercises "
        "the Toms accent swing directly (Attack Magazine's velocity-"
        "contrast fill technique, generalized to a listening test)."),
        build=lambda: {
            L.KICK_TRACK: D.bars([1, 5, 9, 13]),
            L.TOM_LO_TRACK: D.bars([2, 6, 10, 14]),
            # accent overlaps only every other tom hit (2, 10) -- 6 and 14
            # stay ghost, so the accented/unaccented contrast is audible
            # back-to-back within one bar.
            L.ACCENT_TRACK: D.bars([2, 10]),
        }),

    "toms-mills-lead": dict(bpm=130, desc=(
        "No kick or snare -- toms carry the groove with ride + light clap "
        "backbeat, style-inspired by Jeff Mills' documented approach of "
        "building whole passages from toms plus ride/clap alone. Not a "
        "transcription of any specific Mills track."),
        build=lambda: {
            L.TOM_LO_TRACK:  D.bars([1, 9]),
            L.TOM_MID_TRACK: D.bars([3, 11, 14, 16]),
            L.TOM_HI_TRACK:  D.bars([5, 7, 13, 15]),
            L.CLAP_TRACK:    D.bars([5, 13]),
            L.RIDE_TRACK:    D.bars([1, 3, 5, 7, 9, 11, 13, 15]),
            L.ACCENT_TRACK:  D.bars([1, 9]),
        }),

    "toms-descending-roll": dict(bpm=124, desc=(
        "Steady kick/clap/hat groove; HIGH->MID->LOW roll lands only in "
        "the second bar's last 4 steps -- a generic, unattributed techno/"
        "house fill shape, not tied to a specific artist."),
        build=lambda: {
            L.KICK_TRACK: D.bars([1, 5, 9, 13]),
            L.CLAP_TRACK: D.bars([5, 13]),
            L.CHH_TRACK:  D.bars([1, 3, 5, 7, 9, 11, 13, 15]),
            L.TOM_HI_TRACK:  bar32(bar1_1idx=(), bar2_1idx=(13,)),
            L.TOM_MID_TRACK: bar32(bar1_1idx=(), bar2_1idx=(14,)),
            L.TOM_LO_TRACK:  bar32(bar1_1idx=(), bar2_1idx=(15, 16)),
            L.ACCENT_TRACK: D.bars([1, 9]),
        }),
}


def build(name, bpm, pattern_fn):
    data = L.load_base()
    L.drop_module(data, "Bogaudio-UMix")       # GHOST MIX replaces UMix
    L.add_toms(data)
    L.add_snr(data)
    L.add_chhohh(data)
    L.add_crashride(data)
    L.add_ghostmix(data)
    L.reset_routing(data)
    D.set_tempo(data, bpm)
    L.assemble_gmix(data, pattern_fn())
    L.save_vcv(data, os.path.join(DEMODIR, f"{name}.vcv"))


if __name__ == "__main__":
    os.makedirs(DEMODIR, exist_ok=True)
    for name, spec in PATCHES.items():
        build(name, spec["bpm"], spec["build"])
        print(f"  {name}: {spec['desc']}")
