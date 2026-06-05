#!/usr/bin/env python3
"""Multi-level kick study — ONE kick, four dynamic levels.

The GHOST kick has two accent inputs (TOTAL_ACC = Accent A, LOCAL_ACC = Accent B)
and an AccentMix that sets four level tiers: ghost (neither) -6 dB, Accent A
-1 dB, Accent B 0 dB, both +1.5 dB. Programming two accent lanes against the
trigger lane lets a single kick roll with dynamics -- the minimal-techno move.

This patch: clock -> Hora -> GHOST CTRL + GHOST KCK -> audio. Three Hora lanes:
  trigger (out4/track2), Accent A (out3/track1), Accent B (out5/track3).

Per-step tiers (16-step, repeated x2):
  step 1, 9    A+B  -> both   (+1.5 dB)  loudest anchors
  step 5, 13   B    ->  0 dB             backbeat
  step 7, 15   A    -> -1 dB             medium
  step 3,4,11,12  -  -> -6 dB (ghost)    quiet rolls
"""
import os
import build_lessons as L
from build_demos import set_tempo

OUT = os.path.join(L.REPO, "patches/demos/09-multi-level-kick.vcv")
BPM = 130

KCK_TRIG, KCK_LOCAL_ACC, KCK_TOTAL_ACC = 0, 10, 11
ACCENT_B_TRACK = 3                       # unused lane -> out5

# Operator's hand-programmed 2-bar pattern (0-indexed, 32-step), captured from
# the Rack autosave. Accent A (TOTAL_ACC) -> 100%, Accent B (LOCAL_ACC) -> 60%,
# un-accented trigger steps -> 30% ghost (per the kickMix level ladder).
TRIG_32  = [0, 1, 4, 7, 8, 12, 13, 15, 16, 18, 19, 20, 22, 24, 26, 27, 28, 30]
ACC_A_32 = [0, 4, 8, 12, 16, 22, 24, 30]
ACC_B_32 = [1, 5, 9, 13, 16, 20, 24, 28]


def main():
    data = L.load_base()
    L.drop_module(data, "RimClap")
    L.drop_module(data, "Bogaudio-UMix")
    L.reset_routing(data)
    set_tempo(data, BPM)

    hd = L.by_model(data, "Drumsequencer")["data"]
    h  = L.by_model(data, "Drumsequencer")["id"]
    k  = L.by_model(data, "Kck")["id"]
    au = L.by_model(data, "AudioInterface2")["id"]

    L.set_track(hd, L.KICK_TRACK,   TRIG_32)
    L.set_track(hd, L.ACCENT_TRACK, ACC_A_32)           # track1 -> out3 (Accent A)
    L.set_track(hd, ACCENT_B_TRACK, ACC_B_32)           # track3 -> out5 (Accent B)

    L.cable(data, h, L.out_of(L.KICK_TRACK),   k, KCK_TRIG,      "#c91847")
    L.cable(data, h, L.out_of(L.ACCENT_TRACK), k, KCK_TOTAL_ACC, "#f3b0c2")  # A
    L.cable(data, h, L.out_of(ACCENT_B_TRACK), k, KCK_LOCAL_ACC, "#9b59b6")  # B
    L.cable(data, k, 0, au, 0, "#d8d8d8")
    L.cable(data, k, 0, au, 1, "#d8d8d8")

    # CTRL: both accents up so all four tiers are active; master leaves headroom.
    ctrl = L.by_model(data, "GhostCtrl")
    for p in ctrl["params"]:
        p["value"] = {0: 1.0, 1: 1.0, 2: 0.5}[p["id"]]  # Accent A, Accent B, Master

    L.pack_ctrl_row(data)
    L.save_vcv(data, OUT)


if __name__ == "__main__":
    main()
