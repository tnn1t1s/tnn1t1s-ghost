# Changelog

## 2.1.0 (2026-06-01)

- **GHOST MIX** — new dedicated 12-input kit summing mixer: one labeled input
  per voice of the full kit (Kick, Snare, Rim, Clap, Tom Lo/Mid/Hi, CHH, OHH,
  Crash, Ride) plus a MIXIN channel (the 909's external mix-in), mute switch
  each, unity sum to one MIX output.
- **Snare is now a first-class kit voice** — GHOST SNR gets its own mixer channel
  and sequencer lane, so it can be step-programmed (rolls, breakbeat figures)
  rather than sitting outside the kit. Demos now include a backbeat snare where
  it fits the groove (detroit, hypnotic, electro-88).
- **GHOST CTRL RANGE switch** — Tight / Classic / Wide dynamic-range selector
  that scales the kick's ghost floor (the 909 hardware mod, in a switch).
- **Kick** rebuilt as an authentic 909 engine and calibrated by ear; wide,
  pronounced accent ladder (ghost 15% / local 60% / global 100%).
- Accent model DRY'd into a shared `Accent::` policy; un-accented hits never
  jump from silence (additive-over-ghost gain).
- Example patches: `patches/demos/` (8 classic-909 grooves + a multi-level
  kick study) and `patches/lessons/` (build-it-up series).
- Removed the `KckLab` tuning bench from the browser (kick calibration done).

## 2.0.0 (2026-05-30)

First release of the TNN1T1S Ghost drum system.

Modules:

- GHOST CTRL — global state controller (DEFAULT / ACCENT A / ACCENT B / MASTER)
- GHOST KCK / GHOST KCK LAB — kick voice
- GHOST SNR / GHOST SNR LAB — snare voice
- GHOST OHCH / GHOST OHCH LAB — coupled open/closed hi-hat
- GHOST RIMCLAP / GHOST RIMCLAP LAB — rim + clap ROMpler voice
- GHOST TOMS / GHOST TOM LAB — low/mid/high tom kit
- GHOST CRSHRIDE / GHOST CRSHRIDE LAB — crash + ride cymbal pair
