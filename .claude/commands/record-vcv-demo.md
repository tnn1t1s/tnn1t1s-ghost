---
description: Record a VCV Rack patch (screen + clean audio) to a shareable mp4
---

Record a running VCV Rack patch to an mp4 with clean stereo audio, using macOS
`screencapture` + BlackHole. Self-contained — full reference is `doc/recording.md`.

**Args:** `$ARGUMENTS` = optional `<patch.vcv path or demo name> <seconds>`
(default: `04-hypnotic` and `28`). A bare name resolves to
`$HOME/Development/tnn1t1s-ghost/patches/909-demos/<name>.vcv`.

## Critical rules — do NOT deviate
- Use **`screencapture`**, never ffmpeg/avfoundation (avfoundation = warbly audio
  with BlackHole).
- Capture **`BlackHole2ch_UID`**, fed from VCV via the **Multi-Output Device** (so
  the user also hears it on Volt).
- Use timed **`-V <secs>`** so the file self-finalizes. NEVER background + SIGINT.

## Steps

1. **Preflight** — confirm the devices exist; abort with a clear message if not:
   ```bash
   system_profiler SPAudioDataType 2>/dev/null | grep -iE "BlackHole|Multi-Output" || echo "MISSING: install blackhole-2ch + create Multi-Output Device"
   ```

2. **Launch the patch + keep the display awake**:
   ```bash
   caffeinate -d -t 180 &
   open "<resolved patch path>"
   ```
   Wait ~6s, confirm Rack is running (`pgrep -f "VCV Rack"`).

3. **Hand the audio-routing step to the user** (a Rack UI click you can't do
   reliably): ask them to set the AudioInterface2 module's device to
   **Multi-Output Device** and confirm they hear it on Volt. WAIT for their "go"
   before recording — the demo patches ship with no device selected, so until
   this is done there is no audio to capture.

4. **Window bounds**:
   ```bash
   osascript -e 'tell application "System Events" to tell process "VCV Rack 2 Pro" to get {position, size} of front window'
   ```
   Parse `x, y, w, h`.

5. **Record** (timed):
   ```bash
   rm -f /tmp/ghost_demo.mov
   screencapture -v -V <secs> -R "<x>,<y>,<w>,<h>" -G "BlackHole2ch_UID" -x /tmp/ghost_demo.mov
   ```

6. **Verify** — must be `channels=2` and ~`<secs>`; if mono or 0s, the routing
   was wrong (Volt input is mono) — fix routing and re-record:
   ```bash
   ffprobe -v error -select_streams a:0 -show_entries stream=channels,sample_rate -show_entries format=duration /tmp/ghost_demo.mov
   ```

7. **Export + keep**:
   ```bash
   ffmpeg -y -i /tmp/ghost_demo.mov -c:v libx264 -preset medium -crf 23 \
     -pix_fmt yuv420p -movflags +faststart -c:a aac -b:a 256k /tmp/ghost_demo.mp4
   cp /tmp/ghost_demo.mp4 $HOME/Desktop/   # /tmp clears on reboot
   ```
   Report the Desktop path. (Optional trim before export — see doc/recording.md.)
