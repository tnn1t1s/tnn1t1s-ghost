#!/usr/bin/env python3
"""Side-by-side panel editor (no external deps).

URL-addressable per module:  http://127.0.0.1:8765/<Module>   (e.g. /Kck, /Snr)
Left: a YAML file (panel spec for the module / shared theme / shared layout).
Right: the rendered panel. Shift+Return saves + re-renders + refreshes;
Cmd/Ctrl+R reloads the file from disk. Errors show inline.

Usage:  .venv/bin/python tools/editor.py        # then open http://127.0.0.1:8765/Kck
"""
import http.server
import json
import os
import pathlib
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
os.chdir(ROOT)                      # panelkit resolves the repo from the cwd
from panelkit import paths          # noqa: E402
from panelkit.cli import render     # noqa: E402

PORT = 8765
DEFAULT = "Kck"
PANELS = paths.repo_root() / "panelkit/specs/panels"
# the theme and layout are carried by the installed panelkit package; editing
# them here edits the toolkit, not this plugin
SHARED = {"theme": paths.theme_path(),
          "layout": paths.layout_defaults_path()}
RESERVED = {"file", "svg", "render", "favicon.ico"}


def modules():
    return sorted(p.name[:-len(".panel.yaml")] for p in PANELS.glob("*.panel.yaml"))


def file_path(module, name):
    return PANELS / f"{module}.panel.yaml" if name == "panel spec" else SHARED[name]


PAGE = """<!doctype html><html><head><meta charset="utf-8">
<title>panelkit — %(mod)s</title>
<style>
  html,body{margin:0;height:100%%;font-family:-apple-system,system-ui,sans-serif;background:#1b1b1d;color:#eee}
  .bar{display:flex;gap:12px;align-items:center;padding:10px 14px;background:#000;border-bottom:1px solid #333}
  .bar b{font-size:15px} select,button{font-size:15px;padding:4px 8px}
  .hint{margin-left:auto;opacity:.6;font-size:13px}
  .wrap{display:flex;height:calc(100vh - 52px)}
  .left{flex:1;display:flex;flex-direction:column;border-right:1px solid #333}
  textarea{flex:1;background:#111;color:#e8e8e8;border:0;padding:14px;
    font-family:ui-monospace,Menlo,monospace;font-size:15px;line-height:1.5;resize:none;tab-size:2}
  .status{padding:8px 14px;font-size:14px;background:#000;white-space:pre-wrap;min-height:20px}
  .ok{color:#7ec77e} .err{color:#ff6b6b}
  .right{flex:1;display:flex;align-items:center;justify-content:center;background:#777}
  img{height:94%%;background:#1a1a1a;box-shadow:0 10px 50px #0009}
</style></head>
<body>
  <div class="bar">
    <b>panelkit</b>
    <select id="mod"></select>
    <select id="file"></select>
    <button id="reload">&#8635; Reload</button>
    <span class="hint">Shift+Return = save &amp; render &middot; &#8984;R / Ctrl+R = reload from disk</span>
  </div>
  <div class="wrap">
    <div class="left">
      <textarea id="ed" spellcheck="false"></textarea>
      <div class="status" id="st"></div>
    </div>
    <div class="right"><img id="panel"></div>
  </div>
<script>
const MODULE = "%(mod)s", MODULES = %(mods)s, FILES = %(files)s;
const ed=document.getElementById('ed'), sel=document.getElementById('file'),
      modSel=document.getElementById('mod'), st=document.getElementById('st'),
      panel=document.getElementById('panel');
MODULES.forEach(m => { const o=document.createElement('option'); o.text=m; o.selected=(m===MODULE); modSel.add(o); });
FILES.forEach(f => { const o=document.createElement('option'); o.text=f; sel.add(o); });
modSel.onchange = () => { location.href = '/' + modSel.value; };
function refresh(){ panel.src = "/svg?module="+MODULE+"&t="+Date.now(); }
async function load(){ const r=await fetch('/file?module='+MODULE+'&name='+encodeURIComponent(sel.value));
  ed.value = await r.text(); }
sel.onchange = load;
async function rerender(){
  st.textContent='rendering…'; st.className='status';
  const r=await fetch('/render', {method:'POST',
    body: JSON.stringify({module:MODULE, name:sel.value, content:ed.value})});
  const j=await r.json();
  if (j.ok){ st.textContent='✓ rendered'; st.className='status ok'; refresh(); }
  else { st.textContent='✗ '+j.error; st.className='status err'; }
}
async function reloadAll(){ await load(); refresh(); st.textContent='↻ reloaded from disk'; st.className='status ok'; }
document.getElementById('reload').onclick = reloadAll;
ed.addEventListener('keydown', e => { if (e.key==='Enter' && e.shiftKey){ e.preventDefault(); rerender(); } });
window.addEventListener('keydown', e => {
  if ((e.metaKey||e.ctrlKey) && e.key.toLowerCase()==='r'){ e.preventDefault(); reloadAll(); } }, true);
load(); refresh();
</script>
</body></html>"""


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype="text/html; charset=utf-8"):
        data = body if isinstance(body, bytes) else body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _page(self, module):
        self._send(200, PAGE % {"mod": module, "mods": json.dumps(modules()),
                                "files": json.dumps(["panel spec", "theme", "layout"])})

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        seg = u.path.strip("/").split("/")[0]
        if u.path == "/file":
            module = q.get("module", [DEFAULT])[0]
            self._send(200, file_path(module, q["name"][0]).read_text(),
                       "text/plain; charset=utf-8")
        elif u.path == "/svg":
            module = q.get("module", [DEFAULT])[0]
            self._send(200, (ROOT / "res" / f"{module}.svg").read_bytes(), "image/svg+xml")
        elif seg in RESERVED:
            self._send(404, "not found")
        else:
            self._page(seg or DEFAULT)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        req = json.loads(self.rfile.read(length) or b"{}")
        try:
            file_path(req["module"], req["name"]).write_text(req["content"])
            render(req["module"], preview=False)
            self._send(200, json.dumps({"ok": True}), "application/json")
        except Exception as e:
            self._send(200, json.dumps({"ok": False, "error": str(e)}), "application/json")


if __name__ == "__main__":
    print(f"panelkit editor: http://127.0.0.1:{PORT}/<Module>  (modules: {', '.join(modules())})")
    http.server.HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
