"""Finished workshop knob -> the plugin's filmstrip, for ONE style (the other knobs' strips are left alone).

python install_strip.py <render_dir> [style=scallop]

<render_dir> is a knob_studio.py output (rotation_0000.png .. + render.json) made from recipe_to_studio.py's config.
Steps: crop the style's frames into strips/knob_<style>_orig.png (same format as native_to_strips.py), run
final/fix_knob_halo.py (static contact shadow measured from the reference -> strips/knob_<style>.png), then copy that
one file to Source/ui_assets/. The previous strips are kept once as knob_<style>*_before_workshop.png. Frame size and
count are unchanged, so Source/UiLayout.h does not need regenerating; rebuild the plugin to embed the new strip.
"""
import hashlib, json, shutil, subprocess, sys
from pathlib import Path
from PIL import Image

H = Path(__file__).resolve().parent
FINAL = H.parent / "final"; ST = FINAL / "strips"; ASSETS = H.parents[2] / "Source" / "ui_assets"
d = Path(sys.argv[1]).resolve(); style = sys.argv[2] if len(sys.argv) > 2 else "scallop"
meta = json.loads((d / "render.json").read_text()); S = json.loads((ST / "strips.json").read_text())
sc = meta["scale"]; n = len(meta["angles"]); m = S[style]; j = meta["styles"].index(style)
assert abs(meta["angles"][0] + 135) < 1e-6 and abs(meta["angles"][-1] - 135) < 1e-6
assert m["frames"] == n and m["scale"] == sc, (m["frames"], n, m["scale"], sc)
fs = m["frame_px"][0]; cx = meta["centres"][j] * sc; cy = meta["centre_y"] * sc

md5 = lambda p: hashlib.md5(p.read_bytes()).hexdigest()
others = {p.name: md5(p) for p in ST.glob("knob_*.png") if style not in p.name}
for name in ("knob_%s.png" % style, "knob_%s_orig.png" % style):          # keep what was there, once
    keep = ST / name.replace(".png", "_before_workshop.png")
    if (ST / name).exists() and not keep.exists(): shutil.copy(ST / name, keep)

strip = Image.new("RGBA", (fs, fs * n))
edge = 0
for k in range(n):
    fr = Image.open(d / ("rotation_%04d.png" % k)).convert("RGBA")
    tile = fr.crop((cx - fs // 2, cy - fs // 2, cx - fs // 2 + fs, cy - fs // 2 + fs))
    a = tile.getchannel("A"); w = a.width
    edge = max(edge, max(a.crop((0, 0, w, 1)).getextrema()[1], a.crop((0, w - 1, w, w)).getextrema()[1],
                         a.crop((0, 0, 1, w)).getextrema()[1], a.crop((w - 1, 0, w, w)).getextrema()[1]))
    strip.paste(tile, (0, k * fs))
assert edge < 8, "the knob touches the frame edge (alpha %d): it is too big for the %d px frame" % (edge, fs)
strip.save(ST / ("knob_%s_orig.png" % style), optimize=True)
print("wrote", ST / ("knob_%s_orig.png" % style), fs, n)

subprocess.run([sys.executable, str(FINAL / "fix_knob_halo.py")], check=True, cwd=str(FINAL))
changed = [name for name, h in others.items() if md5(ST / name) != h]
print("other knob strips changed by the halo step:", changed or "none")
shutil.copy(ST / ("knob_%s.png" % style), ASSETS / ("knob_%s.png" % style))
print("installed", ASSETS / ("knob_%s.png" % style))
