# Ghost — Accent & GHOST CTRL

How accent works across the Ghost drum voices, and how **GHOST CTRL** shapes it.
This is the canonical behaviour; the implementation lives in `src/GhostBus.hpp`.

## The idea in one line

Each voice has up to **two accent inputs** plus a no-accent "ghost" level, and a
shared **GHOST CTRL** module sets the global balance between them. Per-step accent
gates are patched **by cable**; the global balance is broadcast **by adjacency**
(place GHOST CTRL next to the voices).

## Two accent rails

| Rail | Jack | Meaning |
|------|------|---------|
| **Total accent** (Accent A) | `TOTAL` | The **global** accent — analogous to the classic 909's single ACCENT track. Present on **every** voice. |
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

RIMCLAP and CRSHRIDE expose only Total, matching the classic 909 manual: rim,
clap, crash, ride and the open hat don't take the per‑voice accent.

A gate is "fired" for a hit when its jack is above ~1 V at the trigger edge. The
state is **latched at the trigger** and held for the whole hit, so moving a knob
mid‑hit won't shift its level.

## The four cases

Every voice has a per‑case output **level** ladder. The level depends on which
gates fired at the trigger:

| Case | Gates at trigger |
|------|------------------|
| **ghost**  | neither (the un‑accented "floor") |
| **global** | Total only (A) |
| **local**  | Local only (B) |
| **both**   | Total + Local |

The actual levels are tuned **per voice**:

- **GHOST KCK** — a wide, 909‑style ladder: ghost **15%**, local (B) **60%**,
  global (A) **100%**, both **100%** (capped). The wide spread is what gives the
  kick real accent dynamics; GHOST CTRL's **RANGE** switch moves the ghost floor
  (Tight ~35% / Classic 15% / Wide ~8%).
- **Other voices** — a gentle accent: un‑accented at the normal level, accented
  hits about **+3 dB** on top (Accent A or both). This keeps the global accent
  subtle across the kit, the way a 909-style accent reads.

## GHOST CTRL

GHOST CTRL broadcasts its global state to the **adjacent** Ghost voices via the
expander path (no cables needed for these):

| Control | What it does |
|---------|----------------|
| **ACCENT A** (knob + CV) | scales the Total‑accent contribution (0..1) |
| **ACCENT B** (knob + CV) | scales the Local‑accent contribution (0..1) |
| **MASTER** (knob + CV) | final output level of each voice |
| **RANGE** (switch) | scales each voice's ghost floor: Tight / Classic / Wide |

There is **no DEFAULT knob** — the no‑accent level is the voice's own built‑in
floor, and RANGE scales it. GHOST CTRL is **not** in the trigger path; per‑step
gates (TRIG, LOCAL, TOTAL) are patched from your sequencer directly to each voice.

## How a hit's level is decided

Accent is a **boost added on top of the un‑accented (ghost) level** — the
accented hit never dips below ghost and never jumps in from silence
(`resolveAccentGain`):

```
ghost = lin(ghostDb × RANGE) × ghostAmount
gain  = ghost
        + A·(lin(globalDb) − ghost)          if Total fired
        + B·(lin(localDb)  − ghost)          if Local fired
        + A·B·(lin(bothDb)  − …)              if both fired
```

where `A = ACCENT A`, `B = ACCENT B`. The result is multiplied by **MASTER**.

**Worked example — a Total‑only (global) hit on the kick:**

- ACCENT A = 1 → full global level (the kick's 100% tier)
- ACCENT A = 0.5 → halfway between ghost and global
- ACCENT A = 0 → the hit **collapses to the ghost floor**, not silence

So a global accent is scaled by GHOST CTRL's ACCENT A knob, and Local by ACCENT
B — while RANGE sets how deep the floor they rise from sits.

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
- **GHOST CTRL → voices:** place it adjacent; ACCENT A / ACCENT B / MASTER / RANGE
  ride the expander.
- To make a Local‑capable voice ignore Local, simply leave `LOCAL` unpatched
  (a voice can also be configured so "Local only" collapses to ghost).

> Bring your sequencer. Ghost brings the machine behaviour.
