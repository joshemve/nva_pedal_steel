"""Fit the preset display's dot-matrix text to the reference: dot pitch, origin, dot size and colour, glow.
lit = HD reference - clean (empty display) in the display box. Writes display.json and _display_fit.png."""
import json, os
import numpy as np, cv2
from PIL import Image
from dotfont import dots
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
TEXT = "01 SWEET GEORGIA E9"
hd = np.asarray(Image.open("bg_hd.png"), np.float32); clean = np.asarray(Image.open("bg_clean.png"), np.float32)
bx = json.load(open("aligned/display_empty.json"))["part_box_canvas"]; x0, y0, x1, y1 = bx
lit = np.clip(hd[y0:y1, x0:x1] - clean[y0:y1, x0:x1], 0, 255); L = lit.mean(2)
h, w = L.shape
D = dots(TEXT); cols = max(c for c, r in D) + 1


def render(p, ox, oy, s, adv_dots=None, py=None):
    py = py or PY
    m = np.zeros((h, w), np.float32)
    for c, r in D:
        x, y = ox + c * p, oy + r * py
        xi, yi = int(round(x)), int(round(y))
        if 0 <= xi < w and 0 <= yi < h: m[yi, xi] += 1
    return cv2.GaussianBlur(m, (0, 0), s)


PY = 5.4
best = None
for p, PY in [(a, b) for a in np.arange(5.25, 5.55, 0.025) for b in np.arange(5.2, 7.0, 0.1)]:
    width = (cols - 1) * p
    for oy in np.arange(h / 2 - 3 * PY - 4, h / 2 - 3 * PY + 4, 0.5):
        m0 = render(p, 0, oy, p * 0.35)
        # x origin by correlation along x
        for ox in np.arange(0, w - width, 1.0):
            pass
        cx = (np.argwhere(L > L.max() * 0.4)[:, 1]).mean(); ox0 = cx - width / 2
        for ox in np.arange(ox0 - 4, ox0 + 4, 0.5):
            m = render(p, ox, oy, p * 0.35); c = (m * L).sum() / (np.sqrt((m * m).sum() * (L * L).sum()) + 1e-6)
            if best is None or c > best[0]: best = (c, p, ox, oy, PY)
c, p, ox, oy, PY = best
print("pitch x %.3f y %.2f  origin %.1f %.1f  corr %.3f" % (p, PY, ox, oy, c))
# dot core size + glow: lit ~ core(s1)*col1 + glow(s2)*col2, fit by least squares per channel over a few sizes
bestf = None
for s1 in np.arange(0.8, 2.6, 0.2):
    for s2 in (3, 4, 5, 6, 8):
        a = render(p, ox, oy, s1); g = render(p, ox, oy, s2)
        A = np.stack([a.ravel(), g.ravel()], 1); err = 0; co = []
        for ch in range(3):
            k, res, *_ = np.linalg.lstsq(A, lit[..., ch].ravel(), rcond=None); co.append(k.tolist())
            err += ((A @ k - lit[..., ch].ravel()) ** 2).mean()
        if bestf is None or err < bestf[0]: bestf = (err, s1, s2, co)
err, s1, s2, co = bestf
print("core sigma %.1f glow sigma %d  rmse %.1f  coeffs %s" % (s1, s2, np.sqrt(err / 3), np.round(co, 1)))
model = np.stack([render(p, ox, oy, s1) * co[ch][0] + render(p, ox, oy, s2) * co[ch][1] for ch in range(3)], 2)
out = np.clip(clean[y0:y1, x0:x1] + model, 0, 255)
chk = np.concatenate([hd[y0:y1, x0:x1], out, np.abs(out - hd[y0:y1, x0:x1]) * 3], 0)
Image.fromarray(np.clip(chk, 0, 255).astype(np.uint8)).save("_display_fit.png")
json.dump({"box_canvas": bx, "pitch": p, "pitch_y": PY, "origin": [ox, oy], "adv": 6, "core_sigma": s1, "glow_sigma": s2,
           "core_rgb": [co[ch][0] for ch in range(3)], "glow_rgb": [co[ch][1] for ch in range(3)],
           "ref_text": TEXT, "text_centre_x": ox + (cols - 1) * p / 2}, open("display.json", "w"), indent=1)

# --- measured dot kernel: lit = impulses (*) K, solved per channel in the Fourier domain (bilinear sub-pixel splat)
def impulses(p, ox, oy):
    m = np.zeros((h, w), np.float32)
    for c, r in D:
        x, y = ox + c * p, oy + r * PY; xi, yi = int(np.floor(x)), int(np.floor(y)); fx, fy = x - xi, y - yi
        for dy, wy in ((0, 1 - fy), (1, fy)):
            for dx, wx in ((0, 1 - fx), (1, fx)):
                if 0 <= yi + dy < h and 0 <= xi + dx < w: m[yi + dy, xi + dx] += wx * wy
    return m
I = np.fft.rfft2(impulses(p, ox, oy)); KS = 31
K = np.zeros((KS, KS, 3), np.float32)
for ch in range(3):
    Lf = np.fft.rfft2(lit[..., ch]); k = np.fft.irfft2(np.conj(I) * Lf / (np.abs(I) ** 2 + 0.05), s=(h, w))
    k = np.roll(np.roll(k, KS // 2, 0), KS // 2, 1)[:KS, :KS]; K[..., ch] = k
yy, xx = np.mgrid[0:KS, 0:KS] - KS // 2; K *= np.clip(1.2 - np.hypot(xx, yy) / (KS / 2), 0, 1)[..., None]   # taper
K = np.clip(K, 0, None)
model = np.stack([cv2.filter2D(impulses(p, ox, oy), -1, K[..., ch], borderType=cv2.BORDER_CONSTANT) for ch in range(3)], 2)
out = np.clip(clean[y0:y1, x0:x1] + model, 0, 255)
print("kernel model: mean|err| %.1f (display box, 0-255)" % np.abs(out - hd[y0:y1, x0:x1]).mean())
chk = np.concatenate([hd[y0:y1, x0:x1], out, np.abs(out - hd[y0:y1, x0:x1]) * 3], 0)
Image.fromarray(np.clip(chk, 0, 255).astype(np.uint8)).save("_display_fit.png")
np.save("display_kernel.npy", K)
Image.fromarray(np.clip(K / K.max() * 255, 0, 255).astype(np.uint8)).save("display_dot.png")
d = json.load(open("display.json")); d["kernel"] = "display_dot.png (RGB, scale %.4f per channel max)" % K.max(); d["kernel_max"] = float(K.max())
json.dump(d, open("display.json", "w"), indent=1)

# --- per-character sub-pixel offsets (the reference text isn't on a perfectly even grid), only to measure a sharp kernel
from dotfont import G
chars = list(TEXT.upper()); offs = []
def char_imp(i, dx, dy):
    m = np.zeros((h, w), np.float32)
    for r, row in enumerate(G.get(chars[i], G[" "])):
        for c, v in enumerate(row):
            if v != "#": continue
            x, y = ox + (i * 6 + c) * p + dx, oy + r * PY + dy; xi, yi = int(np.floor(x)), int(np.floor(y)); fx, fy = x - xi, y - yi
            for ddy, wy in ((0, 1 - fy), (1, fy)):
                for ddx, wx in ((0, 1 - fx), (1, fx)):
                    if 0 <= yi + ddy < h and 0 <= xi + ddx < w: m[yi + ddy, xi + ddx] += wx * wy
    return m
Lb = cv2.GaussianBlur(L, (0, 0), 1.5)
imp = np.zeros((h, w), np.float32)
for i, ch in enumerate(chars):
    if ch == " ": offs.append((0, 0)); continue
    bestc = None
    for dx in np.arange(-10, 10.01, 0.5):
        for dy in np.arange(-3, 3.01, 0.5):
            m = cv2.GaussianBlur(char_imp(i, dx, dy), (0, 0), 1.5); c_ = (m * Lb).sum()
            if bestc is None or c_ > bestc[0]: bestc = (c_, dx, dy)
    offs.append(bestc[1:]); imp += char_imp(i, *bestc[1:])
print("per-char offsets:", [tuple(round(v, 1) for v in o) for o in offs])
I = np.fft.rfft2(imp)
for ch in range(3):
    Lf = np.fft.rfft2(lit[..., ch]); k = np.fft.irfft2(np.conj(I) * Lf / (np.abs(I) ** 2 + 0.05), s=(h, w))
    K[..., ch] = np.roll(np.roll(k, KS // 2, 0), KS // 2, 1)[:KS, :KS]
K *= np.clip(1.2 - np.hypot(xx, yy) / (KS / 2), 0, 1)[..., None]; K = np.clip(K, 0, None)
model = np.stack([cv2.filter2D(imp, -1, K[..., ch], borderType=cv2.BORDER_CONSTANT) for ch in range(3)], 2)
on = cv2.dilate((imp > 0.05).astype(np.uint8), np.ones((7, 7), np.uint8)) > 0   # match brightness on the lit dots
gain = np.array([np.median(lit[..., ch][on]) / np.median(model[..., ch][on]) if np.median(model[..., ch][on]) > 2 else 1.0 for ch in range(3)]); print("gain", gain.round(2))
K *= gain[None, None, :]; model *= gain[None, None, :]
out = np.clip(clean[y0:y1, x0:x1] + model, 0, 255)
print("kernel model (per-char placed): mean|err| %.1f" % np.abs(out - hd[y0:y1, x0:x1]).mean())
even = np.stack([cv2.filter2D(impulses(p, ox, oy), -1, K[..., ch], borderType=cv2.BORDER_CONSTANT) for ch in range(3)], 2)
out2 = np.clip(clean[y0:y1, x0:x1] + even, 0, 255)
chk = np.concatenate([hd[y0:y1, x0:x1], out, out2, np.abs(out - hd[y0:y1, x0:x1]) * 3], 0)
Image.fromarray(np.clip(chk, 0, 255).astype(np.uint8)).save("_display_fit.png")
np.save("display_kernel.npy", K)
Image.fromarray(np.clip(K / K.max() * 255, 0, 255).astype(np.uint8)).save("display_dot.png")
d = json.load(open("display.json")); d["kernel_max"] = float(K.max()); d["char_offsets_ref"] = offs
json.dump(d, open("display.json", "w"), indent=1)
