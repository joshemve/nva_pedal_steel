"""The TRIM readout's paper, re-grained (Joshua 2026-11-12: AI artifact right of the TRIM knob). The painted "0.0 dB" was
erased smooth, leaving a grain-free blob behind the live readout. Generated grain at the paper's size and strength is
added back where the grain is weak, just enough to bring it up to the surrounding paper's.
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
    target = np.median(_grain(don.mean(2))[1])
    # grain is generated, not copied: the donor paper had faint diagonal creases and copying it doubled them (Joshua:
    # "little artifact lines"). Fine noise, softened to the paper's grain size, scaled to the paper's grain strength.
    rng = np.random.default_rng(7)
    nz = cv2.GaussianBlur(rng.standard_normal(don.shape[:2]).astype(np.float32), (0, 0), 0.8)
    nz = nz - cv2.GaussianBlur(nz, (0, 0), 3)
    nz *= target / max(float(_grain(nz)[1].mean()), 1e-6)
    tint = (don.reshape(-1, 3).mean(0) / don.mean())[None, None, :]       # grain carries the paper's warm cast
    hp_d = nz[..., None] * tint
    k = np.sqrt(np.clip(1 - (sd / target) ** 2, 0, 1))          # how much grain is missing here
    h, w = k.shape; yy, xx = np.mgrid[0:h, 0:w]
    edge = np.minimum(np.minimum(yy, h - 1 - yy), np.minimum(xx, w - 1 - xx))
    k = cv2.GaussianBlur(k.astype(np.float32), (0, 0), 4) * np.clip(edge / 10.0, 0, 1)
    # the erase also left soft brighter lobes: pull its mid tones toward the paper's broad shading
    # shading rebuilt from the paper AROUND the box (a blur over the box kept the erased text's ghost lobes)
    P = 24; X1 = min(x1 + P, bg.shape[1])
    big = cv2.GaussianBlur(bg[y0 - P:y1 + P, x0 - P:X1], (0, 0), 5)
    hole = np.zeros(big.shape[:2], np.uint8); hole[P:P + (y1 - y0), P:P + (x1 - x0)] = 255
    lum = big.mean(2); paper = np.median(lum[hole == 0])
    hole[cv2.dilate((lum < paper - 12).astype(np.uint8), np.ones((15, 15), np.uint8)) > 0] = 255   # not the knob or its shadow
    lo = np.clip(big, 0, 255).astype(np.uint8)
    broad = cv2.inpaint(lo, hole, 12, cv2.INPAINT_TELEA).astype(np.float32)
    broad = cv2.GaussianBlur(broad, (0, 0), 6)[P:P + (y1 - y0), P:P + (x1 - x0)]
    mid = big[P:P + (y1 - y0), P:P + (x1 - x0)]
    wbox = (np.clip(edge / 10.0, 0, 1) * (cv2.GaussianBlur(big.mean(2), (0, 0), 2)[P:P + (y1 - y0), P:P + (x1 - x0)] > paper - 12))[..., None]   # leave the knob's own shadow alone
    out[y0:y1, x0:x1] = reg - (mid - broad) * wbox + hp_d * k[..., None]
    return out
