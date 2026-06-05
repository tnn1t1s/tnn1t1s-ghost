---
description: Master a recorded demo (video or audio) to a loud, present, broadcast-safe mp4
---

Loudness-master a media file to a consistent, punchy, peak-safe level using a
**2-pass `loudnorm`** (EBU R128) + true-peak limiter, then re-encode to a
shareable mp4. This is the standard finish for every VCV demo — apply it after
`record-vcv-demo`, or to re-master any existing clip.

**Args:** `$ARGUMENTS` = `[input file] [target_LUFS] [target_TP]`
- `input file` — defaults to the most recent `/tmp/ghost_demo*.mov` (the raw
  capture from `record-vcv-demo`). Always master from the **raw `.mov`** when
  available — never from an already-encoded mp4.
- `target_LUFS` — default **-11** (loud + present for social/demo use; louder
  than the -14 streaming norm on purpose).
- `target_TP` — default **-1.0** dBTP (true-peak ceiling; keeps transients off 0).

## Why these numbers
VCV drum captures are **high crest-factor**: transients spike to 0 dBFS while the
average sits ~-16 dB, so the raw file measures at streaming loudness yet *sounds*
quiet next to a mastered track. Raising the integrated loudness to -11 LUFS with a
-1 dBTP limiter closes that gap — ~+3 dB louder average, peaks controlled instead
of clipping. Verified result: -14.7 → -11.9 LUFS, mean -16.5 → -13.7 dB.

## Critical rules — do NOT deviate
- **2-pass, always.** Pass 1 measures; pass 2 applies the measured values. A
  single-pass `loudnorm` guesses and under/overshoots.
- **Master from the raw `.mov`**, not a re-encoded mp4 (no double generation loss).
- **`linear=true`** in pass 2 — clean gain, no pumping (the loop's LRA is tiny).
- Keep video quality: `libx264 -crf 23 -preset medium -pix_fmt yuv420p
  -movflags +faststart`, audio `aac -b:a 256k`.
- If the source already clips hard (input_tp well over 0, many `histogram_0db`
  samples), say so — mastering controls peaks but can't undo baked-in clipping.
  The real fix is to **lower the VCV AUDIO/GHOST MIX master and re-record with
  headroom**, not to limit harder.

## Steps

1. **Resolve input + run `date` for the output timestamp** (never trust the
   model clock for filenames):
   ```bash
   IN="${1:-$(ls -t /tmp/ghost_demo*.mov 2>/dev/null | head -1)}"
   I="${2:--11}"; TP="${3:--1.0}"
   TS=$(date +%Y%m%d-%H%M)
   test -f "$IN" || { echo "no input file"; exit 1; }
   ```

2. **Pass 1 — measure** (parse the JSON block):
   ```bash
   ffmpeg -hide_banner -i "$IN" \
     -af loudnorm=I=$I:TP=$TP:LRA=7:print_format=json -f null - 2>&1 | sed -n '/{/,/}/p'
   ```
   Extract `input_i`, `input_tp`, `input_lra`, `input_thresh`, `target_offset`.

3. **Pass 2 — apply measured values + encode** (substitute the measured numbers):
   ```bash
   ffmpeg -y -i "$IN" \
     -af "loudnorm=I=$I:TP=$TP:LRA=7:measured_I=<input_i>:measured_TP=<input_tp>:measured_LRA=<input_lra>:measured_thresh=<input_thresh>:offset=<target_offset>:linear=true" \
     -c:v libx264 -preset medium -crf 23 -pix_fmt yuv420p -movflags +faststart \
     -c:a aac -b:a 256k "/tmp/ghost_demo_loud.mp4"
   ```
   (Audio-only input: drop the `-c:v ...` video flags.)

4. **Verify + report a before/after table** — integrated LUFS, mean, peak:
   ```bash
   ffmpeg -hide_banner -i /tmp/ghost_demo_loud.mp4 -af ebur128 -f null - 2>&1 | grep -A3 'Integrated loudness' | head -5
   ffmpeg -hide_banner -i /tmp/ghost_demo_loud.mp4 -af volumedetect -f null - 2>&1 | grep -iE 'mean_volume|max_volume'
   ```
   Confirm output is within ~1 LUFS of target and peak ≤ target_TP.

5. **Save to Desktop + reveal**:
   ```bash
   cp /tmp/ghost_demo_loud.mp4 "$HOME/Desktop/ghost-demo-loud-$TS.mp4"
   open -R "$HOME/Desktop/ghost-demo-loud-$TS.mp4"
   ```
   Report the path. Offer to A/B (`open` it) or post to Discord (#vcv-rack-devs).
