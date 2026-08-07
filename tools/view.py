#!/usr/bin/env python3
"""Live panel viewer. Renders a module's panel and opens it in the browser on a
neutral background; the page reloads the SVG every second, so after you edit a
YAML and re-render (`panelkit render <Module>`), the view updates.

Usage:  .venv/bin/python tools/view.py [Module]   # default: Kck
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
os.chdir(ROOT)                      # panelkit resolves the repo from the cwd
from panelkit.cli import render   # noqa: E402

module = sys.argv[1] if len(sys.argv) > 1 else "Kck"
render(module, preview=False)                      # (re)generate res/<Module>.svg

svg = ROOT / "res" / f"{module}.svg"
page = ROOT / "tools" / "_preview" / f"{module}_view.html"
page.parent.mkdir(parents=True, exist_ok=True)
page.write_text(f"""<!doctype html><html><head><meta charset="utf-8">
<title>{module} — panelkit</title>
<style>
  html,body {{ margin:0; height:100%; background:#7a7a7a;
    display:flex; flex-direction:column; align-items:center; justify-content:center;
    font-family:-apple-system,system-ui,sans-serif; }}
  img {{ height:92vh; background:#1a1a1a; box-shadow:0 10px 50px #0009; }}
  .cap {{ color:#fff; opacity:.65; font-size:12px; margin-top:10px; }}
  code {{ background:#0003; padding:2px 6px; border-radius:4px; }}
</style></head>
<body>
  <img id="panel">
  <div class="cap">{module}.svg — live; edit a YAML then
    <code>panelkit render {module}</code></div>
  <script>
    const src = "file://{svg}";
    function reload() {{ document.getElementById("panel").src = src + "?t=" + Date.now(); }}
    reload(); setInterval(reload, 1000);
  </script>
</body></html>""")

subprocess.run(["open", str(page)], check=True)
print(f"viewing {module} -> {page.relative_to(ROOT)}")
