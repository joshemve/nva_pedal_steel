"""Evens out the AI paint's blotchy lighting on the dark deck (Joshua 2026-11-13: "ai patchy texture" in the MODE box,
"ai texture lighting differences" behind A / B / C and LKL / LKR). Only the soft, mid-size light/dark patches go: each
area's paint is divided by its own blur (the patches) and multiplied by a much broader blur (the panel's real lighting),
so the fine grain stays. Lines, labels, lamps, outlines and screws are left out by brightness and never changed.
apply(bg) -> bg (float RGB canvas, 3200x2000)."""
import numpy as np, cv2

# canvas px (x0, y0, x1, y1): the A/B/C and LKL/LKR panels plus the strip under them; the MODE box's inside
AREAS = [
    dict(box=(1880, 1350, 3170, 1975), patch=12.0, broad=280.0, limit=0.30),
    dict(box=(384, 1828, 932, 1890), patch=5.0, broad=200.0, limit=0.30),
]


def _nblur(img, m, sigma):
    """blur of img over the masked pixels only (normalised convolution)."""
    num = cv2.GaussianBlur(img * m, (0, 0), sigma)
    den = cv2.GaussianBlur(m, (0, 0), sigma)
    return num / np.maximum(den, 1e-4)


def apply(bg):
    out = bg.copy()
    for a in AREAS:
        x0, y0, x1, y1 = a["box"]
        reg = out[y0:y1, x0:x1]
        lum = reg.mean(2)
        paint = np.median(lum)
        # the deck paint only: not the cream lines/labels/lamps/outlines (and a margin round them, their glow)
        feat = (lum > paint + 45).astype(np.uint8)   # cream ink is ~150+ over ~30 paint; the blotches are well under this
        m = (cv2.dilate(feat, np.ones((9, 9), np.uint8)) == 0).astype(np.float32)
        lo = _nblur(lum, m, a["patch"])
        broad = _nblur(lum, m, a["broad"])
        gain = np.clip(broad / np.maximum(lo, 1.0), 1 - a["limit"], 1 + a["limit"])
        # feather: full inside the paint, fading to none at features and at the area's edges
        h, w = lum.shape; yy, xx = np.mgrid[0:h, 0:w]
        edge = np.minimum(np.minimum(yy, h - 1 - yy), np.minimum(xx, w - 1 - xx))
        wgt = cv2.GaussianBlur(m, (0, 0), 2.0) * np.clip(edge / 12.0, 0, 1)
        g = 1 + (gain - 1) * wgt
        out[y0:y1, x0:x1] = reg * g[..., None]
    return out
