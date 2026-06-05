---
description: Stack a real-world footage band over a VCV rack performance — the GHOST NOTES signature look
---

Build the GHOST "context band" composite: a horizontal slice of real-world
footage on top (~1/3), the VCV rack performance below (~2/3), with the rack's
audio driving the whole clip. Gives a patch a *world* so it reads as a real
track, not a void-floating demo. See `[[project_ghost_context_band_format]]`.
Pick footage from `doc/footage-palette.md`.

**Args:** `$ARGUMENTS` = `<footage: URL or file> <our-vcv.mp4> [slice] [width]`
- `footage` — a YouTube URL (downloaded video-only, no playlist) or a local file.
- `our-vcv.mp4` — the VCV capture (ideally already mastered via `/master-vcv-demo`).
- `slice` — `top` | `middle` (default) | `bottom`: which horizontal third of the
  footage to take.
- `width` — output width, default `1280`.

**No auto-sync.** Do NOT beat-detect or cut the band to the kit. The footage
locks into the groove naturally; humans make that connection. Keep it a static
band.

## Critical rules
- **De-pillarbox the footage first.** Music/archival clips are often pillarboxed
  inside 1920 with black side bars; `cropdetect` finds the real content width.
- **Crop the VCV to module bounds, not the window.** Strip the macOS title bar
  AND the dark rack-rail strip above/below the modules. That rail is dark gray
  (~luma 30), NOT pure black, so `cropdetect` misses it — measure the row-luma
  profile and clamp to where luma jumps to module level.
- **Take a horizontal section; never stretch/justify** the footage slice.
- **Montage footage warning:** `cropdetect` samples one moment, but a montage's
  shots have *different* pillarbox amounts — one de-pillarbox rect can't fit all.
  Prefer single-shot footage, or pass a time-trimmed clip of one consistent shot.
- Map **the VCV audio only**; footage is silent. `-shortest`.

## Run

```bash
FOOT="$1"; OURS="$2"; SLICE="${3:-middle}"; W="${4:-1280}"
TS=$(date +%Y%m%d-%H%M)

# 1. resolve footage (download video-only if a URL)
case "$FOOT" in
  http*) rm -f /tmp/gcb_foot.*; \
    yt-dlp --no-playlist -f "bv[height<=1080]/bestvideo/best" \
      -o "/tmp/gcb_foot.%(ext)s" "$FOOT"; \
    FOOT=$(ls /tmp/gcb_foot.* | head -1);;
esac

# 2. de-pillarbox the footage (most-common detected content rect)
PB=$(ffmpeg -hide_banner -ss 20 -i "$FOOT" -vf cropdetect=24:2:0 -t 4 -f null - 2>&1 \
     | grep -oE 'crop=[0-9]+:[0-9]+:[0-9]+:[0-9]+' | sort | uniq -c | sort -rn \
     | head -1 | grep -oE 'crop=[0-9:]+')
[ -z "$PB" ] && PB="crop=iw:ih:0:0"

# 3. measure VCV module bounds (row-luma; skip the bright title bar)
ffmpeg -hide_banner -y -ss 20 -i "$OURS" -vf "scale=1:ih,format=gray" \
  -frames:v 1 -f rawvideo /tmp/gcb_rows.gray 2>/dev/null
read VT VB < <(python3 - <<'PY'
d=list(open('/tmp/gcb_rows.gray','rb').read()); H=len(d)
skip=int(H*0.05)               # clear the bright macOS title bar / bottom border
thr=45                          # module content is brighter than rack rail (~30)
top=next((i for i in range(skip,H) if d[i]>thr), skip)
bot=next((i for i in range(H-1-skip,top,-1) if d[i]>thr), H-1-skip)
print(top+2, bot-2)             # nudge inward off the edge rail
PY
)
VH=$((VB - VT))
echo "footage: $PB  |  vcv modules: y=$VT..$VB (h=$VH)  |  slice=$SLICE"

# 4. slice y-offset for the footage band
case "$SLICE" in top) SY=0;; bottom) SY="2*ih/3";; *) SY="ih/3";; esac

# 5. compose: footage band over vcv, vcv audio drives it
ffmpeg -y -i "$FOOT" -i "$OURS" -filter_complex \
  "[0:v]$PB,crop=iw:ih/3:0:$SY,scale=$W:-2,fps=30,setsar=1[t];\
   [1:v]crop=in_w:$VH:0:$VT,scale=$W:-2,fps=30,setsar=1[b];\
   [t][b]vstack=inputs=2[v]" \
  -map "[v]" -map 1:a -c:v libx264 -preset medium -crf 20 -pix_fmt yuv420p \
  -movflags +faststart -c:a aac -b:a 256k -shortest "$HOME/Desktop/ghost-band-$TS.mp4"

open "$HOME/Desktop/ghost-band-$TS.mp4"
```

## Verify (always)
Sample an output frame and look — confirm **no dark band** on either video edge
and the footage fills the width (no pillarbox bars):
```bash
ffmpeg -hide_banner -y -ss 25 -i "$HOME/Desktop/ghost-band-$TS.mp4" -frames:v 1 /tmp/gcb_check.png
```
If a dark band remains on the VCV, the auto-detected `VT`/`VB` were off — print
the row-luma profile (`scale=1:ih,format=gray`, dump every 40px) and clamp by hand.
