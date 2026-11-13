"""One consistent cast shadow under every deck control, baked into the deck paint (Joshua 2026-11-13: "these shadows either
are cut off or don't feel like they are proper"). The knob, switch and lever sprites each carry their own small baked
shadow, lit from different directions and fading before their frame edge. With the deck now one even surface that showed.
Every control's silhouette (bgregen/shadow_mask_*.png, measured on the rendered plugin, UI px) casts the same two
shadows from one light above and slightly left: a broad soft one falling down-right, and a tight contact one at its foot.
They're painted into the deck, never into the frame, so they can't be cut off by any frame edge.
apply(bg) -> bg; apply_more(more_rgb, y_canvas) -> more_rgb."""
import os
import numpy as np, cv2
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__))
MASK_MAIN = os.path.join(H, "..", "bgregen", "shadow_mask_main.png")
MASK_MORE = os.path.join(H, "..", "bgregen", "shadow_mask_more.png")
BROAD = dict(sigma=15.0, dx=5.0, dy=16.0, k=0.50)     # canvas px
CONTACT = dict(sigma=4.0, dx=1.5, dy=5.0, k=0.40)
DECK_X = (40, 3160)                                   # canvas px: inside the walnut frame


def _shade(mask_ui, y0, y1):
    m = cv2.resize((np.asarray(Image.open(mask_ui)) > 0).astype(np.float32), (3200, 2000), interpolation=cv2.INTER_LINEAR)
    f = np.ones_like(m)
    for s in (BROAD, CONTACT):
        M = np.float32([[1, 0, s["dx"]], [0, 1, s["dy"]]])
        sh = cv2.GaussianBlur(cv2.warpAffine(m, M, (3200, 2000)), (0, 0), s["sigma"])
        f *= 1 - s["k"] * np.clip(sh, 0, 1)
    f[:, :DECK_X[0]] = 1; f[:, DECK_X[1]:] = 1
    return f[y0:y1]


def _apply(img, mask_ui, y0, y1, yoff):
    if not os.path.exists(mask_ui): return img
    out = img.copy()
    rb = out[y0 - yoff:y1 - yoff, :, 0] - out[y0 - yoff:y1 - yoff, :, 2]
    wood = cv2.GaussianBlur((rb > 28).astype(np.float32), (0, 0), 2.0)   # never darken the walnut frame
    f = _shade(mask_ui, y0, y1)
    f = 1 - (1 - f) * (1 - wood)
    out[y0 - yoff:y1 - yoff] *= f[..., None]
    return out


def apply(bg):
    return _apply(bg, MASK_MAIN, 1340, 1985, 0)


def apply_more(more_rgb, y_canvas):
    return _apply(more_rgb, MASK_MORE, 910, 1985, y_canvas)
