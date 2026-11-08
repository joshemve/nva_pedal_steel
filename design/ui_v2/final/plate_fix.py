"""Match the static shading around each deck knob (contact shadow / dark halo) to APPROVED_main.png.
python final/plate_fix.py
The plate does not rotate, so its low-frequency tone can be taken from the reference directly: in an annulus outside
the knob's largest radius, plate *= blur(ref)/blur(plate) (ticks and text masked out of both, so they stay sharp).
Keeps the untouched render as strips/plate_walnut_raw.png and writes strips/plate_walnut.png (+ knobs/_plate_fix.png)."""
import json, os, shutil
import numpy as np
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__)); S = os.path.join(H, "strips")
RAW = os.path.join(S, "plate_walnut_raw.png")
if not os.path.exists(RAW): shutil.copy(os.path.join(S, "plate_walnut.png"), RAW)
CS = {"__file__": os.path.join(H, "correct_strips.py"), "__name__": "cs"}
exec(open(CS["__file__"]).read().split("\nfor st in")[0], CS)
gblur = CS["gblur"]
plate = np.asarray(Image.open(RAW).convert("RGB"), float); plate0 = plate.copy()
ref = np.asarray(Image.open(os.path.join(os.path.dirname(H), "APPROVED_main.png")).convert("RGB").resize((3200, 2000), Image.LANCZOS), float)
J = json.load(open(os.path.join(S, "strips.json")))
TICK = {"dome": (1.3, 1.75), "scallop": (1.18, 1.5), "space": (1.2, 1.6)}
OUTER = {"dome": 1.18 * 1.03, "scallop": 1.02, "space": 1.02}   # knob's largest radius / r (dome: knurled skirt)
sc = 2.0
def smooth(a, lo, hi): t = np.clip((a - lo) / (hi - lo), 0, 1); return t * t * (3 - 2 * t)
# 1) panel tone over the whole deck panel, broad scale (knob disks, ticks, text, chrome excluded)
px0, px1, py0, py1 = int(25 * 1600 / 1568 * sc), int(1543 * 1600 / 1568 * sc), int(688 * sc), int(970 * sc)
P = plate[py0:py1, px0:px1]; Rf = ref[py0:py1, px0:px1]
yy, xx = np.mgrid[py0:py1, px0:px1]; far = np.ones(P.shape[:2], bool)
for st in ("dome", "scallop", "space"):
    for inst in J[st]["instances"]: far &= np.hypot(xx - inst["cx"] * sc, yy - inst["cy"] * sc) > 1.25 * inst["r"] * sc * (1.18 if st == "dome" else 1)
use = far & (Rf.mean(2) < 90) & (P.mean(2) < 90)
sig = 30 * sc; w = gblur(use.astype(float), sig) + 1e-6
Lg = np.clip(np.stack([gblur(Rf[..., c] * use, sig) / w / np.maximum(gblur(P[..., c] * use, sig) / w, 1) for c in range(3)], -1), 0.5, 1.8)
edge = np.minimum.reduce([xx - px0, px1 - 1 - xx, yy - py0, py1 - 1 - yy]) / (8 * sc)
wt = (np.clip(edge, 0, 1) * (1 - smooth(P.mean(2), 85, 140)))[..., None]
out = plate.copy(); out[py0:py1, px0:px1] = P * (1 + wt * (Lg - 1)); plate = out.copy()
print("panel tone ratio (median rgb):", np.round(np.median(Lg[use], 0), 3))
# 2) local halo around each knob
for st in ("dome", "scallop", "space"):
    for inst in J[st]["instances"]:
        cx, cy, r = inst["cx"] * sc, inst["cy"] * sc, inst["r"] * sc
        x0, x1, y0, y1 = int(cx - 2.6 * r), int(cx + 2.6 * r), int(cy - 2.0 * r), int(cy + 2.8 * r)
        P = plate[y0:y1, x0:x1]; yy, xx = np.mgrid[y0:y1, x0:x1]; rr = np.hypot(xx - cx, yy - cy)
        # align the reference on the printed tick ring: least-squares circle through bright tick pixels, both images
        def ring_centre(img, ox, oy):
            Y, X = np.mgrid[0:img.shape[0], 0:img.shape[1]]; d = np.hypot(X + ox - cx, Y + oy - cy)
            ang = np.degrees(np.arctan2(X + ox - cx, -(Y + oy - cy)))
            m = (img.mean(2) > 110) & (d > TICK[st][0] * r) & (d < TICK[st][1] * r) & (np.abs(ang) < 140)
            xs, ys = X[m] + ox, Y[m] + oy
            A = np.c_[2 * xs, 2 * ys, np.ones(len(xs))]; b = xs ** 2 + ys ** 2
            c = np.linalg.lstsq(A, b, rcond=None)[0]; return c[0], c[1], np.sqrt(c[2] + c[0] ** 2 + c[1] ** 2)
        pcx, pcy, pr = ring_centre(P, x0, y0)
        W = 30; rcx, rcy, rr_ = ring_centre(ref[y0 - W:y1 + W, x0 - W:x1 + W], x0 - W, y0 - W)
        dx, dy = int(round(rcx - pcx)), int(round(rcy - pcy))
        print("   tick ring: plate %.1f,%.1f r%.1f   ref %.1f,%.1f r%.1f" % (pcx / sc, pcy / sc, pr / sc, rcx / sc, rcy / sc, rr_ / sc))
        Rf = ref[y0 + dy:y1 + dy, x0 + dx:x1 + dx]
        rin = OUTER[st] * r
        use = (rr > rin + 1.5 * sc) & (Rf.mean(2) < 105) & (P.mean(2) < 105)   # panel only: no ticks, text, knob
        sig = 4 * sc; w = gblur(use.astype(float), sig) + 1e-6
        L = np.stack([gblur(Rf[..., c] * use, sig) / w / np.maximum(gblur(P[..., c] * use, sig) / w, 1) for c in range(3)], -1)
        L = np.clip(L, 0.3, 2.0)
        t = np.clip((rr - rin) / (2 * sc), 0, 1); fo = np.clip((1.9 * r - rr) / (0.5 * r), 0, 1)
        wt = (t * t * (3 - 2 * t) * fo * (1 - smooth(P.mean(2), 85, 140)))[..., None]
        out[y0:y1, x0:x1] = P * (1 + wt * (L - 1))
        print("%-8s at %4.0f,%4.0f ref shift %+d,%+d" % (st, cx / sc, cy / sc, dx, dy))
Image.fromarray(out.clip(0, 255).astype(np.uint8)).save(os.path.join(S, "plate_walnut.png"))
y0, y1, x0, x1 = 690 * 2, 870 * 2, 40 * 2, 960 * 2
Image.fromarray(np.concatenate([ref[y0:y1, x0:x1], plate0[y0:y1, x0:x1], out[y0:y1, x0:x1]], 0).clip(0, 255).astype(np.uint8)).save(os.path.join(H, "knobs", "_plate_fix.png"))
