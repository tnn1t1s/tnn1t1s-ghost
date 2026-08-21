# GHOST TOMS: a pattern walkthrough

Five ways to programme three drums. Everything here loads from
`patches/demos/toms-*.vcv` — Hora Drum Sequencer driving GHOST TOMS through
GHOST MIX, same routing convention as the rest of the demo patches
(`tools/build_tom_demos.py`, `tools/build_tom_drift_demo.py`).

None of these are transcriptions of a specific record. Where a technique is
lifted from a documented source, that's cited. Where it's a characterization
of an artist's known approach rather than a transcription, that's said
plainly too — and where no citable source existed for an attribution we
wanted to make, we left the attribution out rather than invent one.

## Fill At The Turn

128 BPM. Kick on 1/5/9/13, clap on 5/13, closed hat on every 8th note — a
plain four-on-the-floor bed with nothing in the tom lanes for the whole
first bar. The mid tom only shows up on the turn: steps 11, 12, 15 and 16
of the *second* bar, and nowhere else.

That's not an invented shape. It's drum-patterns.com's "Techno 10 Tom Fill"
transcription, almost verbatim — a groove that plays completely straight for
a bar, then uses four consecutive mid-tom hits at the very end of the phrase
to signal "the pattern is about to repeat." It's one of the oldest tricks in
techno programming, and it works because the tom fill is the *only* thing
that changes between the two bars — your ear has nothing else to latch onto,
so it latches onto that.

Source: [drum-patterns.com, "Techno 10 Tom Fill"](https://drum-patterns.com/techno-10-tom-fill/)

## Accent Contrast

126 BPM. Kick on 1/5/9/13, low tom on 2/6/10/14 — steady, no fill, no
syncopation. What's interesting isn't the pattern, it's the accent lane:
only steps 2 and 10 carry an accent gate. Steps 6 and 14 don't.

This one exists to be listened to closely rather than danced to. Attack
Magazine's TR-909 beginner guide describes exactly this move for toms — hit
the same drum on a steady grid, but alternate full-strength and "WEAK"
velocity between repeats, so the pattern breathes instead of hammering. On
GHOST TOMS specifically, this pattern is also a direct listening test: the
Toms accent stage used to *crush* an accented hit to about 1/8th volume
instead of boosting it (a driveGain bug fixed in this repo's `TomsEngine.hpp`
— see the PR history for the forensics). Loop this patch and the difference
between steps 2/10 and 6/14 should be a clean lift, not a dropout.

Source: [Attack Magazine, "The Beginners Guide To The TR-909"](https://www.attackmagazine.com/technique/tutorials/the-beginners-guide-to-the-tr-909/)

## Mills Lead

130 BPM. No kick. No snare. Low tom on 1/9, mid tom on 3/11/14/16, high tom
on 5/7/13/15, a light clap on 5/13, ride on every 8th note.

Jeff Mills doesn't program toms as decoration — in interviews he's talked
about building whole passages from nothing but toms plus ride and clap,
skipping kick and snare entirely, and treating the 909 as something you
*play* live (start/stop, real-time stutters) rather than something you
sequence once and forget. This patch borrows the instrumentation choice —
toms carrying the groove, ride and clap the only other voices — but it's a
fixed 16-step loop, not a performance, so don't take it as "here's how Mills
plays a 909." It isn't. It's what that instrumentation choice sounds like
once you commit it to a static pattern.

Source: [MusicTech, "Jeff Mills on the TR-909"](https://musictech.com/news/gear/jeff-mills-roland-tr-909-tony-allen/)

## Descending Roll

124 BPM. Same steady bed as Fill At The Turn (kick 1/5/9/13, clap 5/13,
straight 8th-note hats), but the fill this time is a proper roll: high tom
on step 13, mid tom on 14, low tom lands twice on 15 and 16 to close it out.
High to low, four steps, once per two bars.

There's no citation for this one — it's the generic "descending fill"
shape that shows up across house and techno regardless of which drum
machine or artist you're listening to. Worth having in the set precisely
because it *isn't* attributed to anyone: it's the default move, the one
you'd reach for before you'd reach for anything more specific.

## Clock Drift

126 BPM. The steadiest pattern of the five — low tom on 1/9, mid on 5/13,
high on 3/7/11/15, kick and hats holding a plain groove underneath. The
interesting part isn't in the Hora grid at all. It's downstream of it.

We went looking for how Richie Hawtin gets the swung, slightly-off timing on
tracks like *Spastik*, on the theory it might translate to a tom pattern.
Attack Magazine's breakdown of that track is specific about the mechanism:
in Ableton's piano roll, set to 1/32 resolution, individual 32nd-note hits
get nudged later than their quantized position by hand — "the sixth
32nd-note between positions 1 and 1.2 is delayed," and so on for a handful
of other hits. It's a real, documented technique. It's also entirely a DAW
piano-roll trick — the article is explicit that it doesn't cover how (or
whether) that timing was ever done on the original hardware, and the 909
only gets credited as Hawtin's *sample source*, not his sequencing method.
So this isn't a Hawtin pattern, and it isn't attributed to him.

What it is: a different idea, prompted by that dead end. Instead of nudging
one hit at a time in a piano roll, run all three tom trigger lanes through
one shared delay before they reach the module — `Merge` (Fundamental) to
combine the three Hora lanes into one polyphonic cable, a `Bogaudio CVD` set
to roughly 20ms and fully wet, then `Split` back out to GHOST TOMS' three
trigger inputs. One knob, one delay time, applied identically to all three
voices — as if the toms were coming from a second drum machine whose clock
had drifted slightly against the main kit, not as if any individual hit had
been hand-placed. It's a coarser effect than the Ableton technique and a
much simpler patch: three cables in, one delay module, three cables out.

Source: [Attack Magazine, "Spastik-Style Percussive Techno"](https://www.attackmagazine.com/technique/beat-dissected/spastik-style-percussive-techno/)

## Loading these

All five patches are pure VCV Library plugins — Hora Sequencers, SlimeChild
Substation, Core, plus Fundamental and Bogaudio for `toms-clock-drift.vcv`
specifically (the delay chain). No AgentRack dependency, unlike some of the
other example patches in `patches/demos/`. See
[`patches/demos/README.md`](../patches/demos/README.md) for the full plugin
matrix.
