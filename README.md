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
| **GHOST CTRL** | Global state controller: DEFAULT / ACCENT A / ACCENT B / MASTER, broadcast to adjacent voices via the expander path |
| **GHOST KCK** | Kick voice (bridged-T resonator model) |
| **GHOST SNR** | Snare voice (dual triangle VCO + noise) |
| **GHOST OHCH** | Coupled open/closed hi-hat with canonical choke |
| **GHOST RIMCLAP** | Rim + clap ROMpler voice |
| **GHOST TOMS** | Low / mid / high tom kit |
| **GHOST CRSHRIDE** | Crash + ride cymbal pair |

Each voice also ships a **LAB** variant (`GHOST KCK LAB`, etc.) exposing
additional internal parameters for expert tuning and sound design.

## Architecture

- **Global state** comes from GHOST CTRL (default level, two accent modes, master).
- **Hit-time events** (trigger, local accent, total accent) travel by cable
  directly to each voice. GHOST CTRL is *not* an audio or trigger bus; it
  broadcasts global state to adjacent modules via the expander path.

```
GHOST CTRL ──(expander: global state)──> adjacent Ghost voices
   │
   └─ DEFAULT / ACCENT A / ACCENT B / MASTER

sequencer ──(cables: trig / accent gates)──> each voice
```

## Building

Requires a VCV Rack plugin SDK. By default the Makefile points at the
`rack-sdk` vendored in a sibling `vcv-rack` checkout. Override with:

```sh
make RACK_DIR=/path/to/Rack-SDK
make install
```

## License

MIT (see `LICENSE`). Bundled Inter font under the SIL Open Font License.
