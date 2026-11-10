"""Per-preset instruments: each chosen ChatGPT take (make_jobs10.py, aligned by align_parts.py) becomes a background
layer over the walnut plate, limited to where the instrument changed, with the strip under the tone bar rebuilt the
same way as on walnut (bar_plate.py) so the live bar can move. Writes skins/skin_<name>.jpg (opaque crop) and
skins/skins.json (canvas boxes). Usage: python skins.py [maple=skin_maple_1 ...]"""
import json, os, sys
import numpy as np, cv2
from PIL import Image
import bar_plate, floor_3d
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
CW, CH = 3200, 2000; RX, RY = CW / 1568, CH / 1003
clean = np.asarray(Image.open("bg_clean.png"), np.float32)
hd = np.asarray(Image.open("bg_hd.png"), np.float32)
# each instrument its own model (Joshua: "make each one feel like its own thing"): table, legs, keyhead per instrument
CHOICE = {"maple": "model_maple_1", "redsparkle": "model_redsparkle_1", "pearloid": "model_pearloid_1", "koa": "model_koa_2"}
for a in sys.argv[1:]: k, v = a.split("="); CHOICE[k] = v
blur = lambda a, s: cv2.GaussianBlur(a, (0, 0), s)
os.makedirs("skins", exist_ok=True); out = {}
lim = np.zeros((CH, CW), np.float32)   # the steel and its room only: never the top bar or the deck
lim[round(104 * RY):round(668 * RY), round(96 * RX):round(1494 * RX)] = 1
lim = blur(lim, 6)
for sk, job in CHOICE.items():
    if not os.path.exists("aligned/%s.png" % job): print("missing", job); continue
    meta = json.load(open("aligned/%s.json" % job)); x0, y0, x1, y1 = meta["canvas_box"]
    g = np.asarray(Image.open("aligned/%s.png" % job), np.float32)
    full = hd.copy()
    x0c, y0c, x1c, y1c = max(x0, 0), max(y0, 0), min(x1, CW), min(y1, CH)
    full[y0c:y1c, x0c:x1c] = g[y0c - y0:y1c - y0, x0c - x0:x1c - x0]
    # where the instrument changed (coarse, so rug/room texture differences don't count), softened
    d = (np.abs(blur(full, 3) - blur(hd, 3)).mean(2) > 9).astype(np.uint8)
    d = cv2.morphologyEx(d, cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
    d = cv2.morphologyEx(d, cv2.MORPH_CLOSE, np.ones((25, 25), np.uint8))
    # thin parts under the neck (pull rods, knee-lever rods, cross bar) vanish in the coarse mask: fine mask there
    fine = (np.abs(blur(full, 1.0) - blur(hd, 1.0)).mean(2) > 22).astype(np.uint8)
    under = np.zeros_like(fine); under[round(395 * RY):round(668 * RY), round(470 * RX):round(960 * RX)] = 1
    d |= cv2.morphologyEx(fine & under, cv2.MORPH_OPEN, np.ones((2, 2), np.uint8))
    a = np.clip(blur(cv2.dilate(d, np.ones((15, 15), np.uint8)).astype(np.float32), 6) * 1.3, 0, 1) * lim
    comp = full * a[..., None] + hd * (1 - a[..., None])
    # this instrument's realistic end pieces (ends_<name>_1: sculpted keyhead, rounded changer)
    ej = "ends_%s_1" % sk
    if job.startswith("skin_") and os.path.exists("aligned/%s.png" % ej):   # the model redraws carry their own ends
        em = json.load(open("aligned/%s.json" % ej)); ex0, ey0, _, _ = em["canvas_box"]
        eg = np.asarray(Image.open("aligned/%s.png" % ej), np.float32)
        for bx_ in ((100, 138, 468, 318), (1278, 138, 1404, 318)):
            sx0, sy0, sx1, sy1 = round(bx_[0] * RX), round(bx_[1] * RY), round(bx_[2] * RX), round(bx_[3] * RY)
            sec = eg[sy0 - ey0:sy1 - ey0, sx0 - ex0:sx1 - ex0]; hh_, ww_ = sec.shape[:2]
            fx = np.clip(np.minimum(np.arange(ww_), ww_ - 1 - np.arange(ww_)) / 12.0, 0, 1)
            fy = np.clip(np.minimum(np.arange(hh_), hh_ - 1 - np.arange(hh_)) / 12.0, 0, 1)
            ff = (fx[None, :] * fy[:, None])[..., None]
            full[sy0:sy1, sx0:sx1] = sec * ff + full[sy0:sy1, sx0:sx1] * (1 - ff)
            a[sy0:sy1, sx0:sx1] = np.maximum(a[sy0:sy1, sx0:sx1], ff[..., 0])
        comp = full * a[..., None] + hd * (1 - a[..., None])
    # the smaller, understated keyhead for this instrument (keyhead_<name>_<take>)
    KH = {"maple": "keyhead_maple_2", "redsparkle": "keyhead_redsparkle_1", "pearloid": "keyhead_pearloid_b1", "koa": "keyhead_koa_2"}
    # then ten straight strings to ten keys (Joshua 2026-11-10: "strings look a little messed up"; the smaller keyheads had
    # 8-9 keys and bent/crossing strings): strings[2]_<name>_<take>, the whole keyhead-to-nut region
    STR = {"maple": "strings_maple_3", "redsparkle": "strings2_redsparkle_3", "pearloid": "strings2_pearloid_3", "koa": "strings3_koa_3"}   # koa: round 3 straightened the bowed strings
    for kj in (KH.get(sk), STR.get(sk)):
      if kj and os.path.exists("aligned/%s.png" % kj):
          km = json.load(open("aligned/%s.json" % kj)); kx0, ky0, _, _ = km["canvas_box"]
          kg = np.asarray(Image.open("aligned/%s.png" % kj), np.float32)
          sx0, sy0, sx1, sy1 = round(96 * RX), round(132 * RY), round(476 * RX), round(322 * RY)
          sec = kg[sy0 - ky0:sy1 - ky0, sx0 - kx0:sx1 - kx0]; hh_, ww_ = sec.shape[:2]
          fx = np.clip(np.minimum(np.arange(ww_), ww_ - 1 - np.arange(ww_)) / 14.0, 0, 1)
          fy = np.clip(np.minimum(np.arange(hh_), hh_ - 1 - np.arange(hh_)) / 14.0, 0, 1)
          ff = (fx[None, :] * fy[:, None])[..., None]
          comp[sy0:sy1, sx0:sx1] = sec * ff + comp[sy0:sy1, sx0:sx1] * (1 - ff)
          a[sy0:sy1, sx0:sx1] = np.maximum(a[sy0:sy1, sx0:sx1], ff[..., 0])
    comp = bar_plate.rebuild(comp)                       # the bar is drawn live, so not in the plate
    keep = np.maximum(a, 0)
    keep[bar_plate.Y0:bar_plate.Y1, bar_plate.X0 - 8:bar_plate.X1 + 8] = 1   # the rebuilt strip belongs to the layer too
    comp = comp * keep[..., None] + clean * (1 - keep[..., None])
    comp = floor_3d.apply(comp, sk)                     # the one rug; the pedal assembly is drawn live (3D renders)
    ys, xs = np.where(keep > 0.01); bx = [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1]
    Image.fromarray(np.clip(comp[bx[1]:bx[3], bx[0]:bx[2]], 0, 255).astype(np.uint8)).save("skins/skin_%s.jpg" % sk, quality=94)
    out[sk] = {"box": bx, "job": job}
    Image.fromarray(np.clip(comp, 0, 255).astype(np.uint8)).resize((1600, 1000), Image.LANCZOS).save("skins/_full_%s.jpg" % sk, quality=88)
    print(sk, job, "box", bx, "changed %.1f%%" % (100 * (a > 0.5).mean()))
json.dump(out, open("skins/skins.json", "w"), indent=1)
