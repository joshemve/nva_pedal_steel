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
KEEP = [(1538, 1832, 2182, 1882)]  # canvas px: dark parts that aren't paint: the volume slider's recessed groove


def _nblur(img, m, sigma):
    num = cv2.GaussianBlur(img * m[..., None] if img.ndim == 3 else img * m, (0, 0), sigma)
    den = cv2.GaussianBlur(m, (0, 0), sigma)
    return num / np.maximum(den[..., None] if img.ndim == 3 else den, 1e-4)


def apply(bg):
    return _regen(bg, SURFACE, BOX, 0)


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
