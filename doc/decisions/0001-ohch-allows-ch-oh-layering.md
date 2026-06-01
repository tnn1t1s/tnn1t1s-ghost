# 0001 — GHOST OHCH allows closed+open hi-hat on the same step

**Status:** Accepted (2026-06-01)

## Context

On a real TR-909 the closed and open hi-hats are one voice, and the *sequencer*
enforces mutual exclusion: programming a closed hat on a step **erases** any open
hat on that step (and vice versa). The exclusion happens at the pattern layer,
before the voice circuit is involved. The audible choke (a closed hat cutting a
ringing open hat) is a separate, sequential behavior.

GHOST OHCH is driven by an **external** sequencer (Hora, Impromptu, etc.). The
module never sees a "pattern" — it only receives two gates (CHH_TRIG, OHH_TRIG).
So it cannot reproduce the 909's pattern-level erase; it can only decide what to
do when both gates fire on the same sample.

Current behavior when CHH and OHH fire on the same sample: the open hat
retriggers and **both voices sound** (the OHH trigger runs after the CHH trigger
and clears the pending choke). The sequential choke — CHH firing while an OHH is
already ringing — works as built (5 ms envelope collapse, see `src/ChhOhh.cpp`).

## Decision

**Keep the permissive behavior: GHOST OHCH lets you layer a closed and open hat
on the same step.** We do NOT add voice-level "CHH erases OHH on a coincident
step" logic.

## Rationale

- It is *more* expressive than the hardware — deliberate closed+open layering is
  a usable sound a 909 forbids. This matches the GHOST stance of being more
  flexible/playable than the machine it's inspired by (cf. keeping the per-voice
  accent jack so two accent lines are possible — see
  `memory/project_ghost_accent_jack`).
- The 909's erase is a *sequencer* behavior; emulating it in the voice would be
  guessing at intent we don't own (the user's external sequencer is the pattern
  authority).
- Anyone who wants strict 909 behavior simply doesn't program CHH and OHH on the
  same step — no feature is lost.
- The canonical **sequential** choke (the part that defines the 909 hi-hat sound)
  is fully implemented and unaffected by this choice.

## Consequences

- A patch with CHH and OHH on the same step will hear both, not one. This is by
  design, not a bug, and is the one case where the hats "sum."
- Regression fixture: `patches/tests/ohch-choke.vcv` exercises the sequential
  choke (closed hat every 16th, open hat on the offbeats).

## Alternatives considered

- **CHH-priority on coincident step** (CHH erases OHH at the voice level):
  reproduces the 909 result without sequencer logic. Rejected as the default to
  preserve flexibility.
- **Right-click toggle "Closed hat erases open":** offers both. Deferred — can be
  added later if demand appears; not worth the surface area now.
