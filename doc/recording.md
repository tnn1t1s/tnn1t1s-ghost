# Recording a GHOST demo (screen + audio → mp4)

How to capture a VCV Rack window with **clean** audio on macOS. Same technique as
the rcy rig runbook, adapted for VCV: the audio source is VCV's AudioInterface2
(via the Multi-Output Device), not Logic. There's a `/record-vcv-demo` skill that
automates the mechanical parts; this doc is the canonical reference.

## Critical rules (don't deviate)

- Use macOS **`screencapture`**, NOT ffmpeg/avfoundation — avfoundation produces
  warbly/clicky audio with BlackHole (clock-sync issue). screencapture's native
  audio path handles BlackHole correctly.
- Capture **BlackHole** (the loopback), routed from VCV via a **Multi-Output
  Device**, so you also hear it on Volt while recording.
- Use timed **`-V <seconds>`** so the file self-finalizes. Do NOT background +
  SIGINT (truncates the .mov).
- Don't use the VCV Recorder module (maxes CPU).

## Prereqs (already configured on this Mac)

- `brew install blackhole-2ch` → **BlackHole 2ch** present.
- A **Multi-Output Device** in Audio MIDI Setup combining **Volt 2** (primary,
  drift master) + **BlackHole 2ch** (Drift Correction on BlackHole).
- Terminal has Screen Recording permission (System Settings ▸ Privacy).
- CoreAudio UIDs: BlackHole = `BlackHole2ch_UID`; Volt 2 =
  `AppleUSBAudioEngine:Universal Audio:Volt 2:<VOLT_SERIAL>:1,2`.

## Steps

1. **Launch the patch** (keep the display awake while recording):
   ```bash
   caffeinate -d -t 120 &
   open "$HOME/Development/tnn1t1s-ghost/patches/909-demos/04-hypnotic.vcv"
   ```

2. **Route VCV audio → Multi-Output Device** (manual, one click; the demo patches
   ship with no device selected so they stay portable). On the AudioInterface2
   module, set the device to **Multi-Output Device**. Confirm you hear it on Volt.

3. **Get the Rack window bounds**:
   ```bash
   osascript -e 'tell application "System Events" to tell process "VCV Rack 2 Pro" to get {position, size} of front window'
   # -> x, y, w, h   (e.g. 510, 25, 1406, 987)
   ```

4. **Record** (timed; substitute bounds + seconds):
   ```bash
   rm -f /tmp/ghost_demo.mov
   screencapture -v -V 28 -R "510,25,1406,987" -G "BlackHole2ch_UID" -x /tmp/ghost_demo.mov
   ```

5. **Verify** (must be `channels=2`, ~duration):
   ```bash
   ffprobe -v error -select_streams a:0 \
     -show_entries stream=channels,sample_rate -show_entries format=duration \
     /tmp/ghost_demo.mov
   ```

6. **(Optional) trim** (e.g. 4s–24s), then **export a shareable mp4**:
   ```bash
   ffmpeg -y -ss 4 -to 24 -i /tmp/ghost_demo.mov \
     -c:v h264 -pix_fmt yuv420p -c:a aac -b:a 256k /tmp/ghost_trim.mov \
     && mv /tmp/ghost_trim.mov /tmp/ghost_demo.mov
   ffmpeg -y -i /tmp/ghost_demo.mov \
     -c:v libx264 -preset medium -crf 23 -pix_fmt yuv420p -movflags +faststart \
     -c:a aac -b:a 256k /tmp/ghost_demo.mp4
   cp /tmp/ghost_demo.mp4 $HOME/Desktop/   # /tmp clears on reboot
   ```

## Fallback — raw Volt input (mono, no Multi-Output)

If the Multi-Output route isn't available, capture the Volt hardware input
directly (note: the Volt **input** is mono → one-sided audio; prefer the
Multi-Output route above):
```bash
screencapture -v -V 28 -R "510,25,1406,987" \
  -G "AppleUSBAudioEngine:Universal Audio:Volt 2:<VOLT_SERIAL>:1,2" -x /tmp/ghost_demo.mov
# if a UID changed: SwitchAudioSource -a -t input -f json
```

## What did NOT work (so we don't relitigate)

- ffmpeg + avfoundation + BlackHole → warbly/sped-up audio (clock sync).
- VCV Recorder module → CPU maxed.
- Multi-Output with ffmpeg → same warble. Native `screencapture` is the only
  clean path.
