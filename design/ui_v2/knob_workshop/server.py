"""Knob Workshop: local page for tweaking a knob recipe live.

python server.py [port]        ->  http://127.0.0.1:8765

  GET  /api/recipes            list of recipe names
  GET  /api/recipe/<name>      recipes/<name>.json
  PUT  /api/recipe/<name>      save (the page autosaves; "Finished" sets status = finished)
  GET/PUT /api/versions/<name> kept versions of a recipe (thumbnail + recipe), versions/<name>.json
  GET  /files/<path>           an image under design/ui_v2 (panel plate, reference, strips)
Everything else is served from this folder. Bound to 127.0.0.1 only.
"""
import json, re, sys, threading, time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent                      # design/ui_v2
RECIPES = HERE / "recipes"
VERSIONS = HERE / "versions"
NAME = re.compile(r"^[a-z0-9_\-]{1,40}$")
WRITE = threading.Lock()   # saves arrive on several threads; Windows refuses a replace while another is mid-write
IMAGES = {".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg"}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=str(HERE), **k)

    def log_message(self, *a):
        pass

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _recipe_path(self):
        name = self.path.split("?")[0].rsplit("/", 1)[-1]
        return RECIPES / (name + ".json") if NAME.match(name) else None

    def end_headers(self):
        if not self.path.startswith("/files/"):
            self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/api/recipes":
            return self._json(200, sorted(p.stem for p in RECIPES.glob("*.json")))
        if path.startswith("/api/recipe/"):
            p = self._recipe_path()
            if not p or not p.exists(): return self._json(404, {"error": "no such recipe"})
            return self._json(200, json.loads(p.read_text()))
        if path.startswith("/api/versions/"):
            name = path.rsplit("/", 1)[-1]; p = VERSIONS / (name + ".json")
            return self._json(200, json.loads(p.read_text()) if NAME.match(name) and p.exists() else [])
        if path.startswith("/files/"):
            target = (ROOT / path[len("/files/"):]).resolve()
            if ROOT not in target.parents or target.suffix.lower() not in IMAGES or not target.is_file():
                return self.send_error(404)
            data = target.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", IMAGES[target.suffix.lower()])
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            return self.wfile.write(data)
        return super().do_GET()

    def do_PUT(self):
        if self.path.startswith("/api/versions/"):
            name = self.path.split("?")[0].rsplit("/", 1)[-1]
            try:
                data = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
                assert NAME.match(name) and isinstance(data, list)
            except Exception:
                return self._json(400, {"error": "bad versions"})
            VERSIONS.mkdir(exist_ok=True)
            with WRITE: (VERSIONS / (name + ".json")).write_text(json.dumps(data))
            return self._json(200, {"ok": True})
        p = self._recipe_path() if self.path.startswith("/api/recipe/") else None
        if not p: return self._json(400, {"error": "bad recipe name"})
        try:
            recipe = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            assert isinstance(recipe, dict) and "parts" in recipe and "pointer" in recipe
        except Exception:
            return self._json(400, {"error": "bad recipe"})
        recipe["saved_at"] = time.strftime("%Y-%m-%d %H:%M:%S")
        RECIPES.mkdir(exist_ok=True)
        with WRITE:
            tmp = p.with_suffix(".tmp")
            tmp.write_text(json.dumps(recipe, indent=1))
            for attempt in range(5):   # a reader (or antivirus) can hold the file for a moment
                try: tmp.replace(p); break
                except PermissionError:
                    if attempt == 4: return self._json(503, {"error": "file busy"})
                    time.sleep(0.05)
        return self._json(200, {"ok": True, "saved_at": recipe["saved_at"]})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print("Knob Workshop: http://127.0.0.1:%d" % port, flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
