"""Compose plate + strip frames at the reference angles and compare with APPROVED_main.png, knob by knob.
python final/deck_compare.py [frames_dir] [out.png]
frames_dir: a folder with <style>_####.png (default strips/frames_fixed). Prints mean |err| per knob body."""
import json, os, sys
import numpy as np
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__)); S = os.path.join(H, "strips")
FR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(S, "frames_fixed")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(H, "knobs", "_deck_compare.png")
REF = Image.open(os.path.join(os.path.dirname(H), "APPROVED_main.png")).convert("RGB")
J = json.load(open(os.path.join(S, "strips.json")))
ANG = {("dome", 0): 35, ("dome", 1): -50, ("scallop", 0): 30, ("scallop", 1): 25, ("space", 0): -100, ("space", 1): -68}
plate = np.asarray(Image.open(os.path.join(S, "plate_walnut.png")).convert("RGB"), float)
comp = plate.copy(); sc = 2.0
for (st, i), a in ANG.items():
    m = J[st]; n = m["frames"]; fs = m["frame_px"][0]; inst = m["instances"][i]
    k = int(round((a + 135) / 270 * (n - 1)))
    f = np.asarray(Image.open(os.path.join(FR, "%s_%04d.png" % (st, k))).convert("RGBA"), float)
    x0 = int(round(inst["cx"] * sc - fs / 2)); y0 = int(round(inst["cy"] * sc - fs / 2))
    al = f[..., 3:] / 255
    comp[y0:y0 + fs, x0:x0 + fs] = f[..., :3] * al + comp[y0:y0 + fs, x0:x0 + fs] * (1 - al)
ref = np.asarray(REF.resize((3200, 2000), Image.LANCZOS), float)
# deck region 0..960 x 690..870 UI px
y0, y1, x0, x1 = 690 * 2, 870 * 2, 40 * 2, 960 * 2
row = np.concatenate([ref[y0:y1, x0:x1], comp[y0:y1, x0:x1]], 0)
Image.fromarray(row.clip(0, 255).astype(np.uint8)).save(OUT)
for (st, i), a in ANG.items():
    inst = J[st]["instances"][i]; r = inst["r"] * sc
    yy, xx = np.mgrid[0:2000, 0:3200]; d = np.hypot(xx - inst["cx"] * sc, yy - inst["cy"] * sc)
    msk = d < r * 0.85
    print("%-8s %d  mean|err| body %.1f" % (st, i, np.abs(ref[msk] - comp[msk]).mean()))
np.save(OUT.replace(".png", ".npy"), comp[y0:y1, x0:x1].astype(np.uint8))
