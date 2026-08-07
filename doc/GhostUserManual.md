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

## Jacks and conventions

Every voice follows the same I/O pattern:

- **TRIG** — trigger input; the voice fires on the rising edge. Multi-voice
  modules (OHCH, RIMCLAP, TOMS, CRSHRIDE) have one trigger per sub-voice.
- **ACC A** — total accent gate (Accent A, shared by the whole kit), sampled
  at trigger time.
- **ACC B** — local accent gate (Accent B), on voices that support it
  (kick, snare, toms, and CH on GHOST OHCH).
- **One CV input per knob** — every knob (Tune, Decay, Level, …) has a
  matching CV jack, so any parameter can be sequenced, modulated, or
  MIDI-mapped via VCV MIDI-CC / MIDI-Map.
- **OUT** — the voice's audio output; one per sub-voice on multi-voice
  modules.

GHOST CTRL has a CV input per knob (Accent A, Accent B, Master). Accent gates
are sampled at the instant TRIG fires — an accent gate with no coincident
trigger does nothing.

## Accent behavior

Voices respond to two accent rails sampled at trigger time:

- **Accent A** (total accent) — shared by all voices.
- **Accent B** (local accent) — used by voices that support it (e.g. CH on
  GHOST OHCH).

Patch the accent gates from your sequencer to each voice. GHOST CTRL scales how
much each rail contributes across the whole kit.

## Hi-hat choke (GHOST OHCH)

The closed and open hats are one voice, as on the hardware: one sample, one
playback engine, and a closed/open control that changes the decay and voicing.
Only one hat sounds at a time. Everything follows from that:

- Trigger the closed hat while the open hat is ringing and the open hat stops —
  the canonical hi-hat choke, often mistaken for compression.
- Trigger the open hat over a closed one and the open hat takes over.
- Program both on the same step and you get the open hat. There is no setting;
  it is what one voice does. For closed hats on every 16th with open hats on the
  offbeats, the offbeat opens up instead of being buried.

They cannot be layered. The module keeps separate closed and open output jacks so
GHOST MIX can treat them as separate channels, but only one carries the voice at
any moment.

## A typical patch

1. Trigger a voice from your sequencer to hear it standalone.
2. Place GHOST CTRL next to the voices and watch the kit respond as one.
3. Patch Accent A / Accent B gates from the sequencer to each voice.
4. Use GHOST CTRL ACCENT A / ACCENT B / MASTER (and RANGE) to shape dynamics.
5. On GHOST OHCH, trigger CH while OH rings to hear the choke.

Use any Rack sequencer (Hora Drum Sequencer, Impromptu Clocked + SEQ, etc.).
Ghost is the behavioral layer between your sequencer and your drum voices.

## Example patches

Ready-made patches live in the repo under `patches/`:

- `patches/demos/` — eight classic-909 grooves, one per style (house, Detroit,
  Chicago jack, hypnotic, tribal, filter house, minimal, electro), plus a
  multi-level kick study. Each runs the full kit into GHOST MIX at its own
  tempo.
- `patches/lessons/` — a build-it-up series (rimshot → kick → claps → toms →
  hats) for learning the kit one voice at a time.

## Troubleshooting

- **A voice ignores GHOST CTRL** — the bus travels module-to-module: the voice
  must sit in an unbroken row of Ghost modules that includes GHOST CTRL. Any
  non-Ghost module between them breaks the chain.
- **No sound** — check the TRIG cable, the voice's LEVEL knob, MASTER on
  GHOST CTRL, and the channel mute on GHOST MIX.
- **Open hat cuts off** — that's the choke: a closed-hat trigger mutes the
  open hat by design. Clear the CH lane where you want OH to ring.
- **Accent has no effect** — accent gates are sampled at trigger time, so the
  gate must be high when TRIG fires. Then check the ACCENT A / B amounts on
  GHOST CTRL, and that the voice has that accent jack (see Jacks and
  conventions).
- **Problems inside a DAW** (knobs not responding, transport sync, crash on
  load) — see `using-ghost-in-a-daw.md`.

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
