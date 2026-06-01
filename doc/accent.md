# Ghost — Accent & GHOST CTRL

How accent works across the Ghost drum voices, and how **GHOST CTRL** shapes it.
This is the canonical behaviour; the implementation lives in `src/Tr909Bus.hpp`.

## The idea in one line

Each voice has up to **two accent inputs** plus a no-accent "ghost" level, and a
shared **GHOST CTRL** module sets the global balance between them. Per-step accent
gates are patched **by cable**; the global balance is broadcast **by adjacency**
(place GHOST CTRL next to the voices).

## Two accent rails

| Rail | Jack | Meaning |
|------|------|---------|
| **Total accent** (Accent A) | `TOTAL` | The **global** accent — analogous to the TR‑909's single ACCENT track. Present on **every** voice. |
| **Local accent** (Accent B) | `LOCAL` | A **per‑voice** accent track. Present only where the voice responds to it. |

**Which voices have which:**

| Voice | Total (A) | Local (B) |
|-------|:--:|:--:|
| KCK   | ✓ | ✓ |
| SNR   | ✓ | ✓ |
| OHCH  | ✓ | ✓ (closed hat only; open hat ignores it) |
| TOMS  | ✓ | ✓ |
| RIMCLAP | ✓ | — |
| CRSHRIDE | ✓ | — |

RIMCLAP and CRSHRIDE expose only Total, matching the Roland TR‑909 manual: rim,
clap, crash, ride and the open hat don't take the per‑voice accent.

A gate is "fired" for a hit when its jack is above ~1 V at the trigger edge. The
state is **latched at the trigger** and held for the whole hit, so moving a knob
mid‑hit won't shift its level.

## The four cases

Every voice has a per‑case output **level** (in dB, relative to its normal hit):

| Case | Gates at trigger | Default level |
|------|------------------|:--:|
| **ghost**  | neither | −6 dB |
| **global** | Total only (A) | −1 dB |
| **local**  | Local only (B) | 0 dB (the reference "normal" hit) |
| **both**   | Total + Local | +1.5 dB |

> These defaults are **modest, ear‑tunable starting points — not verified
> hardware values**. Several voices currently ship "neutral" (all cases 0 dB)
> until calibrated against TR‑909 reference samples. Per‑voice tuning is expected.

## GHOST CTRL

GHOST CTRL broadcasts four global controls to the **adjacent** Ghost voices via the
expander path (no cables needed for these):

| Control | What it scales |
|---------|----------------|
| **DEFAULT** | the no‑accent (ghost) level |
| **ACCENT A** | the Total‑accent contribution (0..1) |
| **ACCENT B** | the Local‑accent contribution (0..1) |
| **MASTER** | final output level of each voice |

Each has a CV input. GHOST CTRL is **not** in the trigger path — per‑step gates
(TRIG, LOCAL, TOTAL) are patched from your sequencer directly to each voice.

## How a hit's level is decided

For a hit, the output gain is (`resolveAccentGain`):

- **No accent** (or the relevant knob at 0): `gain = lin(ghostDb) × DEFAULT`
- Otherwise: `gain = A·lin(globalDb) + B·lin(localDb) + A·B·(lin(bothDb) − lin(globalDb) − lin(localDb))`
  where `A = ACCENT A`, `B = ACCENT B` are the GHOST CTRL amounts and the terms
  switch in only for the gates that fired.

The result is then multiplied by **MASTER**.

**Worked example — a Total‑only (global) hit:**

```
gain = ACCENT_A × linear(globalDb) × MASTER
```

- ACCENT A = 1 → full global level (default −1 dB)
- ACCENT A = 0.5 → half that linear gain (≈ −7 dB)
- ACCENT A = 0 → the hit **collapses to the ghost level**, not silence

So **yes — a global accent is scaled by GHOST CTRL's ACCENT A knob.** The same
holds for Local accent and ACCENT B.

## Level vs character (kept separate)

Two different things happen on an accented hit:

- **Level** — the per‑case dB above, scaled by the GHOST CTRL knobs. ("How loud.")
- **Character** — the voice's DSP feel: extra drive, pitch dive, brighter click,
  etc., set by the voice's own engine. ("How it sounds.")

Character is gated on **any** accent gate firing (`Total OR Local`) and is **not**
scaled by the ACCENT knobs. Consequence at the extreme: if `ACCENT A = 0` but a
Total gate still fires, the voice plays at **ghost level but with full accent
character**. If you want ACCENT A = 0 to neutralise the feel as well as the level,
that's a deliberate design choice to make, not current behaviour.

## Patching summary

- **From your sequencer → each voice:** `TRIG`, and the accent gates `TOTAL` / `LOCAL`.
- **GHOST CTRL → voices:** place it adjacent; DEFAULT / ACCENT A / ACCENT B / MASTER
  ride the expander.
- To make a Local‑capable voice ignore Local, simply leave `LOCAL` unpatched
  (a voice can also be configured so "Local only" collapses to ghost).

> Bring your sequencer. Ghost brings the machine behaviour.
