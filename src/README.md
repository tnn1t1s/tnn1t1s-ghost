# src/

C++ source for the Ghost VCV Rack plugin. Each voice is a thin Rack `Module`
(`Kck.cpp`, `Snr.cpp`, `Toms.cpp`, …) over a header-only DSP core (`*Engine.hpp`);
`GhostCtrl` and `GhostMix` provide the shared accent/level bus and the kit mixer.

- `embedded/` — baked-in sample data for the ROM voices
- `ghost/` — shared signal utilities
- `lab/` — unregistered per-voice dev/tuning variants
