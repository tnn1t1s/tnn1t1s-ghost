# 0003 — GHOST OHCH models one hi-hat playback system

**Status:** Accepted (2026-08-06)
**Supersedes:** [0001](0001-ohch-allows-ch-oh-layering.md)

## Context

GHOST OHCH modelled the closed and open hats as two parallel voices — two sample
sources, two address counters, two envelopes — plus rules layered on top to make
them behave like one: a choke flag that swapped the open hat onto a 5 ms release,
a coincidence window, per-voice age counters, and a mutual-exclusion setting.

Every added behavior meant another rule. Same-step exclusion needed a guard
window, which needed age counters, which needed the module to read both gates
before firing either, which needed the fire functions to report whether they had
been suppressed. The rules interacted, and the correct behavior was a property of
their conjunction rather than of the structure.

The hardware does not work that way. Per Roland's service documentation there is
one PCM hi-hat playback system: a single sample ROM, a common trigger that resets
the address counter, and a CLOSED/OPEN control signal that changes both the
sample-address behavior and the decay-envelope path.

## Decision

**Model the hardware: one sample ROM, one address counter, one envelope, and a
CLOSED/OPEN control line.** The control line selects playback rate, decay range,
air shelf, accent rail, and mix gain. `ChhOhhVoice::fire(open, char)` is the only
trigger entry point.

## Rationale

Every behavior that previously required a rule now follows from the structure:

- Closed and open cannot sound together — there is one voice.
- A closed hat over a ringing open hat replaces it with the closed-hat sound.
  That is the choke, and it is more faithful than the old 5 ms fade: the open hat
  stops because the address counter reset, not because something faded it out.
- Two triggers on one step resolve to whichever state the control line takes.
  The module drives it OPEN, so the open hat wins — a consequence of one
  assignment (`openMode = ohTrig`), not a priority rule.

Deleted with no replacement: `ohhChokeActive`, `chhChokeActive`,
`kChokeDecaySec`, `kEraseGuardSec`, `chhAgeSec`/`ohhAgeSec`, `eraseChh`/
`eraseOhh`, the same-step exclusion setting and its context menu, the
`dataToJson`/`dataFromJson` pair that persisted it, and the second PCM ROM.

The closed and open recordings were two independent captures (correlation 0.02
over the attack), so the closed hat now plays the same ROM as the open under the
fast decay — as the hardware makes it. This changes the closed-hat timbre; it is
the one audible consequence of this record and was accepted deliberately.

## Consequences

- Closed and open hi-hats can no longer be layered. This reverses 0001.
- The closed hat sounds different: same ROM as the open, shaped by decay.
- `src/embedded/GhostChhData.hpp` is deleted; the shipping binary drops ~140 kB.
- GHOST OHCH keeps two output jacks where the hardware has one, so GHOST MIX can
  treat closed and open as separate channels. The voice emits on the jack its
  control line selects; the vacated jack is ramped to silence over `kDeclickSec`
  (2 ms) so a mid-tail flip is a ramp rather than a step.
- `ChhOhhLab` now drives the shared voice instead of keeping its own copy of the
  DSP, so BITS is the only thing it adds.
