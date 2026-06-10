# patches/demos/

One self-contained groove per classic-909 style (house, detroit, chicago-jack,
hypnotic, …), the full kit through GHOST MIX. `02-detroit-sync.vcv` is the
host-transport-synced variant — silent on load, starts/stops with the DAW
(see `doc/using-ghost-in-a-daw.md`).

Also here: standalone example patches — `ghost-full-kit.vcv`,
`ghost-accent-groove.vcv`, `ghost-buscrush-demo.vcv`, `ghost-output-ep.vcv`,
and `ghost-supersonic.vcv`.

## Plugin requirements

The numbered demos (`01`–`09`) need only free VCV Library plugins besides
Ghost: Hora Sequencers (Drum Sequencer), SlimeChild Substation (clock), and
Core. The lessons additionally use Bogaudio (UMix).

`ghost-accent-groove`, `ghost-buscrush-demo`, `ghost-supersonic`, and
`ghost-output-ep` depend on **AgentRack** (BusCrush, Saphire, …), which is not
in the VCV Library — they load fully only on machines with AgentRack
installed. `ghost-full-kit.vcv` is pure Ghost.
