#!/usr/bin/env python3
"""Verify which Hora lane drives the snare BEFORE wiring it into the kit.

Minimal patch: Clock -> Hora, Hora out_of(SNR_TRACK) -> Snr TRIG, Snr -> audio.
Snare fires four-on-the-floor. If you hear an even snare pulse, the lane mapping
(track 3 -> out 5) is correct and we can build on it. If it's silent or on the
wrong beats, the convention is wrong for this lane and we pick another.

Nothing else is wired, so there's no ambiguity about what triggers the snare.
"""
import os
import build_lessons as L

data = L.load_base()
L.drop_module(data, "Kck")                      # keep it snare-only, no distractions

# add the Snr voice
data["modules"].append(dict(id=L.SNR_ID, plugin="tnn1t1s-ghost",
                            model="Snr", version="2.0.0", pos=[8, 0]))

L.reset_routing(data)                           # clock->Hora only; all gates zeroed

hd = L.by_model(data, "Drumsequencer")["data"]
h  = L.by_model(data, "Drumsequencer")["id"]
s  = L.SNR_ID
au = L.by_model(data, "AudioInterface2")["id"]

L.set_track(hd, L.SNR_TRACK, L.QUARTERS)        # snare on every quarter

# the lane under test -> snare trigger
L.cable(data, h, L.out_of(L.SNR_TRACK), s, L.SNR_TRIG, "#c91847")
# snare audio straight to the interface (no mixer needed for this test)
L.cable(data, s, L.SNR_OUT, au, 0, "#d8d8d8")
L.cable(data, s, L.SNR_OUT, au, 1, "#d8d8d8")

out = os.path.join(L.REPO, "patches/tests/snare-lane.vcv")
os.makedirs(os.path.dirname(out), exist_ok=True)
L.save_vcv(data, out)
print(f"SNR_TRACK={L.SNR_TRACK}  ->  Hora out {L.out_of(L.SNR_TRACK)}  ->  Snr TRIG")
