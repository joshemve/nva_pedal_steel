"""The floor under the steel: one rug plate + the Blender pedal assembly (blender/pedals_3d.py).
  plate():            the ChatGPT pedal-free rug (aligned/floorclean_1), extended upward with its own rows so it also
                      fills under the higher pearloid body; applied below each instrument's body edge.
  apply(img, skin):   paste the rug into a canvas-space image (3200x2000) below that skin's body edge.
  package(src, dst):  render frames -> vertical sprite strips cropped to each layer's alpha bbox + floor3d.json.
  python floor_3d.py check <render_dir> <out.png>    reference | composite at rest | A+LKL engaged
  python floor_3d.py package <render_dir_chrome> <render_dir_brass>"""
import json, os, sys
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__))
CW, CH = 3200, 2000
RECT = (990, 700, 2010, 1338)                  # canvas: rug area (between the front legs, down to the deck)
BODY = {"walnut": 811, "maple": 827, "redsparkle": 809, "pearloid": 753, "koa": 809}   # body bottom edge, canvas y
blur = lambda a, s: cv2.GaussianBlur(a, (0, 0), s)
_plate = None


def plate():
    global _plate
    if _plate is not None: return _plate
    m = json.load(open(os.path.join(H, "aligned/floorclean_1.json"))); x0, y0, x1, y1 = m["canvas_box"]
    g = np.asarray(Image.open(os.path.join(H, "aligned/floorclean_1.png")), np.float32)
    full = np.zeros((CH, CW, 3), np.float32)
    xa, ya, xb, yb = max(x0, 0), max(y0, 0), min(x1, CW), min(y1, CH)
    full[ya:yb, xa:xb] = g[ya - y0:yb - y0, xa - x0:xb - x0]
    # above the walnut body edge the plate shows the body: refill those rows with rug from 70 rows lower
    top = 816
    for y in range(top - 1, RECT[1] - 1, -1): full[y] = full[y + 70]
    _plate = full; return full


def apply(img, skin="walnut"):
    p = plate(); out = img.astype(np.float32).copy()
    m = np.zeros((CH, CW), np.float32); x0, y0, x1, y1 = RECT
    m[max(BODY[skin], y0):y1, x0:x1] = 1
    fx = np.clip(np.minimum(np.arange(CW) - x0, x1 - 1 - np.arange(CW)) / 18.0, 0, 1)   # soft at the sides
    m *= fx[None, :]
    yb = BODY[skin]; m[yb:yb + 2] *= 0.5                                              # body edge: 1px AA
    m[y1 - 3:y1] *= np.linspace(1, 0, 3)[:, None]
    return out * (1 - m[..., None]) + p * m[..., None]


def load(d, name, f):
    rd = lambda n: np.asarray(Image.open(os.path.join(d, "%s_%02d.png" % (n, f))).convert("RGBA"), np.float32)
    if name == "bar" and os.path.exists(os.path.join(d, "bar_solo_00.png")):
        # bar_lit: the pedals'/rods' shadows fall on the bar (but also on the rug); bar_solo: only the bar's own rug
        # shadow. The bar's own (opaque) pixels from bar_lit, everything else from bar_solo.
        solo, lit = rd("bar_solo"), rd("bar_lit")
        body = np.clip((solo[..., 3:] - 235) / 18.0, 0, 1)
        out = solo.copy(); out[..., :3] = lit[..., :3] * body + solo[..., :3] * (1 - body)
        return out
    return rd(name)


def over(dst, src, x0, y0):
    a = src[..., 3:] / 255.0; h, w = src.shape[:2]
    dst[y0:y0 + h, x0:x0 + w] = src[..., :3] * a + dst[y0:y0 + h, x0:x0 + w] * (1 - a)


def compose(d, state, skin="walnut", base=None):
    meta = json.load(open(os.path.join(d, "render.json"))); rx, ry = meta["region"][:2]
    img = apply(np.asarray(Image.open(os.path.join(H, "bg_clean.png")).convert("RGB"), np.float32) if base is None else base, skin)
    top = BODY[skin]
    for name in ["bar", "knee_L", "knee_R", "pedal_A", "pedal_B", "pedal_C"]:
        f = state.get(name, 0); fr = load(d, name, f).copy()
        fr[: max(0, top - ry)] = 0                                                       # nothing above the body edge
        over(img, fr, rx, ry)
    return img


LAYERS = ["bar", "knee_L", "knee_R", "pedal_A", "pedal_B", "pedal_C"]


def package(dirs):
    """dirs: {"chrome": render_dir, "brass": render_dir}. Same box for both metals; frames stacked vertically."""
    out = {"layers": {}, "body_y": BODY, "rect": RECT}
    os.makedirs(os.path.join(H, "sprites"), exist_ok=True)
    for name in LAYERS:
        frames = {}
        for metal, d in dirs.items():
            meta = json.load(open(os.path.join(d, "render.json"))); rx, ry = meta["region"][:2]
            frames[metal] = [load(d, name, f) for f in range(meta["layers"][name])]
            for fr in frames[metal]:   # invisible shadow-catcher noise (alpha < 4/255) would only bloat the sprites
                fr[fr[..., 3] < 4] = 0
        al = np.max([fr[..., 3] for fs in frames.values() for fr in fs], 0)
        ys, xs = np.where(al >= 4)
        x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        n = len(frames["chrome"])
        for metal, fs in frames.items():
            strip = np.concatenate([fr[y0:y1, x0:x1] for fr in fs], 0)
            Image.fromarray(np.clip(strip, 0, 255).astype(np.uint8), "RGBA").save(
                os.path.join(H, "sprites", "f3d_%s_%s.png" % (metal, name)), optimize=True)
        out["layers"][name] = {"box": [int(x0 + rx), int(y0 + ry), int(x1 + rx), int(y1 + ry)], "frames": n}
        print(name, out["layers"][name])
    json.dump(out, open(os.path.join(H, "sprites", "floor3d.json"), "w"), indent=1)


if __name__ == "__main__":
    if sys.argv[1] == "package":
        package({"chrome": sys.argv[2], "brass": sys.argv[3]})
    elif sys.argv[1] == "check":
        d = sys.argv[2]; n = json.load(open(os.path.join(d, "render.json")))["frames"] - 1
        ref = np.asarray(Image.open(os.path.join(H, "bg_hd.png")).convert("RGB"), np.float32)
        a = compose(d, {}); b = compose(d, {"pedal_A": n, "knee_L": n})
        box = (980, 760, 2020, 1350)
        cr = lambda im: Image.fromarray(np.clip(im[box[1]:box[3], box[0]:box[2]], 0, 255).astype(np.uint8))
        w = box[2] - box[0]; hh = box[3] - box[1]
        o = Image.new("RGB", (w, hh * 3)); o.paste(cr(ref), (0, 0)); o.paste(cr(a), (0, hh)); o.paste(cr(b), (0, 2 * hh))
        o.save(sys.argv[3])
