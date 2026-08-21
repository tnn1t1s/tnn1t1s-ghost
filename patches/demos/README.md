# patches/demos/

One self-contained groove per classic-909 style (house, detroit, chicago-jack,
hypnotic, …), the full kit through GHOST MIX. `02-detroit-sync.vcv` is the
host-transport-synced variant — silent on load, starts/stops with the DAW
(see `doc/using-ghost-in-a-daw.md`).

Also here: standalone example patches — `ghost-full-kit.vcv`,
`ghost-accent-groove.vcv`, `ghost-buscrush-demo.vcv`, `ghost-output-ep.vcv`,
and `ghost-supersonic.vcv`.

## Tom pattern showcase (`toms-*.vcv`)

Four patches built to exercise GHOST TOMS specifically (built by
`tools/build_tom_demos.py`). Style-inspired grooves, not transcriptions of
any specific recording — see the module docstring for sourcing on each:

- `toms-fill-at-the-turn.vcv` — steady groove, mid-tom fill on the turn of
  the second bar (a documented techno tom-fill convention).
- `toms-accent-contrast.vcv` — a steady tom line alternating accented and
  ghost hits, a direct listening test for the Toms accent swing.
- `toms-mills-lead.vcv` — no kick or snare; toms plus ride/clap carry the
  whole groove, style-inspired by Jeff Mills' documented toms-as-lead
  approach.
- `toms-descending-roll.vcv` — a generic high-to-low tom roll into the next
  bar.

## Plugin requirements

The numbered demos (`01`–`09`) need only free VCV Library plugins besides
Ghost: Hora Sequencers (Drum Sequencer), SlimeChild Substation (clock), and
Core. The lessons additionally use Bogaudio (UMix).

The demos were designed with **Hora's Drum Sequencer** — it's the right tool
for step-programming a 909-style kit, and Ghost was built to pair with it.
The voices respond to standard triggers and gates, so any Rack sequencer
works; the demos standardize on Hora because they built something good.

`ghost-accent-groove`, `ghost-buscrush-demo`, `ghost-supersonic`, and
`ghost-output-ep` depend on **AgentRack** (BusCrush, Saphire, …), which is not
in the VCV Library — they load fully only on machines with AgentRack
installed. `ghost-full-kit.vcv` is pure Ghost.
