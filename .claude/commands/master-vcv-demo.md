---
description: Master a recorded demo (video or audio) to a present, peak-safe mp4 with a light limiter
---

Lift a VCV demo to a louder, present level using a **light true-peak limiter** —
modest drive into a peak ceiling with a smooth release. No loudnorm. Apply after
`record-vcv-demo`, or to re-master any clip. Always **audition the raw capture
first** (it's the user's call whether/how much to limit).

**Args:** `$ARGUMENTS` = `[input file] [drive]`
- `input file` — defaults to the most recent `/tmp/ghost_demo*.mov` (the raw
  capture). Always master from the **raw `.mov`**, never a re-encoded mp4.
- `drive` — limiter input gain, default **1.7** (≈ +4.6 dB). Higher = louder +
  more limiting; lower → toward pure peak-normalize. ~1.7 lands around -13 LUFS
  on a typical kit loop.

## Why a light limiter, not loudnorm
We tried 2-pass `loudnorm` to -11 LUFS and it **pumped** on sparse, transient
drum material (its gate/limiter stage breathes when there's space between hits).
A light limiter just tames the kick/snare transient peaks and lifts the average —
no gating, no pumping. Verified: -16.7 → **-13.1 LUFS**, peak -0.7 dBFS, and the
loudness range stays healthy (**LRA ~5 LU** — dynamics intact, not squashed).

Loud-but-present for a drum demo is ~-12 to -13 LUFS with the transients
breathing, NOT -11 slammed flat.

## Critical rules
- **Audition raw first.** Master only after the user likes the take. Don't
  pre-master.
- **Master from the raw `.mov`** (no double generation loss).
- **Keep the dynamics.** If LRA collapses below ~3 LU or it starts pumping, back
  the `drive` off. The kick/snare transients should still punch.
- If the source clips hard at the source (peak pinned at 0, body distorted), the
  fix is to **lower the VCV AUDIO/GHOST MIX master and re-record with headroom**,
  not to limit harder.
- Video: `libx264 -crf 23 -preset medium -pix_fmt yuv420p -movflags +faststart`,
  audio `aac -b:a 320k`.

## Run

```bash
IN="${1:-$(ls -t /tmp/ghost_demo*.mov 2>/dev/null | head -1)}"
DRIVE="${2:-1.7}"
TS=$(date +%Y%m%d-%H%M)
DEST="$HOME/Desktop/ghost-demo-ltd-$TS.mp4"
test -f "$IN" || { echo "no input file"; exit 1; }

ffmpeg -y -i "$IN" \
  -af "alimiter=level_in=$DRIVE:limit=0.89:attack=5:release=60:level=disabled" \
  -c:v libx264 -preset medium -crf 23 -pix_fmt yuv420p -movflags +faststart \
  -c:a aac -b:a 320k "$DEST"
# (audio-only input: drop the -c:v ... video flags)

open "$DEST"
```

## Verify + report (always)
Before/after LUFS, peak, and loudness range — confirm peak ≤ ~-0.7 dBFS and LRA
didn't collapse (≈ no pumping):
```bash
ffmpeg -hide_banner -i "$DEST" -af ebur128 -f null - 2>&1 | grep -E 'I:|LRA:' | tail -2
ffmpeg -hide_banner -i "$DEST" -af volumedetect -f null - 2>&1 | grep -iE 'mean_volume|max_volume'
```
If it pumps or sounds squashed, lower `drive`. Then offer to post to Discord
(#vcv-rack-devs).
