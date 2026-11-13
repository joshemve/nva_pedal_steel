"""The top bar's cream paper replaced by one continuous sheet ChatGPT painted (Joshua 2026-11-13: the background should be
one natural surface, not patched; the TRIM corner's stub, creases and erased "0.0 dB" were all hand repairs). ChatGPT got
the plugin screenshot and made the bar empty; only its paper is used. Everything on the bar that isn't plain paper (logo,
NORTHERN VALLEY AUDIO, rules, display, arrow keys, meter, TRIM and its knob, their shadows) keeps its own pixels, riding on
the new paper by difference, so nothing is AI-redrawn.
apply(bg) -> bg (float RGB canvas, 3200x2000)."""
import os
import numpy as np, cv2
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__))
SURFACE = os.path.join(H, "..", "bgregen", "out", "top_bare_a_1.png")
BOX = (0, 0, 3200, 186)           # canvas px: the bar down to just above its bottom edge (the edge and its shadow stay)
KEEP_CIRCLES = [(2975.0, 105.0, 62.0)]   # canvas (cx, cy, r): the TRIM knob, its cream dome is paper-coloured


_SURF = None   # the matched paper sheet over BOX, kept for swap_sprite (apply runs first)


def swap_sprite(sp, X0, Y0):
    """an overlay sprite's own paper patches (the small window's top bar, cut from an older concept) -> the same sheet,
    so they don't show as differently grained rectangles; its ink keeps its pixels. sp: float RGBA; X0, Y0: its canvas
    top-left."""
    if _SURF is None: return sp
    sp = sp.copy(); h, w = sp.shape[:2]
    surf = np.zeros((h, w, 3), np.float32)
    hh, ww = min(h, _SURF.shape[0] - Y0), min(w, _SURF.shape[1] - X0)
    surf[:hh, :ww] = _SURF[Y0:Y0 + hh, X0:X0 + ww]
    if hh < h: surf[hh:] = surf[hh - 1:hh]
    rgb = sp[..., :3]; a = sp[..., 3]; lum = rgb.mean(2)
    level = np.median(lum[(a > 250) & (lum > 150)])
    plain = ((a > 8) & (lum > level - 12) & (lum < level + 14)).astype(np.uint8)
    paint = (cv2.dilate(1 - cv2.erode(plain, np.ones((3, 3), np.uint8)), np.ones((7, 7), np.uint8)) == 0).astype(np.float32)
    old_p = _nblur(rgb, paint, 4.0); new_p = _nblur(surf, paint, 4.0)
    wgt = cv2.GaussianBlur(paint, (0, 0), 1.2)[..., None]
    sp[..., :3] = surf * wgt + (rgb - old_p + new_p) * (1 - wgt)
    return sp


def _nblur(img, m, sigma):
    num = cv2.GaussianBlur(img * m[..., None], (0, 0), sigma)
    den = cv2.GaussianBlur(m, (0, 0), sigma)[..., None]
    return num / np.maximum(den, 1e-4)


def apply(bg):
    if not os.path.exists(SURFACE): return bg
    out = bg.copy()
    x0, y0, x1, y1 = BOX
    reg = out[y0:y1, x0:x1]
    gen = np.asarray(Image.open(SURFACE).convert("RGB"), np.float32)
    gen = cv2.resize(gen, (3200, 2000), interpolation=cv2.INTER_CUBIC)[y0:y1, x0:x1]
    lum = reg.mean(2)
    # plain paper: close to the local paper level (ink, keys, display, knob and the soft shadows round them are darker)
    level = cv2.GaussianBlur(np.where(lum > 150, lum, 0).astype(np.float32), (0, 0), 25) / np.maximum(
        cv2.GaussianBlur((lum > 150).astype(np.float32), (0, 0), 25), 1e-4)
    plain = ((lum > level - 10) & (lum < level + 14)).astype(np.uint8)
    plain = cv2.erode(plain, np.ones((5, 5), np.uint8))
    feature = cv2.dilate(1 - plain, np.ones((9, 9), np.uint8))
    paint = (feature == 0).astype(np.float32)
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    kept = np.zeros_like(paint)
    for cx, cy, r in KEEP_CIRCLES: kept[np.hypot(xx - cx, yy - cy) < r] = 1.0
    paint[kept > 0] = 0.0
    kept = cv2.GaussianBlur(kept, (0, 0), 3.0)[..., None]   # exactly our own pixels there
    # the new paper matched to ours in mean colour
    gm = gen.reshape(-1, 3)[paint.ravel() > 0.5].mean(0); om = reg.reshape(-1, 3)[paint.ravel() > 0.5].mean(0)
    surf = gen * (om / np.maximum(gm, 1e-3))[None, None, :]
    global _SURF; _SURF = surf
    old_paper = _nblur(reg, paint, 5.0)
    new_paper = _nblur(surf, paint, 5.0)
    w = cv2.GaussianBlur(paint, (0, 0), 1.5)[..., None]
    h = y1 - y0; fade = np.clip((h - 1 - np.arange(h)) / 8.0, 0, 1)[:, None, None]   # blend back to ours at the bottom edge
    mixed = surf * w + (reg - old_paper + new_paper) * (1 - w)
    mixed = mixed * (1 - kept) + reg * kept
    out[y0:y1, x0:x1] = mixed * fade + reg * (1 - fade)
    return out
