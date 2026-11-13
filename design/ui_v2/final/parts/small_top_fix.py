"""Small window's top-bar overlay (sp_small_top, cut from the AI concept small_1): a heavy "IM" sat over the plain
"TRIM" and a stray "B" fragment over the live "0.0 dB" (Joshua 2026-11-12: AI artifact). The B fragment is cleared; TRIM
is redrawn whole in Montserrat Bold at OUTPUT's size and ink, on paper with the plain label inpainted away.
fix(sprite_rgba_float, bg_canvas_float, box_ui) -> sprite."""
import os
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "fonts", "Montserrat-Bold.ttf")
INK = (29, 23, 19)                 # OUTPUT's ink in the same overlay
CAP = 26                           # OUTPUT's cap height (sprite px)
TRIM_BOX = (586, 4, 712, 52)       # sprite px: the label's patch
TRIM_CX = 643                      # the TRIM knob's axis (sprite px)
B_BOX = (812, 70, 867, 124)        # the stray "B" fragment
STUB_BOX = (634, 44, 656, 53)      # the painted pointer stub between TRIM and the knob


def fix(sp, bg, box_ui):
    sp = sp.copy()
    x0, y0, x1, y1 = B_BOX; sp[y0:y1, x0:x1, 3] = 0
    X0, Y0 = int(round(box_ui[0] * 2)), int(round(box_ui[1] * 2))
    x0, y0, x1, y1 = TRIM_BOX
    paper = np.clip(bg[Y0 + y0:Y0 + y1, X0 + x0:X0 + x1], 0, 255).astype(np.uint8)
    dark = (paper.mean(2) < 150).astype(np.uint8)
    dark = cv2.dilate(dark, np.ones((5, 5), np.uint8))
    paper = cv2.inpaint(paper, dark, 5, cv2.INPAINT_TELEA)
    img = Image.fromarray(paper); d = ImageDraw.Draw(img)
    size = 20
    while True:                     # the font size whose cap H is CAP
        f = ImageFont.truetype(FONT, size); b = f.getbbox("H")
        if b[3] - b[1] >= CAP: break
        size += 1
    tr = 1.6; w = sum(f.getlength(c) for c in "TRIM") + tr * 3
    tx = TRIM_CX - x0 - w / 2; ty = 16 - y0 - f.getbbox("H")[1]
    for c in "TRIM":
        d.text((tx, ty), c, font=f, fill=INK); tx += f.getlength(c) + tr
    patch = np.asarray(img, np.float32)
    h, w_ = patch.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w_]
    edge = np.minimum(np.minimum(yy, h - 1 - yy), np.minimum(xx, w_ - 1 - xx))
    a = np.clip(edge / 6.0, 0, 1) * 255
    reg = sp[y0:y1, x0:x1]
    ar = a[..., None] / 255
    reg[..., :3] = patch * ar + reg[..., :3] * (1 - ar)
    reg[..., 3] = np.maximum(reg[..., 3], a)
    # the painted pointer stub under TRIM (the plugin draws the pointer; Joshua: "there's an extra pointer")
    x0, y0, x1, y1 = STUB_BOX
    st = np.clip(sp[y0:y1, x0:x1, :3], 0, 255).astype(np.uint8)
    m = cv2.dilate((st.mean(2) < 175).astype(np.uint8), np.ones((3, 3), np.uint8))
    sp[y0:y1, x0:x1, :3] = cv2.inpaint(st, m, 4, cv2.INPAINT_TELEA).astype(np.float32)
    return sp
