# Changelog

## Unreleased (2.1.3)

- **Toms attack rebuilt as the 909's noise burst** (#51). The burst is now a
  real transient: gain 0.06 → 0.30, band-limited by a 350 Hz high-pass (per
  Roland's s/n 426700 factory change), tightened to a ~9 ms spike. Accent
  drives the burst as it does the hardware's noise VCA. The synthetic click
  defaults to 0 — the 909 has no click feedthrough. Attack-to-body ratio
  +3.9 dB; body level unchanged.
- Plugin version reads 2.1.3 (module right-click → Info) so this build is
  identifiable against 2.1.2.

- **GHOST OHCH is now one hi-hat voice**, matching the hardware: one sample ROM,
  one address counter, one envelope, and a closed/open control that selects the
  decay and voicing path. Closed and open can no longer sound together. A closed
  hat over a ringing open hat replaces it (the choke); both on one step gives the
  open hat. None of this is configurable — it is what a single voice does.
  See doc/decisions/0003.
- **Closed hat timbre changed.** It now plays the same ROM as the open hat under
  the fast decay, as the hardware makes it, instead of a separate recording. The
  second ROM is gone and the plugin binary drops ~140 kB.
- **Closed hat mix gain raised 2.50 → 4.60** to match. Under the closed decay
  only the first few tens of ms of the ROM sound, which measured 5.3 dB below the
  old dedicated closed-hat recording; the makeup puts the default back where it
  was. Patches saved between these two changes will need CLOSED LEVEL turned back
  down. Applied post-drive, so it is level only and does not alter saturation.

## 2.1.2 (2026-06-20)

- **GHOST CTRL** — Accent A, Accent B, and Master now default to 100% (were
  50%). Applies to newly added modules; saved patches keep their stored values.
  (#33)

## 2.1.1 (2026-06-08)

- **Snare** — fixed ~170 Hz high-pass on the body to clear the kick/snare
  collision on the backbeat when the snare is tuned low. Transparent at center
  tune; self-cleans as you tune down. (#24)

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
