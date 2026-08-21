# Building Machine Rhythms With GHOST TOMS

Tuning, accent and timing can turn three tom voices into a surprisingly
complete source of rhythm. The following patches explore one idea at a
time: shaping a phrase with accents, distributing it across pitches, and
finally allowing the whole pattern to move against the main clock.

Each patch changes only one element, making it easy to hear exactly what
that element — accent, tuning, orchestration, clock movement — contributes
on its own, and just as easy to lift it and misuse it somewhere else.

## Working In A Tradition

None of the techniques explored here emerged in isolation. They belong to
a machine-music vocabulary developed over decades by artists who found
possibilities in drum machines, sequencers and synchronization systems
that their designers hadn't necessarily anticipated — Juan Atkins, Jeff
Mills and Richie Hawtin among them. These patches are small studies in
that vocabulary: a chance to listen closely to an idea, understand its
mechanics, and then carry it somewhere personal. Studying it closely is
the point, not a substitute for it — the value is in changing one thing
and hearing what becomes possible, not in the study itself.

So the artist references below are precise about what kind of reference
they are. Three kinds appear in this piece, and each is labeled as such:

- **Transcription** — this pattern reproduces a specific, cited source.
- **Adaptation** — this patch adapts a documented technique to a new
  context (a different instrumentation, a different tool).
- **In the tradition of** — this experiment draws on an approach
  associated with an artist's known body of work, without claiming any
  single track as its source.

Nothing here is a transcription of a record. Where a technique is
adapted from a specific documented source, that source is named. Where a
patch simply works in a tradition an artist is known for, it says so
plainly instead of implying a closer connection than exists.

## The Setup

One routing, shared by every patch below, stated once so it doesn't need
repeating:

```
step sequencer  →  GHOST TOMS (low / mid / high)  →  output or mixer
```

A reference kick or hat joins in wherever a technique needs something
steady to be heard against. BPM, step counts and any patch-specific detail
sit in a small diagram beside each example rather than in the running text.

## Phrase Marking

**Technique: concentrate a short tom figure at a phrase boundary.**

A loop that never varies has no sense of where it is in time — every bar
sounds like every other bar. Put a brief, otherwise-absent event at the end
of a phrase, and the loop suddenly has a shape: a length, an approach, a
turnaround. The tom fill isn't decoration here, it's the only information
telling the listener a cycle is about to close.

```
bar 1   kick · · · clap · · · kick · · · clap · · ·   (no toms)
bar 2   same, plus mid tom on steps 11 12 · · 15 16
```

Try it:
- Run a plain two-bar foundation with nothing in the tom lanes at all.
- Add a four-hit mid-tom figure only at the very end of bar two.
- Move that figure's starting point a step or two earlier each time and
  listen to how early is too early.
- Swap the single mid-tom voice for a rising low–mid–high figure instead.
- Take the last hit away entirely and listen to what the phrase loses.

The lesson isn't really about programming a fill. It's that a phrase's
length and shape can live entirely in one small, brief event — the rest of
the loop can stay exactly the same.

## Accent Patterns

**Technique: build a second rhythm out of amplitude alone.**

Keep the trigger grid completely steady — same voice, same spacing, no
new hits added or removed — and change only which of those hits are
accented. The placement pattern and the accent pattern are now two
different rhythms occupying the same steps, and the second one is audible
even though nothing about *where* anything happens has changed.

```
low tom   ·   X   ·   ·   ·   X   ·   ·   ·   X   ·   ·   ·   X   ·   ·
accent            ^                       ^
step      1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
```

Try it:
- Start with evenly spaced hits and no accent at all.
- Accent every other hit and listen for the new, slower pulse that
  appears.
- Run a three-accent cycle against a bar that's naturally divisible by
  four, so the accent keeps landing somewhere different each time round.
- Give each of the three tom voices its own, unrelated accent cycle.
- Compare what an accent change does to the groove against what adding or
  removing a trigger does — they're not interchangeable moves.

In the tradition of Juan Atkins' machine-funk: a vocabulary built on exact
repetition animated from the inside, through emphasis, timbre and subtle
internal variation rather than through the pattern itself changing. The
groove here works the same way — the trigger grid never moves, and the
motion comes entirely from what gets emphasized inside it.

## Interlocking Voices

**Technique: build one composite rhythm out of three sparse parts.**

Give the low, mid and high toms each a short, incomplete figure — sparse
enough that none of them describes the groove on its own — and let the
groove exist only in how the three interlock. No voice is the lead; the
composite is the lead.

```
low    X   ·   ·   ·   ·   ·   ·   ·   X   ·   ·   ·   ·   ·   ·   ·
mid    ·   ·   X   ·   ·   ·   ·   ·   ·   ·   X   ·   ·   X   ·   X
high   ·   ·   ·   ·   X   ·   X   ·   ·   ·   ·   ·   X   ·   X   ·
step   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
```

Try it:
- Start with a short low-tom figure alone and get familiar with its gaps.
- Place the mid tom only inside those gaps.
- Add the high tom sparingly — only where it changes the direction or
  momentum of what's already there, not on every remaining free step.
- Mute each voice in turn and listen to what the other two sound like
  without it.
- Change a single trigger anywhere in the figure and notice how much of
  the combined pattern shifts, not just the one step you touched.

In the tradition of Jeff Mills, whose records demonstrate percussion
treated as complete compositional material — capable of carrying
identity, momentum and form the way a melody would carry them elsewhere.
Inspired by the way that approach lets tuned percussion do that carrying
work, this patch distributes a sparse phrase across the three GHOST TOMS
voices rather than giving any one of them the whole line. The practical
skill is orchestration: deciding which voice owns which moment, and
leaving the others silent so it can.

## Contour Through Tuning

**Technique: use the three voices as a small pitch space.**

Low, mid and high tom aren't three interchangeable drums — they're three
points a figure can travel through. A descending roll is the obvious
starting shape, but reversed, interrupted or repeated-note contours each
produce a different kind of motion, and the identity of the figure lives
in the path, not in any single hit.

```
step    13    14    15    16
voice   high  mid   low   low
```

Try it:
- Program a plain high-to-low roll across four consecutive steps.
- Reverse it, low to high, and compare the sense of motion.
- Repeat one voice before completing the movement — high, high, mid,
  low — and listen to what the repetition does to the arrival.
- Change the tuning interval between the three voices and hear how far
  the same rhythmic shape can be pushed before it stops reading as one
  gesture.
- Push decay time until the hits either stay separate or start to overlap
  into something closer to a continuous slide.

This section is really about hearing the toms as a three-point pitch
space rather than three separate instruments. The figure is defined as
much by the direction it traces as by when its hits land.

## Group Clock Drift

**Technique: move an internally coherent pattern against the main clock.**

Sequence all three tom voices from their own clock, separate from
whatever's driving the rest of the kit. The relationships between low,
mid and high stay exactly as programmed — but the whole group can now
shift against the reference rhythm as a single object, rather than any
one hit moving on its own.

```
tom clock   →  offset  →  low / mid / high (relationships fixed)
kick clock  →  reference
```

Try it:
- Start with both clocks locked, so there's a clean baseline to compare
  against.
- Introduce a small, fixed offset between them and listen to the toms
  arrive consistently late (or early) as a group.
- Let the offset drift continuously instead of sitting at one fixed
  amount, and listen to the toms pass through a whole range of
  relationships with the kick over time.
- Reset the offset back to zero at a chosen phrase length, so the drift
  becomes a repeating arc rather than an open-ended wander.
- Compare that continuous drift against a single fixed phase
  displacement — they read as different effects, not degrees of the same
  one.

In the tradition of Richie Hawtin, whose work provides a real lineage for
treating timing, phase and the relationship between machines as musical
parameters in their own right, not just as things to correct. The starting
point was a specific adaptation attempt: *Spastik*'s swung, slightly-off
timing, on the theory it might translate to a tom pattern. Worth telling
that part honestly — that timing turns out to be a piano-roll technique,
individual 32nd-notes nudged late by hand in a DAW, and nothing on record
says whether it was ever done on hardware. It doesn't adapt to a
sequencer. What this patch takes from Hawtin isn't that specific
technique, then — it's the underlying premise his work argues for, that
the relationship between machines is itself a musical parameter. The
result isn't random mistiming. It's coherent displacement: one rhythmic
object, moving as a whole against another.

## Combining The Techniques

One slightly larger patch, built from two or three of the above rather
than all five at once — piling on every technique at the same time just
buries them again, which defeats the point of isolating them in the first
place. A workable combination:

- interlocking voices establish the core rhythm,
- an accent cycle layered on top gives it a longer-than-the-bar arc,
- and a phrase-marking fill at the turn signals where that longer cycle
  resets.

Any two or three techniques can substitute for these. The goal is hearing
how they compose, with each one still identifiable inside the result.

## Closing Invitation

Five ways to use the same three voices: as markers at a phrase boundary,
as an accent cycle layered over a fixed grid, as interlocking parts with
no lead, as a contour through pitch, and as a coherently displaced group
against the main clock. Each is small enough to hear on its own, and open
enough to become a starting point rather than a finished pattern.

Take one existing sequence and change exactly one dimension at a time —
placement, accent, voice assignment, tuning, or clock relationship — before
touching anything else. Working example patches for all five techniques
are in `patches/demos/toms-*.vcv`.
