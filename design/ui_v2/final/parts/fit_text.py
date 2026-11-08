"""Pick the font, size, position and colour of every live text item by matching the reference.
text layer = HD reference - clean plate inside each box. Writes text.json and _text_fit.png."""
import json, os, sys
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
FD = "../../fonts"; FONTS = sys.argv[1:] or ["Montserrat-Medium", "Montserrat-SemiBold", "Montserrat-Regular", "Poppins-Medium",
                                             "Poppins-Regular", "segoeuisb", "segoeui", "bahnschrift"]
def font_path(n): return os.path.join(FD, n + ".ttf") if os.path.exists(os.path.join(FD, n + ".ttf")) else "C:/Windows/Fonts/%s.ttf" % n
hd = np.asarray(Image.open("bg_hd.png"), np.float32); clean = np.asarray(Image.open("bg_clean.png"), np.float32)
A = lambda n: json.load(open("aligned/%s.json" % n))["part_box_canvas"]
pad = lambda b, m: [b[0] - m, b[1] - m, b[2] + m, b[3] + m]
ITEMS = {f"val_{k}": (pad(A("pillempty_" + k), 40), t) for k, t in
         [("TONE", "65 %"), ("PICK", "40 %"), ("GLIDE", "145 ms"), ("SWELL", "35 %"), ("ECHO", "18 %"), ("REVERB", "28 %")]}
_mb = pad(A("modebox_empty"), 6); _mb[2] -= round((_mb[2] - _mb[0]) * 0.2)   # leave out the dropdown arrow
ITEMS["mode"] = (_mb, "Smart Auto-Pedal E9")
ITEMS["vol"] = (pad(A("pctbox_empty"), 6), "72 %")
ITEMS["db"] = (pad(A("trimdb_empty"), 8), "0.0 dB")
SS = 4


def render(fp, size, text, w, h, x, y):
    """Coverage mask (h,w) of text with its ink box's left/centre-y at (x, y) canvas px, 4x supersampled."""
    f = ImageFont.truetype(fp, round(size * SS))
    im = Image.new("L", (w * SS, h * SS)); d = ImageDraw.Draw(im)
    l, t, r, b = d.textbbox((0, 0), text, font=f)
    d.text((x * SS - l, y * SS - (t + b) / 2), text, fill=255, font=f)
    return np.asarray(im.resize((w, h), Image.BOX), np.float32) / 255, (r - l) / SS, (b - t) / SS


def fit_item(fp, box, text):
    x0, y0, x1, y1 = box; T = np.abs(hd[y0:y1, x0:x1] - clean[y0:y1, x0:x1]).mean(2); h, w = T.shape
    # keep glyph strokes only: box outlines/edge residue are thin and faint, opened away
    core = cv2.morphologyEx((T > T.max() * 0.45).astype(np.uint8), cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    T = T * (cv2.dilate(core, np.ones((7, 7), np.uint8)) > 0)
    ys, xs = np.where(T > T.max() * 0.35)
    if len(xs) == 0: return None
    tw, th, cx, cy = xs.max() - xs.min(), ys.max() - ys.min(), (xs.max() + xs.min()) / 2, (ys.max() + ys.min()) / 2
    best = None
    _, rw, rh = render(fp, 40, text, 8, 8, 0, 0)
    s0 = 40 * tw / max(rw, 1)
    for s in np.arange(s0 * 0.92, s0 * 1.08, s0 * 0.02):
        for dx in np.arange(-2, 2.01, 1.0):
            for dy in np.arange(-2, 2.01, 1.0):
                mm, rw, rh = render(fp, s, text, w, h, cx - tw / 2 + dx, cy + dy)
                c = (mm * T).sum() / (np.sqrt((mm * mm).sum() * (T * T).sum()) + 1e-6)
                if best is None or c > best[0]: best = (c, s, cx - tw / 2 + dx, cy + dy)
    return best, T


def model(fp, box, text, best):
    x0, y0, x1, y1 = box; c, s, x, y = best; w, h = x1 - x0, y1 - y0
    m, rw, rh = render(fp, s, text, w, h, x, y)
    T = hd[y0:y1, x0:x1] - clean[y0:y1, x0:x1]; sel = m > 0.6
    dark = hd[y0:y1, x0:x1][sel].mean() < clean[y0:y1, x0:x1][sel].mean()
    col = np.percentile(hd[y0:y1, x0:x1][sel], 15 if dark else 85, axis=0)   # ink colour on the stroke cores
    out = clean[y0:y1, x0:x1] * (1 - m[..., None]) + col * m[..., None]
    return out, col, rw


if __name__ == "__main__":
    res = {}
    for fn in FONTS:
        fp = font_path(fn); tot = []
        for k, (box, text) in ITEMS.items():
            r = fit_item(fp, box, text)
            if r: tot.append(r[0][0]); res.setdefault(k, {})[fn] = r[0]
        print("%-22s mean corr %.3f  %s" % (fn, np.mean(tot), " ".join("%.2f" % t for t in tot)))
    json.dump({k: {f: [float(x) for x in v] for f, v in d.items()} for k, d in res.items()}, open("text_fit_all.json", "w"), indent=1)
    if len(FONTS) == 1:   # final: write text.json + check image
        fn = FONTS[0]; fp = font_path(fn); out = {}; tiles = []
        OVR = {"mode": "Inter-w500-o14", "db": "Poppins-SemiBold"}   # faces that differ in the reference
        for k, (box, text) in ITEMS.items():
            if k in OVR:
                fn_k = OVR[k]; b = list(fit_item(font_path(fn_k), box, text)[0]); o, col, rw = model(font_path(fn_k), box, text, b)
            else:
                fn_k = fn; b = res[k][fn]; o, col, rw = model(fp, box, text, b)
            x0, y0, x1, y1 = box
            out[k] = dict(font=fn_k, text=text, size_canvas=float(b[1]), ink_left=float(b[2] + x0), ink_cy=float(b[3] + y0),
                          ink_cx=float(b[2] + x0 + rw / 2), ink_w=float(rw), colour=[float(v) for v in col], corr=float(b[0]))
            r = hd[y0:y1, x0:x1]; tiles.append(np.concatenate([r, o, np.abs(o - r) * 3], 1))
            print("%-10s size %.1f  corr %.3f  err %.1f" % (k, b[1], b[0], np.abs(o - r).mean()))
        W = max(t.shape[1] for t in tiles)
        Image.fromarray(np.clip(np.concatenate([np.pad(t, ((0, 4), (0, W - t.shape[1]), (0, 0))) for t in tiles], 0), 0, 255).astype(np.uint8)).save("_text_fit.png")
        json.dump(out, open("text.json", "w"), indent=1)
