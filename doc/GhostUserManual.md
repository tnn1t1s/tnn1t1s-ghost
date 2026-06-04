# Ghost User Manual

**Classic machine behavior, rebuilt for Rack.**

Ghost is a drum system: a set of voice modules plus a central state controller
(**GHOST CTRL**) that makes them behave like one instrument. Each voice works
standalone with any sequencer; GHOST CTRL adds the shared accent and level
behavior on top.

> Bring your sequencer. Ghost brings the machine behavior.

## The modules

### GHOST CTRL

Global state controller. Three knobs (each with a CV input) plus a switch:

- **ACCENT A** — amount applied whenever Accent A (total accent) fires.
- **ACCENT B** — amount applied whenever Accent B (local accent) fires.
- **MASTER** — final output level scalar.
- **RANGE** — dynamic range: how far an un-accented hit ducks below an accented
  one. **Tight** (even), **Classic** (default), **Wide** (full ghost-kick
  dynamics). RANGE scales each voice's accent floor, so it only affects voices
  that have one — currently the kick. There is no separate "default level" knob:
  the no-accent level is the voice's built-in floor, which RANGE scales.

GHOST CTRL broadcasts this global state to adjacent Ghost voices via the
expander path. It is *not* an audio or trigger bus: per-step triggers and
accent gates travel by cable directly from your sequencer to each voice.

### Voices

| Module | Voice |
|--------|-------|
| **GHOST KCK** | Kick — struck bridged-T resonator: damped sine body, pitch-envelope glide, bandpassed click, tanh drive |
| **GHOST SNR** | Snare — dual reset triangle VCOs, fixed binary noise, split body/noise envelopes |
| **GHOST OHCH** | Open + closed hi-hat sharing a coupled sound path; a CH hit instantly chokes any sounding OH |
| **GHOST RIMCLAP** | Rim + clap ROMpler voice, independent triggers/controls/outputs |
| **GHOST TOMS** | Low / mid / high tom kit on one calibrated engine |
| **GHOST CRSHRIDE** | Crash + ride cymbals, independent Tune / Decay / Drive / Level |

### GHOST MIX

A dedicated summing mixer for the kit: twelve labeled inputs — one per voice of
the full kit (Kick, Snare, Rim, Clap, Tom Lo/Mid/Hi, Closed Hat, Open Hat,
Crash, Ride) plus a MIXIN channel — the 909's external mix-in jack, for an
outside signal or a chained voice —
each with a mute switch, summed to one MIX output. Per-voice level lives on the
voices, so the mixer is a clean unity summer — the whole kit on one master mix
point, in the box.

## Accent behavior

Voices respond to two accent rails sampled at trigger time:

- **Accent A** (total accent) — shared by all voices.
- **Accent B** (local accent) — used by voices that support it (e.g. CH on
  GHOST OHCH).

Patch the accent gates from your sequencer to each voice. GHOST CTRL scales how
much each rail contributes across the whole kit.

## Hi-hat choke (GHOST OHCH)

The open and closed hats share one sound path. Triggering the closed hat
instantly mutes any sounding open hat — the canonical hi-hat choke, often
mistaken for compression. A fresh open-hat trigger re-arms the voice.

## A typical patch

1. Trigger a voice from your sequencer to hear it standalone.
2. Place GHOST CTRL next to the voices and watch the kit respond as one.
3. Patch Accent A / Accent B gates from the sequencer to each voice.
4. Use GHOST CTRL DEFAULT / ACCENT A / ACCENT B / MASTER to shape dynamics.
5. On GHOST OHCH, trigger CH while OH rings to hear the choke.

Use any Rack sequencer (Hora Drum Sequencer, Impromptu Clocked + SEQ, etc.).
Ghost is the behavioral layer between your sequencer and your drum voices.

## Running in a DAW

To host Ghost in Logic Pro (or any DAW) via VCV Rack 2 Pro, and to sync the kit
to your project's transport so it starts, stops, and follows tempo, see
`using-ghost-in-a-daw.md`. It also covers the audio routing and two host-specific
gotchas (knobs not moving, and a keyboard-related crash on load).

## Audio provenance

All embedded audio captures were recorded by the author from a drum machine the
author owns and runs in their studio, and which the author has used on published
recordings. No third-party sample libraries or proprietary ROM dumps are
included.
