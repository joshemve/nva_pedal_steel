"""Native Cycles knob frames (fixed lights, rotating geometry: knob_studio.py + native_v2.json) -> production
filmstrips strips/knob_<style>_orig.png in the existing strips.json format (128 frames, -135..135, 2x, centred).
fix_knob_halo.py then adds the panel contact shadow (static, measured from the reference) and writes knob_<style>.png.
Joshua: "the knobs don't rotate with proper lighting and shadows staying in place" - the previous strips were rotated
reference pixels; these are real renders, so highlights and shading stay put while the knob turns.
python native_to_strips.py native_v2"""
import json, os, shutil, sys
from pathlib import Path
from PIL import Image
H = Path(__file__).resolve().parent; ST = H.parent / "strips"
d = (H / sys.argv[1]).resolve(); meta = json.loads((d / "render.json").read_text())
S = json.load(open(ST / "strips.json")); sc = meta["scale"]; n = len(meta["angles"])
assert abs(meta["angles"][0] + 135) < 1e-6 and abs(meta["angles"][-1] - 135) < 1e-6
for j, st in enumerate(meta["styles"]):
    if st not in ("scallop", "space"): continue
    m = S[st]; assert m["frames"] == n and m["scale"] == sc
    fs = m["frame_px"][0]; cx = meta["centres"][j] * sc; cy = meta["centre_y"] * sc
    strip = Image.new("RGBA", (fs, fs * n))
    for k in range(n):
        fr = Image.open(d / ("rotation_%04d.png" % k)).convert("RGBA")
        strip.paste(fr.crop((cx - fs // 2, cy - fs // 2, cx - fs // 2 + fs, cy - fs // 2 + fs)), (0, k * fs))
    orig = ST / ("knob_%s_orig.png" % st); keep = ST / ("knob_%s_refrot.png" % st)
    if orig.exists() and not keep.exists(): shutil.copy(orig, keep)   # the rotated-reference version, kept
    strip.save(orig, optimize=True); print("wrote", orig, fs, n)
