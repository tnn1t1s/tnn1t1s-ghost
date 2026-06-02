# TNN1T1S Ghost

**Classic machine behavior, rebuilt for Rack.**

Ghost is a Rack-native drum system inspired by classic 909 behavior: separate
voices, shared accent, coupled hi-hat choke behavior, and a central controller
(**GHOST CTRL**) that makes independent modules behave like one instrument.

Ghost is not a clone. It is the trace of a drum machine rebuilt as modules:
individual voices, shared accent, and a central controller that coordinates the
kit. Each voice works standalone and comes alive with GHOST CTRL.

> Bring your sequencer. Ghost brings the machine behavior.

Ghost is a good Rack citizen. The voices respond to standard triggers and CV, so
you drive them with any sequencer in the ecosystem (Hora Drum Sequencer, Impromptu
Clocked + SEQ, etc.). Ghost is the 909-style behavioral layer between your
sequencer and your drum voices, not a closed world.

## Modules

| Module | Role |
|--------|------|
| **GHOST CTRL** | Global controller: ACCENT A / ACCENT B / MASTER amounts + a RANGE switch (kick dynamic range), broadcast to adjacent voices via the expander path |
| **GHOST KCK** | Kick voice (bridged-T resonator model) |
| **GHOST SNR** | Snare voice (dual triangle VCO + noise) |
| **GHOST OHCH** | Coupled open/closed hi-hat with canonical choke |
| **GHOST RIMCLAP** | Rim + clap ROMpler voice |
| **GHOST TOMS** | Low / mid / high tom kit |
| **GHOST CRSHRIDE** | Crash + ride cymbal pair |
| **GHOST MIX** | Dedicated 12-input kit summing mixer: one labeled input per voice (+ aux), each with a mute switch |

## Architecture

- **Global state** comes from GHOST CTRL (two accent amounts, master, dynamic range).
- **Hit-time events** (trigger, local accent, total accent) travel by cable
  directly to each voice. GHOST CTRL is *not* an audio or trigger bus; it
  broadcasts global state to adjacent modules via the expander path.

```
GHOST CTRL ──(expander: global state)──> adjacent Ghost voices
   │
   └─ ACCENT A / ACCENT B / MASTER / RANGE

sequencer ──(cables: trig / accent gates)──> each voice
```

## RANGE — dynamic range in a switch

A real 909 needs a hardware mod to change how far an un-accented hit ducks below
an accented one. GHOST CTRL's **RANGE** switch does it with three positions —
**Tight** (even, house-friendly), **Classic** (default), and **Wide** (full
ghost-kick dynamics). It scales each voice's accent floor, so it only moves
voices that have one (the kick); the rest are untouched. The ACCENT A / B knobs
still ride within the chosen range.

## Patches

Example patches live in `patches/`:

- `patches/909-demos/` — eight classic-909 grooves, one per style (house,
  Detroit, Chicago jack, hard techno, tribal, filter house, minimal techno, electro),
  plus a minimal-techno multi-level kick study, each on GHOST MIX at its own tempo.
- `patches/909-lessons/` — a build-it-up series (rimshot → kick → claps → toms →
  hats) for learning the kit one voice at a time.

## Building

Requires a VCV Rack plugin SDK. By default the Makefile points at the
`rack-sdk` vendored in a sibling `vcv-rack` checkout. Override with:

```sh
make RACK_DIR=/path/to/Rack-SDK
make install
```

## Audio provenance

All embedded audio captures were recorded by the author from a drum machine the
author owns and runs in their studio, and which the author has used on published
recordings. No third-party sample libraries or proprietary ROM dumps are
included.

## License

MIT (see `LICENSE`). Bundled Inter font under the SIL Open Font License.
