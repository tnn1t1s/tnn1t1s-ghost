# GHOST calibration & provenance

How the GHOST voices were tuned, and against what. This is the single home for
the **cross-cutting** calibration stories (per style guide §9) — the inline code
comments point here so the reasoning survives refactors.

## Reference & provenance

GHOST is *inspired by* classic 909 behavior, not a clone (see ADR 0002). All
embedded PCM (hats, cymbals, rim, clap) was recorded by the author from a
909-style hardware unit the author owns and has used on released records — "the
reference machine" throughout. The synthesized voices (kick, snare, toms) were
calibrated by ear and against measurements of that same unit. No third-party
sample libraries or ROM dumps are used.

## Kick (`src/Kck.cpp`, `KckFit::Config`)

A clipped/saturated-triangle body through a resonant 2-pole SVF whose cutoff
tracks a single swept pitch (the "rez" zap), plus a filtered click. The shipped
default is **the reference match**: it lands at **TUNE = 100% with every shaping
knob at noon**.

Calibrated values (1/tau where noted):
- **Pitch**: `basePitchOffset 38 Hz` (TUNE 0) → `basePitchSpan 30.48` so TUNE 1 = **68.48 Hz** (matched). Sweep is a *ratio* of the fundamental (`sweepRatio 4.0`), widening as you tune up.
- **Body envelope**: `ampDecayMin 26 / ampDecaySpan 16.4` → at DECAY 0.5, 1/tau ≈ 17.8 (≈ −20 dB at 170 ms, matched).
- **Body**: clipped triangle, `bodyClip 2.4`, `bodyFundGain 0.696`, resonant SVF `bodyReso 0.62`, `bodyFcMult 1.30`. `driveBase 1.55`.
- **Click**: brief filtered-noise burst (`clickRateBase 140`), tonal chirp off by default.

**Knob → match remapping** (so noon = the matched values): CLICK/DRIVE are
remapped so noon hits the matched amounts; ATTACK/TONE macro the click sharpness
and body brightness so noon → matched `clickRate 140` / `bodyFcMult 1.30`.

A handful of body/sub/HP coefficients are inherited from the original JUCE
generator and kept by ear (`bodyHarmGain 0.19`, `subGain 0.36`, `hpCoef 0.0012`).

## Toms (`src/Toms.cpp`, `TomFit::Config`)

Three damped sines at ratios **1 : 1.5 : 2.77** plus a short white-noise burst
(the "little noise" the body alone lacks). Per-voice difference is only `baseHz`;
everything else shares defaults. Calibrated against the reference LowTom at
tune 0.5 / decay 0.5: **tau ≈ 100 ms**. `tuneOffset 0.62 / tuneSpan 0.88`,
`pitchBendRate 16`, `noiseGain 0.06` / `noiseDecayRate 28` (~36 ms burst).

## Snare (`src/Snr.cpp`)

Faithful analog topology rather than a sample: two reset triangle VCOs (the
body) + filtered binary noise, with split body/noise envelopes. Tuned by ear.

## Accent level ladders

Set in code (`Accent::` in `src/GhostBus.hpp`); see `doc/accent.md` for the model.
- **Kick** — wide ladder: ghost 15% / Accent B 60% / Accent A 100% / both 100%.
  GHOST CTRL's **RANGE** switch scales the ghost floor: Tight ~35% / Classic 15% / Wide ~8%.
- **Other voices** — gentle: un-accented at normal level, accented ≈ +3 dB.

## Sample sources (reference-faithful)

Crash, ride, open hat, closed hat, rim, and clap play embedded PCM captured from
the reference machine (6-bit-era character preserved). Bass, snare, and toms are
synthesized (the reference unit's analog voices).
