"""The deck's paint replaced by one continuous surface ChatGPT painted (Joshua 2026-11-13: "the whole background isn't the
same background... feels cut out and covered up in some areas"). final/bgregen/: ChatGPT got the plugin screenshot and
made the deck empty (no controls, text or lines). Only that surface is used: it's scaled to the canvas, matched in tone
and colour to our paint, and put under everything that isn't bare paint. Our cream ink (labels, lines, boxes), the
walnut frame and the screws stay exactly as drawn, so nothing is AI-redrawn text.
apply(bg) -> bg (float RGB canvas, 3200x2000)."""
import os
import numpy as np, cv2
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__))
SURFACE = os.path.join(H, "..", "bgregen", "out", "deck_bare_a_1.png")   # Joshua's pick: A (bare) or B (lighter top)
SURFACE_MORE = os.path.join(H, "..", "bgregen", "out", "more_bare_a_1.png")   # the tall MORE deck, painted the same way
BOX = (24, 1330, 3176, 1985)      # canvas px: the deck inside its walnut frame (the frame itself is kept by colour)
BOX_MORE = (24, 904, 3176, 1985)  # canvas px: the MORE deck inside its frame
KEEP = [(1538, 1838, 2182, 1878)]  # canvas px: dark parts that aren't paint: the volume slider's recessed groove
# the FINAL slider groove (after more_deck's strip step shortens it): the old painted handle left its shadow over and
# round it and a bright fleck at its end, showing at 0 % (Joshua). fix_groove() runs on the finished images.
GROOVE_Y, GROOVE_X, CLEAN_X = (1833, 1881), (1572, 2150), (1600, 1900)   # rims and their edges too (a line was left at them)
SMUDGE = (1930, 1812, 2040, 1886)   # canvas px round the old handle: its shadow over the deck paint


def _nblur(img, m, sigma):
    num = cv2.GaussianBlur(img * m[..., None] if img.ndim == 3 else img * m, (0, 0), sigma)
    den = cv2.GaussianBlur(m, (0, 0), sigma)
    return num / np.maximum(den[..., None] if img.ndim == 3 else den, 1e-4)


_SURF = None


def apply(bg):
    return _regen(bg, SURFACE, BOX, 0)


def swap_sprite(sp, X0, Y0):
    """an overlay sprite's own patches of old deck paint (the small window's bolder labels, cut from an older concept,
    each with a fringe of that concept's blotchy paint) -> the new surface; the cream ink keeps its pixels.
    sp: float RGBA; X0, Y0: its canvas top-left. Run after apply()."""
    if _SURF is None: return sp
    sp = sp.copy(); h, w = sp.shape[:2]
    surf = _SURF[Y0:Y0 + h, X0:X0 + w]
    if surf.shape[:2] != (h, w): surf = cv2.copyMakeBorder(surf, 0, h - surf.shape[0], 0, w - surf.shape[1], cv2.BORDER_REPLICATE)
    rgb = sp[..., :3]; a = sp[..., 3]; lum = rgb.mean(2); rb = rgb[..., 0] - rgb[..., 2]
    paint = ((a > 8) & (lum < 70) & (rb < 25)).astype(np.uint8)
    ink = cv2.dilate(((lum > 90) & (a > 8)).astype(np.uint8), np.ones((5, 5), np.uint8))
    paint = (paint & (1 - ink)).astype(np.float32)
    num = cv2.GaussianBlur(rgb * paint[..., None], (0, 0), 3.0); den = cv2.GaussianBlur(paint, (0, 0), 3.0)[..., None]
    old_p = num / np.maximum(den, 1e-4)
    num = cv2.GaussianBlur(surf * paint[..., None], (0, 0), 3.0); new_p = num / np.maximum(den, 1e-4)
    wgt = cv2.GaussianBlur(paint, (0, 0), 1.0)[..., None]
    sp[..., :3] = surf * wgt + (rgb - old_p + new_p) * (1 - wgt) * (den > 1e-3) + rgb * (1 - wgt) * (den <= 1e-3)
    return sp


def apply_more(more_rgb, y_canvas):
    """the MORE deck image (rows y_canvas..2000 of the canvas) gets its own ChatGPT surface"""
    return _regen(more_rgb, SURFACE_MORE, BOX_MORE, y_canvas)


def _regen(img, surface, box, yoff):
    if not os.path.exists(surface): return img
    out = img.copy()
    x0, y0, x1, y1 = box[0], box[1] - yoff, box[2], box[3] - yoff
    reg = out[y0:y1, x0:x1]
    gen = np.asarray(Image.open(surface).convert("RGB"), np.float32)
    gen = cv2.resize(gen, (3200, 2000), interpolation=cv2.INTER_CUBIC)[y0 + yoff:y1 + yoff, x0:x1]
    lum = reg.mean(2); rb = reg[..., 0] - reg[..., 2]
    paint_lum = np.median(lum[(lum < 60) & (rb < 25)])
    # bare paint = dark, not the orange-brown walnut frame, and away from ink / screws (a margin keeps their glow)
    keep = ((lum > paint_lum + 30) | (rb > 28)).astype(np.uint8)
    paint = (cv2.dilate(keep, np.ones((7, 7), np.uint8)) == 0).astype(np.float32)
    # the new surface matched to our paint: same mean colour, same overall contrast of its fine grain
    gm = gen.reshape(-1, 3)[paint.ravel() > 0.5].mean(0); om = reg.reshape(-1, 3)[paint.ravel() > 0.5].mean(0)
    surf = gen * (om / np.maximum(gm, 1e-3))[None, None, :]
    if yoff == 0:   # the main deck's matched surface, for swap_sprite
        global _SURF; _SURF = np.zeros((2000, 3200, 3), np.float32); _SURF[y0:y1, x0:x1] = surf
    # ink and frame keep their own pixels; their surrounding paint is swapped by difference (ink sits on the new paint)
    old_paint = _nblur(reg, paint, 4.0)
    new_paint = _nblur(surf, paint, 4.0)
    w = cv2.GaussianBlur(paint, (0, 0), 1.5)[..., None]
    kept = (rb > 28).astype(np.float32)
    for kx0, ky0, kx1, ky1 in KEEP: kept[ky0 - yoff - y0:ky1 - yoff - y0, kx0 - x0:kx1 - x0] = 1.0
    wood = cv2.GaussianBlur(kept, (0, 0), 3.0)[..., None]   # the frame and KEEP areas: our own pixels
    ink_on_new = reg - old_paint + new_paint
    mixed = surf * w + ink_on_new * (1 - w)
    out[y0:y1, x0:x1] = mixed * (1 - wood) + reg * wood
    return out


def fix_groove(img, yoff=0):
    """the finished slider groove: even inside along its whole length, and the old handle's shadow round it gone"""
    out = img.copy()
    if _SURF is not None:   # the shadow on the deck paint above and below the groove -> the new surface
        x0, y0, x1, y1 = SMUDGE
        for ya, yb in ((y0, GROOVE_Y[0] + 1), (GROOVE_Y[1] - 1, y1)):   # overlapping the groove rebuild: no gap rows left
            seg = out[ya - yoff:yb - yoff, x0:x1]; new = _SURF[ya:yb, x0:x1]
            h, w = seg.shape[:2]; xx = np.arange(w)
            wx = np.clip(np.minimum(xx, w - 1 - xx) / 12.0, 0, 1)[None, :, None]
            dark = np.clip((new.mean(2) - seg.mean(2)) / 6.0, 0, 1)[..., None]   # only where it's darker than the paint
            out[ya - yoff:yb - yoff, x0:x1] = new * wx * dark + seg * (1 - wx * dark)
    gy0, gy1 = GROOVE_Y[0] - yoff, GROOVE_Y[1] - yoff
    prof = np.median(out[gy0:gy1, CLEAN_X[0]:CLEAN_X[1]], axis=1)
    n = GROOVE_X[1] - GROOVE_X[0]
    fill = np.repeat(prof[:, None, :], n, axis=1) + np.random.default_rng(3).normal(0, 0.8, (gy1 - gy0, n))[..., None]
    ramp = np.clip(np.minimum(np.arange(n), n - 1 - np.arange(n)) / 6.0, 0, 1)[None, :, None]
    seg = out[gy0:gy1, GROOVE_X[0]:GROOVE_X[1]]
    out[gy0:gy1, GROOVE_X[0]:GROOVE_X[1]] = fill * ramp + seg * (1 - ramp)
    # the right cap held the old handle's bright fleck: it becomes the clean left cap, mirrored (rims at 1550 / 2165)
    cy0, cy1 = 1834 - yoff, 1884 - yoff
    left = out[cy0:cy1, 3715 - 2190:3715 - 2130][:, ::-1]           # mirror of columns 2130..2190
    w = left.shape[1]; blend = np.clip((np.arange(w) - 2.0) / 14.0, 0, 1)[None, :, None]
    out[cy0:cy1, 2130:2190] = left * blend + out[cy0:cy1, 2130:2190] * (1 - blend)
    return out
