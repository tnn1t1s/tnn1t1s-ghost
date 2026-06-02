# 0002 — Trademark framing: inspired-by, never a clone

**Status:** Accepted (2026-06-02), **superseded same day — escalated to a full rename** (see Update below)
**Relates to:** RELEASE.md Track A #6 (IP / ethics audit)

## Update (2026-06-02) — escalated to full rename

The original decision kept tier-4 internal identifiers. On reflection the
operator escalated: the project began as a 909 clone but became its own
instrument (hence "Ghost"), so the code names should say what it *is*. **Tier 4
was done too:** namespace `Ghost::TR909::` flattened to `Ghost::`; files renamed
(`Tr909Bus`→`GhostBus`, `TR909VoiceCommon`→`GhostVoice`, `Tr909Ctrl`→`GhostCtrl`,
`NineOhNinePanel`→`GhostPanel`, `embedded/*909Data`→`Ghost*Data`); identifiers
(`Tr909Module`→`GhostModule`, `NineOhNine` ns→`LabArt`, `*909_f32`→`ghost*_f32`);
and the `GhostCtrl` model slug (with all patches + builders updated). Result:
**no "909" model reference anywhere in `src/`** — comments, identifiers, or files.

## Context

"TR-909", "TR-09", and "Roland" are Roland trademarks. VCV Library rules reject
plugins that present themselves as clones or use trademarked names in a way that
implies the product *is* or is endorsed by the trademark holder. GHOST is
inspired by classic 909 behavior but is its own implementation (different DSP,
its own panels, its own brand). Presenting a module as "TR-909 closed + open
hi-hat" reads as a clone and is exactly what would fail review.

Audit (2026-06-02): the **user-facing surface is already clean** — `plugin.json`
descriptions/names and `README.md` have zero "TR-909/TR-09/Roland" references
(they use "909-inspired", "909-style", "inspired by classic 909 behavior"). The
remaining references are in `src/` (code) and `doc/`: mostly the internal
`TR909` identifier (namespace/classes, not user-visible) plus comment text and
calibration provenance.

## Decision

GHOST is **"inspired by / 909-style," never a clone.** Tiered policy:

1. **User-facing (manifest names + descriptions, README, manual, panel text):**
   no "TR-909 / TR-09 / Roland". Use "909-style" / "inspired by classic 909
   behavior". (Already compliant — keep enforcing.)
2. **File-header / module comments:** reframe "TR-909 X" → "909-style X".
3. **Calibration provenance:** keep the engineering record, but phrase the
   reference generically — "the reference machine" / "a 909-style hardware
   reference" — rather than repeating "TR-909". (The README "Audio provenance"
   section already establishes the author owns and measured the hardware.)
4. **Internal code identifiers** (`Ghost::TR909`, `Tr909Bus`, `Tr909Ctrl`, …):
   **retained.** They are never shown to a user, renaming ~146 sites is risky
   churn, and a model number in a private identifier is not consumer-facing
   trademark use. Revisit only if a reviewer objects.

## Rationale

- The trademark risk that matters is *consumer-facing* use implying clone/
  endorsement — tier 1 — and that's already clean.
- Factual comparison against hardware the author owns (provenance) is honest and
  defensible; softening it is caution, not necessity.
- Identifier churn buys negligible IP protection for real refactor risk.

## Consequences

- A pre-submission fix pass reframes tier-2 comments and softens tier-3
  provenance. Tier-4 identifiers stay.
- `doc/` references get the same tier-2/3 treatment.
- If VCV review ever flags an identifier, revisit tier 4 then.
