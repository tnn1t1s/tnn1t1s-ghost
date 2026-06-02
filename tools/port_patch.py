#!/usr/bin/env python3
"""Port an AgentRack-era .vcv patch to the TNN1T1S Ghost plugin.

The 909 voices were extracted from AgentRack into the standalone `tnn1t1s-ghost`
plugin. New CV inputs were appended *after* the original trigger/accent/output
ports, so existing cabling (the part that makes a patch play) maps unchanged --
a plain plugin-name swap is enough. Three modules changed their param surface
(RimClap gained TUNE, Toms gained per-voice TUNE/DECAY, GhostCtrl dropped
DEFAULT); their params are reset to defaults so knobs land sensibly.

Usage: python tools/port_patch.py <src.vcv> <dst.vcv>
"""
import json, subprocess, sys, tempfile, pathlib

GHOST_909 = {"Kck", "Snr", "ChhOhh", "RimClap", "Toms", "CrashRide", "GhostCtrl"}
RESET_PARAMS = {"RimClap", "Toms", "GhostCtrl"}   # param surface changed -> use new defaults


def read_patch(vcv: str) -> dict:
    raw = subprocess.run(["bash", "-c", f'zstd -dc "{vcv}" | tar -xO ./patch.json'],
                         capture_output=True).stdout
    return json.loads(raw)


def write_patch(patch: dict, vcv: str) -> None:
    with tempfile.TemporaryDirectory() as d:
        (pathlib.Path(d) / "patch.json").write_text(json.dumps(patch, indent=2))
        subprocess.run(["bash", "-c", f'tar -C "{d}" -cf - . | zstd -q -o "{vcv}" -f'], check=True)


def port(src: str, dst: str) -> dict:
    patch = read_patch(src)
    swapped, reset = 0, 0
    for m in patch.get("modules", []):
        if m.get("plugin") == "AgentRack" and m.get("model") in GHOST_909:
            m["plugin"] = "tnn1t1s-ghost"
            swapped += 1
            if m.get("model") in RESET_PARAMS:
                m.pop("params", None)   # let VCV apply the module's new configParam defaults
                reset += 1
    write_patch(patch, dst)
    return {"swapped": swapped, "reset": reset, "modules": len(patch.get("modules", []))}


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__); sys.exit(1)
    info = port(sys.argv[1], sys.argv[2])
    print(f"ported {sys.argv[1]} -> {sys.argv[2]}: "
          f"{info['swapped']} voices swapped, {info['reset']} param-reset, "
          f"{info['modules']} modules total")
