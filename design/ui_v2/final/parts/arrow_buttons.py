"""The preset arrow buttons in the top bar, redrawn clean (Joshua 2026-11-12: the AI-painted ones had texture artifacts).
Same place, size and colours as the approved design (measured on the baked buttons): a dark rounded key with a soft drop
shadow, a smooth face shading lighter at the top, a faint top highlight, and a crisp cream triangle pressed into it.
apply(bg) -> bg (float RGB canvas, 3200x2000)."""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SS = 4                                          # supersampling
BUTTONS = {"prev": (1086, 83, 1187, 161), "next": (2010, 83, 2111, 161)}   # canvas px (x0, y0, x1, y1)
RADIUS = 13
FACE_TOP, FACE_BOT = (50, 40, 31), (31, 25, 19)
RIM = (24, 18, 13)
TRI = (233, 201, 171)
TRI_W, TRI_H = 27, 35                           # canvas px (point to base, base height)
PAD = 14                                        # room around the key for its shadow


def _key(w, h, left):
    W, H = (w + 2 * PAD) * SS, (h + 2 * PAD) * SS
    x0, y0, x1, y1 = PAD * SS, PAD * SS, (PAD + w) * SS, (PAD + h) * SS
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    # drop shadow: below and a little soft all round
    sh = Image.new("L", (W, H), 0); ImageDraw.Draw(sh).rounded_rectangle((x0, y0 + 3 * SS, x1, y1 + 3 * SS), RADIUS * SS, fill=150)
    sh = sh.filter(ImageFilter.GaussianBlur(4 * SS))
    out = Image.composite(Image.new("RGBA", (W, H), (20, 12, 6, 255)), out, sh)
    # body: rim, then the face with a vertical gradient
    mask = Image.new("L", (W, H), 0); ImageDraw.Draw(mask).rounded_rectangle((x0, y0, x1, y1), RADIUS * SS, fill=255)
    out.paste(Image.new("RGBA", (W, H), (*RIM, 255)), (0, 0), mask)
    inset = int(2.2 * SS)
    fm = Image.new("L", (W, H), 0); ImageDraw.Draw(fm).rounded_rectangle((x0 + inset, y0 + inset, x1 - inset, y1 - inset), (RADIUS - 2) * SS, fill=255)
    t = np.linspace(0.0, 1.0, H)[:, None, None]
    grad = (np.array(FACE_TOP) * (1 - t) + np.array(FACE_BOT) * t) * np.ones((1, W, 1))
    face = Image.fromarray(np.dstack([grad, np.full((H, W, 1), 255.0)]).astype(np.uint8), "RGBA")
    out.paste(face, (0, 0), fm)
    # a faint highlight along the top inner edge
    hl = Image.new("L", (W, H), 0)
    ImageDraw.Draw(hl).rounded_rectangle((x0 + inset, y0 + inset, x1 - inset, y1 - inset), (RADIUS - 2) * SS, outline=255, width=int(1.4 * SS))
    hl_np = np.asarray(hl, np.float32) * np.clip(1.0 - (np.arange(H)[:, None] - (y0 + inset)) / (0.35 * (y1 - y0)), 0, 1)
    out = Image.composite(Image.new("RGBA", (W, H), (125, 106, 88, 255)), out, Image.fromarray((hl_np * 0.55).astype(np.uint8)))
    # the triangle, centred on its own centroid, with a small dark inset shadow so it reads pressed into the key
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    tw, th = TRI_W * SS, TRI_H * SS
    if left: pts = [(cx - tw * 2 / 3, cy), (cx + tw / 3, cy - th / 2), (cx + tw / 3, cy + th / 2)]
    else:    pts = [(cx + tw * 2 / 3, cy), (cx - tw / 3, cy - th / 2), (cx - tw / 3, cy + th / 2)]
    ts = Image.new("L", (W, H), 0); ImageDraw.Draw(ts).polygon([(x - 1.8 * SS, y - 2.0 * SS) for x, y in pts], fill=215)
    ts = ts.filter(ImageFilter.GaussianBlur(0.8 * SS))
    out = Image.composite(Image.new("RGBA", (W, H), (10, 7, 5, 255)), out, ts)
    tm = Image.new("L", (W, H), 0); ImageDraw.Draw(tm).polygon(pts, fill=255)
    out.paste(Image.new("RGBA", (W, H), (*TRI, 255)), (0, 0), tm)
    return out.resize((w + 2 * PAD, h + 2 * PAD), Image.LANCZOS)


def apply(bg):
    out = bg.copy()
    for name, (x0, y0, x1, y1) in BUTTONS.items():
        w, h = x1 - x0, y1 - y0
        X0, Y0 = x0 - PAD, y0 - PAD
        reg = out[Y0:Y0 + h + 2 * PAD, X0:X0 + w + 2 * PAD]
        # (drawn over the old key at the same place and size: its rim and shadow cover the painted one)
        key = np.asarray(_key(w, h, name == "prev"), np.float32)
        a = key[..., 3:] / 255
        reg[:] = key[..., :3] * a + reg * (1 - a)
    return out
