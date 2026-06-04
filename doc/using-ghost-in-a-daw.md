# Using Ghost in a DAW (Logic Pro)

Ghost runs inside any DAW that hosts **VCV Rack 2 Pro** as a plugin. This guide
covers loading it in Logic Pro, getting audio out, and syncing the kit to your
project's transport so it starts, stops, and stays in time with the song.

Everything here applies to Logic specifically (Audio Unit), but the sync recipe
is the same in any host that sends MIDI clock to the plugin.

## 1. Load Rack as an instrument

1. On a software-instrument track, set the Instrument slot to
   **AU Instruments → VCV → VCV Rack 2**.
2. Open the plugin window and load your Ghost patch (File → Open), or build one.

### Audio routing: the DAW owns the I/O

When Rack runs as a plugin, **Logic owns all audio input and output** through
your audio interface (Apollo, Volt, whatever the project uses). You do **not**
pick an audio device inside Rack. Route your Ghost mix to the plugin's output
bus, and it lands on the Logic channel strip like any instrument. There is no
device selector to set in the patch.

## 2. Sync the kit to the project transport

By default a Rack patch free-runs the moment it loads, ignoring the DAW. To make
Ghost start and stop with the song and follow its tempo, drive the sequencer's
clock from the host instead of an internal clock.

Logic sends the plugin a 24-PPQN MIDI clock plus Start / Stop / Continue. The
Core **MIDI-CV** module exposes these as jacks:

| MIDI-CV output | What it is |
|----------------|------------|
| **CLK** (out 7) | the raw 24-PPQN clock — use this |
| **CLK/N** (out 8) | a pre-divided clock — do **not** use it for the Hora |
| **START / STOP / CONTINUE** | triggers on transport events |

### The wiring

1. Add a **Core MIDI-CV** module. In the plugin, confirm its MIDI device points
   at the **host** transport, not a hardware port (Rack Pro normally auto-routes
   the host MIDI).
2. Cable **MIDI-CV CLK (out 7) → the Hora Drum Sequencer's CLOCK input.** The
   Hora reads the raw 24-PPQN clock as a tempo reference and subdivides to 16th
   steps internally, so it wants the un-divided CLK. Do **not** use the divided
   CLK/N output — it runs the pattern dramatically too slow (160 BPM played like
   10).
3. Cable **MIDI-CV START (out 9) → the Hora's RESET input** so every Play snaps
   the sequence to step 1 (the downbeat).
4. Make sure nothing else clocks the sequencer. If you keep an internal clock
   module for layout, set its RUN to 0 and leave its outputs unpatched.

Result: the patch is **silent on load**, starts in sync when you press Play, and
follows the project tempo. (It is silent in standalone Rack too, because there is
no host clock — that is expected.)

A ready-made example ships in `patches/909-demos/02-detroit-sync.vcv`: the
Detroit demo, clock-slaved to the host.

## 3. Gotchas

### Knobs won't move in the plugin window

If you can drag cables but knobs don't respond inside the plugin, turn off
**cursor lock**: Rack's **☰ menu → untick "Allow cursor lock."** Knob dragging
uses a pointer-warp that the DAW's sandboxed plugin view can block; absolute
tracking (cables) is unaffected. As a fallback, enable **"Knob scroll"** and
scroll over a knob to change it. The setting is shared with standalone Rack, so
setting it once sticks.

### Rack crashes the instant you add it (gaming / programmable keyboards)

Some keyboards with a vendor-defined HID interface (for example the Keychron K8
HE) can crash VCV Rack on load **inside a DAW** but not in standalone. This is a
core-Rack issue in its HID/joystick scan under the host's plugin sandbox, not a
Ghost bug. Workaround: **unplug the keyboard** (or its wireless dongle) before
loading Rack in the DAW; all plugin-add steps are mouse-only, so you don't need
the keyboard during load. Reconnect afterward.

### Downbeat alignment

A sequencer left un-reset resumes from wherever it sat rather than snapping to
step 1. The fix is the `START → RESET` cable in step 3 above. The shipped
`02-detroit-sync.vcv` already wires it, so each Play begins on the downbeat; keep
that cable if you build your own synced patch.

## See also

- `GhostUserManual.md` — the modules and accent behavior.
- `recording.md` — capturing audio from Rack.
