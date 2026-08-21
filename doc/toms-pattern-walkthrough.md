# GHOST TOMS: a pattern walkthrough

Five patches, all loading from `patches/demos/toms-*.vcv`: Hora Drum
Sequencer driving GHOST TOMS through GHOST MIX, the same routing every other
demo patch here uses. Built by `tools/build_tom_demos.py` and
`tools/build_tom_drift_demo.py`.

A few of these borrow a real technique from a documented source, and that's
cited under each one. A couple are style-inspired by a specific artist's
known approach rather than a transcription of a track. The rest are just
generic techno/house moves with no particular owner. None of it is a
transcription of a recording.

## Fill At The Turn

A plain four-on-the-floor bed: kick on 1/5/9/13, clap on 5/13, closed hat on
every 8th note. Nothing in the tom lanes for the first bar. Then the mid tom
shows up on steps 11, 12, 15 and 16 of the second bar and nowhere else.
128 BPM.

This is drum-patterns.com's "Techno 10 Tom Fill" transcription, close to
verbatim. The pattern plays completely straight for a bar, then puts four
consecutive mid-tom hits at the very end of the phrase to mark the turnaround.
It works because the fill is the only thing that changes between the two
bars.

Source: [drum-patterns.com, "Techno 10 Tom Fill"](https://drum-patterns.com/techno-10-tom-fill/)

## Accent Contrast

Kick on 1/5/9/13, low tom on 2/6/10/14, steady, no syncopation. 126 BPM. The
point of this one is the accent lane: only steps 2 and 10 carry an accent
gate. Steps 6 and 14 don't.

Attack Magazine's TR-909 beginner guide describes this move for toms
directly — hit the same drum on a steady grid but alternate full-strength
and ghost hits, so the pattern breathes instead of hammering flat. On GHOST
TOMS this also doubles as a regression test: the accent stage used to crush
an accented hit down to about 1/8th volume instead of boosting it (a
driveGain bug, fixed in `TomsEngine.hpp`). Loop this patch and the gap
between steps 2/10 and 6/14 should read as a lift, not a dropout.

Source: [Attack Magazine, "The Beginners Guide To The TR-909"](https://www.attackmagazine.com/technique/tutorials/the-beginners-guide-to-the-tr-909/)

## Mills Lead

No kick, no snare. Low tom on 1/9, mid tom on 3/11/14/16, high tom on
5/7/13/15, a light clap on 5/13, ride on every 8th note. 130 BPM.

Jeff Mills has talked in interviews about building whole passages from
nothing but toms plus ride and clap, skipping kick and snare entirely, and
treating the 909 as an instrument he plays live rather than a sequence he
sets and forgets. This patch takes the instrumentation choice — toms
carrying the groove, ride and clap the only support — and locks it into a
static 16-step loop. Style-inspired, not a transcription: nobody's playing
this one live.

Source: [MusicTech, "Jeff Mills on the TR-909"](https://musictech.com/news/gear/jeff-mills-roland-tr-909-tony-allen/)

## Descending Roll

Same bed as Fill At The Turn — kick 1/5/9/13, clap 5/13, straight 8th-note
hats — but the fill is a descending roll instead of a repeated hit: high tom
on step 13, mid on 14, low tom landing twice on 15 and 16. 124 BPM.

No source for this one. It's the generic high-to-low fill shape that shows
up across house and techno regardless of drum machine or artist — the move
you'd reach for before reaching for anything more specific.

## Clock Drift

The steadiest pattern in the set: low tom on 1/9, mid on 5/13, high on
3/7/11/15, plain kick and hats underneath. 126 BPM. The interesting part is
downstream of the Hora grid, not in it.

The starting point was Richie Hawtin's *Spastik*, on the theory its swung
timing might translate to a tom pattern. Attack Magazine's breakdown of the
track is specific about how that timing was actually done: in Ableton's
piano roll, set to 1/32 resolution, individual 32nd-note hits get nudged
later than their quantized position by hand — "the sixth 32nd-note between
positions 1 and 1.2 is delayed," and a few more like it. That's a real,
documented technique, and it's a DAW piano-roll edit. The article doesn't
say anything about how or whether that timing existed on the original
hardware — the 909 is credited there as Hawtin's sample source, not his
sequencing method. So this patch isn't a Hawtin pattern.

What it is instead: all three tom trigger lanes merged into one poly cable
(`Merge`, Fundamental), through a single `Bogaudio CVD` set to roughly 20ms
and fully wet, then split back out to GHOST TOMS' three trigger inputs. One
delay time, applied identically to all three voices, as if the toms were
coming from a second drum machine whose clock had drifted against the main
kit. Three cables in, one delay module, three cables out — a coarser and
much simpler idea than nudging individual hits in a piano roll, arrived at
because that piano-roll technique doesn't have a real hardware-sequencer
equivalent to build toward.

Source: [Attack Magazine, "Spastik-Style Percussive Techno"](https://www.attackmagazine.com/technique/beat-dissected/spastik-style-percussive-techno/)

## Loading these

All five patches run on VCV Library plugins only: Hora Sequencers,
SlimeChild Substation, Core, plus Fundamental and Bogaudio for
`toms-clock-drift.vcv`'s delay chain. No AgentRack dependency, unlike some
of the other example patches in `patches/demos/`. Full plugin matrix in
[`patches/demos/README.md`](../patches/demos/README.md).
