"""The deck's foot switches and knee-lever toggles: clean plate + Blender frames (blender/deck_3d.py).
  apply(img):   paste the ChatGPT-cleaned deck (gen/deckclean_1, controls removed) into a canvas image (3200x2000), only
                inside the controls' areas, feathered; letters, lamps and everything else stay the background's own.
  package(d):   render frames -> vertical sprite strips cropped to each layer's alpha bbox, ../parts/sprites/d3d_*.png +
                d3d.json; the lamps above A/B/C come from the old 'on' sprites (lamp area only).
  python deck_pack.py check <render_dir> <out.png>     background | cleaned + 3D at rest | all engaged
  python deck_pack.py package <render_dir>"""
import json, os, sys
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__))
P = os.path.join(H, "..", "parts")
CW, CH = 3200, 2000
AREAS = [(1965, 1488, 2490, 1745), (2565, 1435, 3060, 1712)]     # canvas: the pedals, the toggles (what gets cleaned)
LAYERS = ["pedal_A", "pedal_B", "pedal_C", "lever_L", "lever_R"]
_plate = None


def plate():
    """cleaned deck crop, ECC-aligned onto the background crop outside the control areas, tone matched"""
    global _plate
    if _plate is not None: return _plate
    box = json.load(open(os.path.join(H, "deck_crop.json")))["box"]; w, h = box[2] - box[0], box[3] - box[1]
    ref = np.asarray(Image.open(os.path.join(H, "deck_crop.png")).convert("RGB"), np.float32)
    gen = np.asarray(Image.open(os.path.join(H, "gen/deckclean_1_1.png")).convert("RGB").resize((1536, 1024), Image.LANCZOS), np.float32)
    mask = np.ones(ref.shape[:2], np.uint8)
    for x0, y0, x1, y1 in AREAS:
        sx, sy = 1536 / w, 1024 / h
        mask[int((y0 - box[1]) * sy) - 10:int((y1 - box[1]) * sy) + 10, int((x0 - box[0]) * sx) - 10:int((x1 - box[0]) * sx) + 10] = 0
    g = lambda a: cv2.GaussianBlur(a.mean(2), (0, 0), 1.5).astype(np.float32) / 255
    W = np.eye(2, 3, dtype=np.float32)
    _, W = cv2.findTransformECC(g(ref), g(gen), W, cv2.MOTION_AFFINE,
                                (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 300, 1e-6), mask, 5)
    al = cv2.warpAffine(gen, W, (1536, 1024), flags=cv2.INTER_LINEAR | cv2.WARP_INVERSE_MAP, borderMode=cv2.BORDER_REFLECT)
    keep = mask > 0
    for c in range(3):
        A = np.stack([al[..., c][keep], np.ones(keep.sum())], 1)
        k, b = np.linalg.lstsq(A, ref[..., c][keep], rcond=None)[0]; al[..., c] = al[..., c] * k + b
    full = np.zeros((CH, CW, 3), np.float32)
    full[box[1]:box[3], box[0]:box[2]] = np.asarray(Image.fromarray(np.clip(al, 0, 255).astype(np.uint8)).resize((w, h), Image.LANCZOS), np.float32)
    _plate = full; return full


def apply(img):
    p = plate(); m = np.zeros((CH, CW), np.float32)
    for x0, y0, x1, y1 in AREAS: m[y0:y1, x0:x1] = 1
    m = cv2.GaussianBlur(m, (0, 0), 6)
    return img.astype(np.float32) * (1 - m[..., None]) + p * m[..., None]


def over(dst, src, x0, y0):
    a = src[..., 3:] / 255.0; h, w = src.shape[:2]
    dst[y0:y0 + h, x0:x0 + w] = src[..., :3] * a + dst[y0:y0 + h, x0:x0 + w] * (1 - a)


def compose(d, f, base):
    meta = json.load(open(os.path.join(d, "render.json"))); rx, ry = meta["region"][:2]
    img = apply(base)
    for n in LAYERS:
        over(img, np.asarray(Image.open(os.path.join(d, "%s_%02d.png" % (n, f))).convert("RGBA"), np.float32), rx, ry)
    return img


def package(d):
    meta = json.load(open(os.path.join(d, "render.json"))); rx, ry = meta["region"][:2]
    out = {"layers": {}}
    for n in LAYERS:
        fs = [np.asarray(Image.open(os.path.join(d, "%s_%02d.png" % (n, f))).convert("RGBA"), np.float32) for f in range(meta["layers"][n])]
        for fr in fs: fr[fr[..., 3] < 4] = 0
        if n.startswith("lever"):   # the engaged halo's soft warm glow on the leather (like the A/B/C lamps' glow)
            cx = (2733 if n == "lever_L" else 2870) - rx; cy = 1480 - ry
            yy, xx = np.mgrid[:fs[0].shape[0], :fs[0].shape[1]]; rr = np.hypot(xx - cx, (yy - cy) * 1.15)
            gl = np.exp(-(rr / 26.0) ** 2) * (rr > 14)
            for f, fr in enumerate(fs):
                t = f / (len(fs) - 1); t = t * t * (3 - 2 * t); a = gl * 0.55 * t * 255
                aa = a + fr[..., 3] * (1 - a / 255)
                col = np.array([255, 150, 60], np.float32)
                fr[..., :3] = np.where(aa[..., None] > 0, (col * a[..., None] + fr[..., :3] * fr[..., 3:] * (1 - a[..., None] / 255)) / np.maximum(aa[..., None], 1e-3), fr[..., :3])
                fr[..., 3] = aa
        al = np.max([fr[..., 3] for fr in fs], 0); ys, xs = np.where(al >= 4)
        x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        Image.fromarray(np.clip(np.concatenate([fr[y0:y1, x0:x1] for fr in fs], 0), 0, 255).astype(np.uint8), "RGBA").save(
            os.path.join(P, "sprites", "d3d_%s.png" % n), optimize=True)
        out["layers"][n] = {"box": [int(x0 + rx), int(y0 + ry), int(x1 + rx), int(y1 + ry)], "frames": len(fs)}
        print(n, out["layers"][n])
    # the A/B/C lamps: the lamp area of the old 'on' sprites (their pedal part is replaced by the renders)
    S = json.load(open(os.path.join(P, "sprites", "sprites.json")))
    for k in ("A", "B", "C"):
        b = S["pedal_%s_on" % k]["box"]; sp = np.asarray(Image.open(os.path.join(P, "sprites", "pedal_%s_on.png" % k)).convert("RGBA"), np.float32)
        y_end = 1497 - b[1]                                    # stop above the pedal's top edge
        lam = sp[:y_end].copy()
        # keep only the lamp and its glow (the letter above it is the background's own)
        yy, xx = np.mgrid[:lam.shape[0], :lam.shape[1]]
        cy = 1466 - b[1]; cx = lam.shape[1] / 2
        r = np.hypot(yy - cy, xx - cx); lam[..., 3] *= np.clip((34 - r) / 10, 0, 1)
        Image.fromarray(np.clip(lam, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(P, "sprites", "d3d_lamp_%s.png" % k), optimize=True)
        out["layers"]["lamp_" + k] = {"box": [b[0], b[1], b[2], b[1] + y_end], "frames": 1}
    json.dump(out, open(os.path.join(P, "sprites", "d3d.json"), "w"), indent=1)


if __name__ == "__main__":
    if sys.argv[1] == "package":
        package(sys.argv[2])
    elif sys.argv[1] == "check":
        d = sys.argv[2]; n = json.load(open(os.path.join(d, "render.json")))["frames"] - 1
        base = np.asarray(Image.open(os.path.join(P, "bg_clean.png")).convert("RGB"), np.float32)
        box = (1940, 1380, 3080, 1770)
        cr = lambda im: Image.fromarray(np.clip(im[box[1]:box[3], box[0]:box[2]], 0, 255).astype(np.uint8))
        w, hh = box[2] - box[0], box[3] - box[1]
        o = Image.new("RGB", (w, hh * 3))
        o.paste(cr(base), (0, 0)); o.paste(cr(compose(d, 0, base)), (0, hh)); o.paste(cr(compose(d, n, base)), (0, 2 * hh))
        o.save(sys.argv[3])
