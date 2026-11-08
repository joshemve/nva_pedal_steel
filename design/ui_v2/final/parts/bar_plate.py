"""Rebuild the strip under the reference tone bar (canvas x X0..X1). Strings, wood grain and chrome run horizontally,
and the fret grid repeats every ~57 px here, so: detail (high-pass) from the same rows two fret spaces to the left,
low frequencies interpolated row by row between the strip's edges. Used by build_ui.py."""
import numpy as np, cv2
BAR_X0, BAR_X1 = 1612, 1680   # the reference bar's columns (canvas px)
X0, X1, Y0, Y1, SHIFT = 1604, 1694, 292, 652, 114


def rebuild(img):
    out = img.copy(); w = X1 - X0
    L = img[Y0:Y1, X0 - 6:X0].mean(1); R = img[Y0:Y1, X1:X1 + 6].mean(1)
    L[428 - Y0:482 - Y0] = R[428 - Y0:482 - Y0]   # the fret-9 marker touches the left edge on these rows
    t = (np.arange(w) + 0.5) / w
    low = L[:, None] * (1 - t)[None, :, None] + R[:, None] * t[None, :, None]
    src = img[Y0:Y1, X0 - SHIFT:X1 - SHIFT].copy()
    # the fret-9 marker sits at the source's right end (canvas x>=1564, rows 428-482): take those columns one fret further left
    c0 = 1564 - (X0 - SHIFT); src[428 - Y0:482 - Y0, c0:] = img[428:482, 1564 - 57:X1 - SHIFT - 57]
    src_low = cv2.GaussianBlur(src, (0, 0), 6)
    # low of the source, row-interpolated the same way, so the detail is src - its own row-trend
    s_trend = np.median(src, axis=1, keepdims=True)   # per-row level of the source (robust to a marker at its edge)
    low = low - (np.median(low, axis=1, keepdims=True) - (L + R)[:, None] / 2) * 0
    patch = low + (src - s_trend)
    # per-row level: match one fret span on either side (fret lines included on both, so like compares with like)
    tgt = (img[Y0:Y1, X0 - 57:X0].mean(1) + img[Y0:Y1, X1:X1 + 57].mean(1)) / 2
    patch += (tgt - patch.mean(1))[:, None, :]
    # feather 4 px at the left/right edges
    f = np.clip(np.minimum(np.arange(w) + 1, w - np.arange(w)) / 5.0, 0, 1)[None, :, None]
    fy = np.clip(np.minimum(np.arange(Y1 - Y0) + 1, Y1 - Y0 - np.arange(Y1 - Y0)) / 5.0, 0, 1)[:, None, None]
    a = f * fy
    out[Y0:Y1, X0:X1] = np.clip(patch, 0, 255) * a + img[Y0:Y1, X0:X1] * (1 - a)
    return out
