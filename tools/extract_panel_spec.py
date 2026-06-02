#!/usr/bin/env python3
"""Extract a machine-readable panel spec for every Ghost module.

Source of truth is the module C++ itself: the ParamId/InputId/OutputId enums
(for stable IDs and order) and the configParam/configInput/configOutput calls
(for names, ranges, defaults, units). One object per model slug.

This JSON is consumed by the panel-background generator AND handed to anyone
designing panels by hand, so the live knob/jack widgets line up with the art.

Usage:  python3 tools/extract_panel_spec.py [--out tools/panel_spec.json]
"""
import json, re, sys, pathlib

SRC = pathlib.Path(__file__).resolve().parent.parent / "src"
PLUGIN_JSON = pathlib.Path(__file__).resolve().parent.parent / "plugin.json"
FILES = ["Kck.cpp", "Snr.cpp", "ChhOhh.cpp", "RimClap.cpp", "Toms.cpp",
         "CrashRide.cpp", "GhostCtrl.cpp"]

NUM = re.compile(r"^NUM_")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def match_braces(text, open_idx):
    """Return index just past the matching close brace for the '{' at open_idx."""
    depth = 0
    for i in range(open_idx, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return i + 1
    return len(text)


def parse_enum(body, kind):
    """kind in {ParamId, InputId, OutputId} -> ordered list of member names."""
    m = re.search(r"enum\s+%s\b[^{]*\{" % kind, body)
    if not m:
        return []
    end = match_braces(body, m.end() - 1)
    inner = body[m.end():end - 1]
    members = []
    for tok in inner.split(','):
        name = tok.strip().split('=')[0].strip()
        if not name or NUM.match(name):
            continue
        # strip any trailing comments / whitespace
        name = re.split(r"\s|/", name)[0]
        if name:
            members.append(name)
    return members


def parse_config_params(body):
    """enum_member -> dict(min,max,default,label,unit,displayMult,displayOffset)."""
    out = {}
    for m in re.finditer(
            r"configParam\s*\(\s*([A-Za-z0-9_]+)\s*,([^;]*)\)\s*;", body):
        member = m.group(1)
        args = m.group(2)
        # split top-level args, keeping quoted strings intact
        parts = re.findall(r'"[^"]*"|[^,]+', args)
        parts = [p.strip() for p in parts]
        def num(i, d=None):
            try:
                return float(parts[i].replace('f', '').strip())
            except Exception:
                return d
        label = ""
        for p in parts:
            if p.startswith('"'):
                label = p.strip('"'); break
        # parts: min,max,default,label,unit,base,mult,offset
        strings = [p for p in parts if p.startswith('"')]
        unit = strings[1].strip('"') if len(strings) > 1 else ""
        mn, mx, dflt = num(0), num(1), num(2)
        # displayMultiplier is the 2nd numeric after the unit string (base, mult)
        nums_after = []
        seen_label = False
        for p in parts:
            if p.startswith('"'):
                seen_label = True
                continue
            if seen_label:
                v = num_str(p)
                if v is not None:
                    nums_after.append(v)
        base = nums_after[0] if len(nums_after) > 0 else 0.0
        mult = nums_after[1] if len(nums_after) > 1 else 1.0
        offset = nums_after[2] if len(nums_after) > 2 else 0.0
        out[member] = {
            "label": label, "unit": unit,
            "min": mn, "max": mx, "default": dflt,
            "displayBase": base, "displayMultiplier": mult,
            "displayOffset": offset,
        }
    return out


def num_str(s):
    try:
        return float(s.replace('f', '').strip())
    except Exception:
        return None


def parse_config_ports(body, fn):
    """fn in {configInput, configOutput} -> {member: label}."""
    out = {}
    for m in re.finditer(
            r"%s\s*\(\s*([A-Za-z0-9_]+)\s*,\s*\"([^\"]*)\"" % fn, body):
        out[m.group(1)] = m.group(2)
    return out


def find_hp(text, struct_name):
    """Best-effort HP width from the widget's panelSize_/addScrews_ call."""
    wm = re.search(r"struct\s+%sWidget\b" % re.escape(struct_name), text)
    region = text[wm.start():] if wm else text
    m = re.search(r"panelSize_(\d+)HP|addScrews_(\d+)HP", region[:4000])
    if m:
        return int(m.group(1) or m.group(2))
    return None


def main():
    out_path = "tools/panel_spec.json"
    if "--out" in sys.argv:
        out_path = sys.argv[sys.argv.index("--out") + 1]

    names = {m["slug"]: m["name"]
             for m in json.loads(PLUGIN_JSON.read_text())["modules"]}

    spec = {}
    warnings = []
    for fname in FILES:
        text = (SRC / fname).read_text()
        # map struct -> slug
        slug_of = {}
        for m in re.finditer(
                r"createModel<\s*([A-Za-z0-9_]+)\s*,\s*[A-Za-z0-9_]+\s*>\(\"([^\"]+)\"\)",
                text):
            slug_of[m.group(1)] = m.group(2)

        for struct_name, slug in slug_of.items():
            sm = re.search(r"struct\s+%s\b[^{]*\{" % re.escape(struct_name), text)
            if not sm:
                continue
            end = match_braces(text, sm.end() - 1)
            body = strip_comments(text[sm.end():end])

            pnames = parse_enum(body, "ParamId")
            inames = parse_enum(body, "InputId")
            onames = parse_enum(body, "OutputId")
            pmeta = parse_config_params(body)
            imeta = parse_config_ports(body, "configInput")
            ometa = parse_config_ports(body, "configOutput")

            params = []
            for i, nm in enumerate(pnames):
                md = pmeta.get(nm, {})
                params.append({
                    "id": i, "enum": nm,
                    "label": md.get("label", ""),
                    "unit": md.get("unit", ""),
                    "min": md.get("min"), "max": md.get("max"),
                    "default": md.get("default"),
                    "displayMin": (None if md.get("min") is None
                                   else md["min"] * md.get("displayMultiplier", 1)
                                        + md.get("displayOffset", 0)),
                    "displayMax": (None if md.get("max") is None
                                   else md["max"] * md.get("displayMultiplier", 1)
                                        + md.get("displayOffset", 0)),
                })
            inputs = [{"id": i, "enum": nm, "label": imeta.get(nm, "")}
                      for i, nm in enumerate(inames)]
            outputs = [{"id": i, "enum": nm, "label": ometa.get(nm, "")}
                       for i, nm in enumerate(onames)]

            # validation: config call count should match enum count
            if len(pmeta) != len(pnames):
                warnings.append(f"{slug}: {len(pnames)} params in enum but "
                                f"{len(pmeta)} configParam calls")
            if len(imeta) != len(inames):
                warnings.append(f"{slug}: {len(inames)} inputs in enum but "
                                f"{len(imeta)} configInput calls")

            spec[slug] = {
                "slug": slug,
                "displayName": names.get(slug, slug),
                "tier": ("controller" if slug == "GhostCtrl"
                         else "lab" if slug.endswith("Lab") else "core"),
                "hp": find_hp(text, struct_name),
                "source": fname,
                "counts": {"params": len(params),
                           "inputs": len(inputs),
                           "outputs": len(outputs)},
                "params": params,
                "inputs": inputs,
                "outputs": outputs,
            }

    pathlib.Path(out_path).write_text(json.dumps(spec, indent=2) + "\n")
    print(f"Wrote {out_path}: {len(spec)} modules")
    for slug, d in sorted(spec.items()):
        c = d["counts"]
        print(f"  {slug:14s} {d['displayName']:20s} "
              f"hp={d['hp']}  {c['params']}k / {c['inputs']}in / {c['outputs']}out")
    if warnings:
        print("\nWARNINGS:")
        for w in warnings:
            print("  ! " + w)


if __name__ == "__main__":
    main()
