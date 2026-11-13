"""The TRIM readout's paper, re-grained (Joshua 2026-11-12: AI artifact right of the TRIM knob). The painted "0.0 dB" was
erased smooth, leaving a grain-free blob behind the live readout. Paper grain (the fine detail above the blob, same
columns) is added back where the grain is weak, just enough to bring it up to the surrounding paper's.
Also erases the painted pointer stub between the TRIM label and the knob (Joshua: "there's an extra pointer"): the
plugin draws the knob's pointer itself, so the stub read as a second pointer once TRIM was turned.
apply(bg) -> bg (float RGB canvas, 3200x2000)."""
import numpy as np, cv2

BOX = (3020, 78, 3190, 140)     # canvas px: the erased readout area
SHIFT = 52                      # donor grain from this far above (clean paper, same columns)


def _grain(l):
    hp = l - cv2.GaussianBlur(l, (0, 0), 3)
    return hp, np.sqrt(cv2.GaussianBlur(hp * hp, (0, 0), 6))


STUB = (2962, 50, 2990, 73)     # canvas px: the painted pointer stub above the knob, with its shadow tail


def apply(bg):
    out = bg.copy()
    # stub and its soft shadow tail -> the clean paper either side (same rows, 26 px left and right), feathered in
    sx0, sy0, sx1, sy1 = STUB
    D = 26
    fill = 0.5 * (bg[sy0:sy1, sx0 - D:sx1 - D] + bg[sy0:sy1, sx0 + D:sx1 + D])
    h, w = sy1 - sy0, sx1 - sx0; yy, xx = np.mgrid[0:h, 0:w]
    e = np.minimum(np.minimum(yy, h - 1 - yy), np.minimum(xx, w - 1 - xx))
    f = np.clip(e / 4.0, 0, 1)[..., None]
    out[sy0:sy1, sx0:sx1] = fill * f + out[sy0:sy1, sx0:sx1] * (1 - f)
    x0, y0, x1, y1 = BOX
    reg = out[y0:y1, x0:x1]
    _, sd = _grain(reg.mean(2))
    don = bg[y0 - SHIFT:y1 - SHIFT, x0:x1]
    hp_d = don - cv2.GaussianBlur(don, (0, 0), 3)
    target = np.median(_grain(don.mean(2))[1])
    k = np.sqrt(np.clip(1 - (sd / target) ** 2, 0, 1))          # how much grain is missing here
    h, w = k.shape; yy, xx = np.mgrid[0:h, 0:w]
    edge = np.minimum(np.minimum(yy, h - 1 - yy), np.minimum(xx, w - 1 - xx))
    k = cv2.GaussianBlur(k.astype(np.float32), (0, 0), 4) * np.clip(edge / 10.0, 0, 1)
    # the erase also left soft brighter lobes: pull its mid tones toward the paper's broad shading
    big = cv2.copyMakeBorder(bg[y0 - 30:y1 + 30, x0 - 30:x1], 0, 0, 0, 30, cv2.BORDER_REFLECT)   # the canvas ends 10 px right of the box
    broad = cv2.GaussianBlur(big, (0, 0), 24)[30:-30, 30:-30]
    mid = cv2.GaussianBlur(reg, (0, 0), 5)
    out[y0:y1, x0:x1] = reg - (mid - broad) * k[..., None] + hp_d * k[..., None]
    return out
