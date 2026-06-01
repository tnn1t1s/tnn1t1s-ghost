# GHOST — Release Roadmap

Single view of everything between here and a public launch on the VCV Library.
Each item links to its tracking issue. Two tracks run in parallel: **Ship the
plugin** (technical / VCV submission) and **Launch it** (community / content).

## Track A — Ship the plugin (VCV Library submission)

- [ ] **Unregister the tuning benches** — drop `KckLab` (and don't ship `TomLab`);
      they're flagged temporary in `src/plugin.cpp`. *(no issue yet — pre-req)*
- [ ] #6 IP / ethics audit — no Roland/TR-909 public-facing; panel distinctness
- [ ] #1 Manifest keywords for discoverability
- [ ] #9 Documentation completeness (README + manual)
- [ ] #4 Stability soak + timing / choke / accent correctness
- [ ] #5 Memory-leak & CPU profiling (Instruments)
- [ ] #7 Local release verification (`make dist`/install + Rack load + save/reload)
- [ ] #3 Cross-platform CI via the official VCV toolchain (VCV builds per-OS from source)
- [ ] #10 Submit to VCV Library

## Track B — Launch it (community / content)

- [ ] #8 Demo patches for the demo sequence *(have: buscrush, accent-groove, supersonic, full-kit)*
- [ ] #11 Website — simple hosted landing page (GitHub Pages)
- [ ] #12 Discord — public community channel
- [ ] #13 YouTube channel + 5 launch videos

## Suggested order

1. **IP/ethics audit (#6)** first — it can force naming/panel changes; cheapest to do before docs/site/videos lock copy.
2. **Website (#11)** — self-contained, reuses the panel renders, becomes the hub all other links point at.
3. **Docs (#9) + demo patches (#8)** — the manual and patches feed both the submission and the videos.
4. **Discord (#12)** — stand it up so the invite can go in docs/site/listing.
5. **YouTube (#13)** — scripts can start now; record once patches + site are ready.
6. **Verification (#4/#5/#7) → CI (#3) → Submit (#10).**

## Done

- Voice DSP: all seven on the panelkit system; kick rebuilt + TR-09 calibrated;
  toms noise circuit added. Suite sounds convincing (audit: real 909 samples for
  hats/crash/ride/rim/clap, faithful analog topology for snare/kick/toms).
- Inventory cleaned across tnn1t1s-ghost + AgentRack.
