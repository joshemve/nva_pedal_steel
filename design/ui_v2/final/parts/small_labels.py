"""Small-window variant (round18/small_1, chosen by Joshua): the same screen with bolder, larger labels, value boxes and
values. small_1 is registered to APPROVED_main.png (< 0.4 px), so its label pixels drop straight onto the plate.
Builds sprites/small_deck.png + sprites/small_top.png (label layers; their boxes are added to sprites/sprites.json) and
text_small.json (ink centre / width / colour of each live text item in small_1, measured against the emptied boxes).
Run after build_ui.py and the jobs9 images (make_jobs9.py)."""
import json, os
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
CW, CH = 3200, 2000; RX, RY = CW / 1568, CH / 1003
SM = np.asarray(Image.open("../../round18/small_1.png").convert("RGB").resize((CW, CH), Image.LANCZOS), np.float32)
clean = np.asarray(Image.open("bg_clean.png"), np.float32)
crops = json.load(open("crops.json"))
blur = lambda a, s: cv2.GaussianBlur(a, (0, 0), s)


def empty_patch(part):
    """small_1 with one of the generated empty boxes pasted (text pixels only), in canvas space."""
    x0, y0, x1, y1 = crops[part]["crop"]
    src = Image.open("crops/small_%s.png" % part).convert("L"); src = np.asarray(src, np.float32)
    best = None
    for n in (1, 2):
        p = "gen/small_%s_empty_%d_1.png" % (part, n)
        if not os.path.exists(p): continue
        g = Image.open(p).convert("RGB").resize((1536, 1024), Image.LANCZOS); gg = np.asarray(g.convert("L"), np.float32)
        W = np.eye(2, 3, dtype=np.float32)
        try: _, W = cv2.findTransformECC(src, gg, W, cv2.MOTION_AFFINE, (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 100, 1e-5), None, 5)
        except cv2.error: pass
        ga = cv2.warpAffine(np.asarray(g, np.float32), W, (1536, 1024), flags=cv2.INTER_LINEAR | cv2.WARP_INVERSE_MAP)
        err = np.abs(ga.mean(2) - src).mean()
        if best is None or err < best[0]: best = (err, ga)
    if best is None: return None
    cx0, cy0, cx1, cy1 = round(x0 * RX), round(y0 * RY), round(x1 * RX), round(y1 * RY)
    return (cx0, cy0, cx1, cy1), cv2.resize(best[1], (cx1 - cx0, cy1 - cy0), interpolation=cv2.INTER_AREA)


SMe = SM.copy()
LIMIT = {"pills": (88, 850, 935, 906), "modebox": (183, 910, 440, 952), "pctbox": (1176, 913, 1258, 950), "trim": (1488, 36, 1566, 70)}   # ref px
for part in ("pills", "modebox", "pctbox", "trim"):
    r = empty_patch(part)
    if r is None: print("missing", part); continue
    (x0, y0, x1, y1), g = r
    x0c, y0c, x1c, y1c = max(x0, 0), max(y0, 0), min(x1, CW), min(y1, CH)
    g = g[y0c - y0:y1c - y0, x0c - x0:x1c - x0]; s = SMe[y0c:y1c, x0c:x1c]
    # tone-match the generated crop to small_1 where nothing changed, then take only the removed text
    for c in range(3):
        m = np.abs(g[..., c] - s[..., c]) < 25
        if m.sum() > 100: g[..., c] = (g[..., c] - g[..., c][m].mean()) * (s[..., c][m].std() / max(g[..., c][m].std(), 1e-3)) + s[..., c][m].mean()
    d = (np.abs(s.mean(2) - g.mean(2)) > 28).astype(np.uint8)
    d = cv2.morphologyEx(d, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    lim = np.zeros_like(d); lx0, ly0, lx1, ly1 = LIMIT[part]   # only this part's own boxes (other crops overlap them)
    lim[max(round(ly0 * RY) - y0c, 0):max(round(ly1 * RY) - y0c, 0), max(round(lx0 * RX) - x0c, 0):max(round(lx1 * RX) - x0c, 0)] = 1
    d &= lim
    a = np.clip(blur(cv2.dilate(d, np.ones((7, 7), np.uint8)).astype(np.float32), 2) * 1.5, 0, 1)[..., None]
    SMe[y0c:y1c, x0c:x1c] = g * a + s * (1 - a)

# --- live text in small_1: ink box of (small_1 - emptied) inside each item's box
T = json.load(open("text.json")); out = {}
for k, t in T.items():
    cx, cy, w = t["ink_cx"], t["ink_cy"], t["ink_w"]
    bx0, bx1 = int(cx - w * 0.62 - 14), int(cx + w * 0.62 + 14); by0, by1 = int(cy - 20), int(cy + 20)   # inside the box outline
    if k == "mode": bx0, bx1 = int(t["ink_left"] - 20), int(t["ink_left"] + w * 1.35)
    s = SM[by0:by1, bx0:bx1]; e = SMe[by0:by1, bx0:bx1]
    ink = (np.abs(s - e).mean(2) > 40).astype(np.uint8)
    ink = cv2.morphologyEx(ink, cv2.MORPH_OPEN, np.ones((2, 2), np.uint8))
    ys, xs = np.where(ink > 0)
    if len(xs) < 20: print("no ink found for", k); continue
    col = np.median(s[ink > 0], 0)
    out[k] = dict(text=t["text"], ink_left=float(bx0 + xs.min()), ink_cx=float(bx0 + (xs.min() + xs.max() + 1) / 2),
                  ink_cy=float(by0 + (ys.min() + ys.max() + 1) / 2), ink_w=float(xs.max() + 1 - xs.min()),
                  colour=[float(v) for v in col], font="Poppins-SemiBold" if k != "mode" else "Inter-w500-o14")
    print("%-11s ink w %.0f (was %.0f)  centre %.0f,%.0f" % (k, out[k]["ink_w"], w, out[k]["ink_cx"], out[k]["ink_cy"]))
json.dump(out, open("text_small.json", "w"), indent=1)

# --- label layers: small_1 (emptied) wherever it differs from the plate, inside label regions only (no moving parts)
REGIONS_DECK = [(60, 688, 1525, 724),     # VOICE FEEL SPACE / A B C / LKL LKR headers
                (70, 818, 940, 912),      # knob labels + value boxes
                (85, 908, 480, 954), (560, 908, 750, 954), (1172, 908, 1262, 954), (1325, 905, 1490, 956)]   # MODE, VOLUME PEDAL, % box, MIDI LEARN
REGIONS_TOP = [(1145, 40, 1232, 66), (1455, 8, 1505, 32), (1490, 36, 1565, 70)]   # OUTPUT, TRIM, dB box
S = json.load(open("sprites/sprites.json"))
for name, regs in (("small_deck", REGIONS_DECK), ("small_top", REGIONS_TOP)):
    mask = np.zeros((CH, CW), np.float32)
    for x0, y0, x1, y1 in regs: mask[round(y0 * RY):round(y1 * RY), round(x0 * RX):round(x1 * RX)] = 1
    diff = (np.abs(blur(SMe, 1.0) - blur(clean, 1.0)).mean(2) > 7).astype(np.uint8)
    a = np.clip(blur(cv2.dilate(diff, np.ones((5, 5), np.uint8)).astype(np.float32), 1.5) * 1.4, 0, 1) * blur(mask, 2)
    ys, xs = np.where(a > 0.01); bx = [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1]
    rgba = np.dstack([SMe, a * 255])[bx[1]:bx[3], bx[0]:bx[2]]
    Image.fromarray(rgba.astype(np.uint8), "RGBA").save("sprites/%s.png" % name)
    S[name] = {"box": bx}
    print(name, "box", bx)
json.dump(S, open("sprites/sprites.json", "w"), indent=1)
Image.fromarray(SMe.astype(np.uint8)).resize((1600, 1000), Image.LANCZOS).save("_small_emptied.png")
