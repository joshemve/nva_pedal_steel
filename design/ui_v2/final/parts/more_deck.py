"""MORE state of the control deck (Joshua's pick: round31 r3_2 with 11 labelled CHARACTER spots, round32 k11_5).

Built from the plugin's own background pixels so it matches exactly:
  * row 1 (frame top + VOICE/FEEL/SPACE/pedals/levers) moves up by D canvas px,
  * a second row (MOTION / AMP / TIME / CHARACTER / SYNC) takes row 1's old rows, on a cleaned copy of the panel,
    with tick rings, value boxes, dividers and rules copied from row 1 and labels in the deck's face (Montserrat Bold),
  * the bottom strip (both states) gets a shorter volume slider so a MORE / LESS button fits after MIDI LEARN.

apply_strip(bg)  -> bg with the new bottom strip (normal state, button reads MORE)
build_more(bg)   -> the MORE deck image: canvas rows Y0..2000 (row 1 moved up, row 2, strip with LESS)
layout()         -> dict of UI-px geometry for gen_layout.py
All coordinates here are canvas px (2x of the 1600x1000 UI) unless named *_ui.
"""
import os
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

H = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(H, "..", "..", "fonts", "Montserrat-Bold.ttf")

# ---- measured on ui_bg (canvas px)
FRAME_TOP = 1278            # walnut frame's top edge (shadow above it)
ROW_TOP, RULE_Y = 1350, 1782  # row 1 panel content rows, and the rule between the knob row and the strip
D = RULE_Y + 4 - ROW_TOP    # 436: row 1 moves up by one row height (+ the rule)
Y0 = FRAME_TOP - D          # the MORE deck image starts here (842)
TOPBAND = 110               # rows below FRAME_TOP searched for the walnut rim (above it: rug, now transparent)
RIM_ROW = 1340              # the rim's top in the middle of the deck (measured on ui_bg)
FRAME_OUTLINE = (23, 3178, 1340, 52)   # the walnut frame: left, right, top, corner radius (canvas, ui_bg)
XI0, XI1 = 46, 3156         # panel interior (between the walnut sides)
LABEL_Y = (1654, 1679)      # knob labels' cap box (TONE)
TITLE_Y = (1392, 1420)      # section titles' cap box (VOICE)
RULE_ROWS = (1403, 1411)    # the thin rules either side of a title
LABEL_COL = (246, 223, 186)

# row 1 sources
TONE_C = (261, 1542)        # dome centre (ptrTONEOrigin + frame/2, x2)
GLIDE_C = (884, 1551)
ECHO_C = (1492, 1560)
BOX_PCT = (202, 1694, 348, 1748)   # TONE value box
BOX_MS = (806, 1694, 970, 1748)    # GLIDE value box (wider)
DIV_SRC = (704, 1380, 728, 1752)   # a section divider column
LAMP_SRC = (2030, 1440, 2082, 1494)  # pedal A's unlit lamp

# row 2 (UI px, from the approved concept, scaled 1600/1568)
R2 = {   # name: (kind, cx_ui)
    "VIBRATO": ("scallop", 135), "RATE": ("scallop", 270), "SLIDE": ("scallop", 405),
    "DRIVE": ("dome", 562), "WIDTH": ("dome", 706),
    "DELAY": ("space", 882), "DECAY": ("space", 1030),
}
CHAR_CX_UI, CHAR_SCALE = 1282, 1.30   # the knob as big as the concept's (k11_5: ~110 UI px across; the workshop SPACE knob is 42 px in radius)
CHAR_CY_UI, CHAR_R_UI = 798, 70.0     # knob centre row, dot radius (measured on k11_5)
SYNC_CX_UI = 1508
DIVS_UI = (480, 792, 1120, 1450)
TITLES = (("MOTION", 65, 470), ("AMP", 492, 772), ("TIME", 812, 1100), ("CHARACTER", 1132, 1432))
CHARS = ["STUDIO", "TAPE", "OPTO", "PHASER", "SPRING", "R&B", "WARBLE", "SHIMMER", "ROTARY", "GATE", "ROOM"]
# degrees clockwise from 12 o'clock, measured on k11_5: two columns either side and R&B on top (within the strip's +-135)
CHAR_ANG = [-131, -103, -81, -59, -32, 0, 32, 59, 81, 103, 131]
CHAR_RAD = [70.5, 73.0, 72.0, 69.0, 66.7, 65.5, 66.7, 69.0, 72.0, 73.0, 70.5]   # UI px per spot (closer at the top)

# bottom strip: shorten the groove's middle by SHIFT, slide the rest left, MORE/LESS where MIDI LEARN ended
GROOVE = (1546, 2370); GROOVE_Y = (1830, 1876); TICK_Y = (1878, 1906); TICKS0 = (1586, 2322)
SHIFT = 202
MOVE_X = (2390, 3040)       # value box, divider, MIDI LEARN: moved left by SHIFT
BTN_SRC = (2716, 1817, 3030, 1896)   # MIDI LEARN button (frame + text)
BTN = (2856, 3030)          # new MORE / LESS button x range
STRIP_Y = (1800, 1925)


def font(px):
    return ImageFont.truetype(FONT, px)


def _fit_label():
    """size + tracking that reproduce TONE (cap 25 px, ink width 101 px)"""
    best = None
    for size in range(28, 42):
        f = font(size); cap = f.getbbox("H")[3] - f.getbbox("H")[1]
        if abs(cap - 25) > 1: continue
        for tr in np.arange(0, 8, 0.25):
            w = ink_width("TONE", f, tr)
            if best is None or abs(w - 101) < best[0]: best = (abs(w - 101), size, tr)
    return best[1], best[2]


def ink_width(text, f, tr):
    x = 0; xs = []
    for i, ch in enumerate(text):
        b = f.getbbox(ch); xs.append((x + b[0], x + b[2])); x += f.getlength(ch) + tr
    return xs[-1][1] - xs[0][0]


def draw_text(img, text, cx, cy_top, f, tr, col=LABEL_COL, anchor="c"):
    """letterspaced text whose cap top sits at cy_top; anchor c / l / r on the ink box"""
    d = ImageDraw.Draw(img); w = ink_width(text, f, tr); hb = f.getbbox("H")
    x = cx - w / 2 if anchor == "c" else (cx if anchor == "l" else cx - w)
    x -= f.getbbox(text[0])[0]
    for ch in text:
        d.text((x, cy_top - hb[1]), ch, font=f, fill=col); x += f.getlength(ch) + tr
    return w


LSIZE, LTR = None, None


def _init_fonts():
    global LSIZE, LTR
    if LSIZE is None: LSIZE, LTR = _fit_label()


def clean_panel(band, full):
    """row-1 band -> the same panel with every printed part removed (smooth tone + the panel's own grain)"""
    b = band.astype(np.float32); out = b.copy()
    inner = b[:, XI0:XI1]
    lum = inner.mean(2)
    base = np.median(lum)
    mask = (np.abs(lum - base) > 9).astype(np.uint8)
    mask = cv2.dilate(mask, np.ones((9, 9), np.uint8))
    m3 = mask[..., None].astype(np.float32)
    # smooth panel tone from unmasked pixels only (normalised blur)
    w = cv2.GaussianBlur(1 - mask.astype(np.float32), (0, 0), 40)[..., None] + 1e-4
    smooth = cv2.GaussianBlur(inner * (1 - m3), (0, 0), 40) / w
    # grain: high-pass of an empty patch (A-B-C section under the lamps), tiled
    gp = full[ROW_TOP + 160:ROW_TOP + 390, 1990:2470].astype(np.float32); grain = gp - cv2.GaussianBlur(gp, (0, 0), 6)
    th, tw = grain.shape[:2]
    reps = np.tile(grain, (inner.shape[0] // th + 1, inner.shape[1] // tw + 1, 1))[:inner.shape[0], :inner.shape[1]]
    out[:, XI0:XI1] = smooth + reps
    return np.clip(out, 0, 255)


def paste_patch(dst, src_img, sc, dc, r, scale=1.0):
    """circular patch of radius r around src centre sc pasted at dst centre dc (soft edge)"""
    x, y = sc; p = src_img[y - r:y + r, x - r:x + r].astype(np.float32)
    if scale != 1.0:
        n = int(round(2 * r * scale)); p = cv2.resize(p, (n, n), interpolation=cv2.INTER_CUBIC); rr = n // 2
    else: rr = r
    yy, xx = np.mgrid[-rr:rr, -rr:rr] + 0.5; a = np.clip((rr - np.hypot(xx, yy)) / 4.0, 0, 1)
    X, Y = int(round(dc[0])), int(round(dc[1]))
    a = (a * np.clip((LABEL_Y[0] - 6 - (Y + yy)) / 3.0, 0, 1))[..., None]   # never carry row 1's label along
    reg = dst[Y - rr:Y + rr, X - rr:X + rr]
    dst[Y - rr:Y + rr, X - rr:X + rr] = p * a + reg * (1 - a)


def paste_rect(dst, src_img, box, dx, dy):
    x0, y0, x1, y1 = box; dst[y0 + dy:y1 + dy, x0 + dx:x1 + dx] = src_img[y0:y1, x0:x1]


def rule(dst, src_img, x0, x1, y_rows=RULE_ROWS):
    """a horizontal hairline from x0 to x1 (copied rule profile from row 1)"""
    prof = src_img[y_rows[0]:y_rows[1], 200:600].astype(np.float32).mean(1, keepdims=True)
    dst[y_rows[0]:y_rows[1], x0:x1] = np.repeat(prof, x1 - x0, axis=1)


def title(dst_img_pil, dst_arr, src, text, x0_ui, x1_ui):
    """'—— TEXT ——' between x0 and x1"""
    f = font(LSIZE + 3); tr = LTR + 0.6
    cx = (x0_ui + x1_ui)  # canvas = ui*2, centre = (x0+x1)/2*2
    w = ink_width(text, f, tr); gap = 26
    rule(dst_arr, src, x0_ui * 2, int(cx - w / 2 - gap))
    rule(dst_arr, src, int(cx + w / 2 + gap), x1_ui * 2)
    return (text, cx, TITLE_Y[0], f, tr)


def build_row2(src):
    """src: full background (float, canvas). Returns the row-2 band (rows ROW_TOP..RULE_Y) as uint8 RGB."""
    _init_fonts()
    band = clean_panel(src[ROW_TOP:RULE_Y], src)
    canvas = src.copy(); canvas[ROW_TOP:RULE_Y] = band
    texts = []
    for n, (kind, cx_ui) in R2.items():
        cx = cx_ui * 2
        if kind == "dome": c = (cx, TONE_C[1]); paste_patch(canvas, src, TONE_C, c, 104); box = BOX_PCT
        elif kind == "scallop": c = (cx, GLIDE_C[1]); paste_patch(canvas, src, GLIDE_C, c, 118); box = BOX_MS
        else: c = (cx, ECHO_C[1]); paste_patch(canvas, src, ECHO_C, c, 100); box = BOX_PCT if n == "DECAY" else BOX_MS
        bw = box[2] - box[0]; bx = cx - bw // 2
        canvas[box[1]:box[3], bx:bx + bw] = src[box[1]:box[3], box[0]:box[2]]
        texts.append((n, cx, LABEL_Y[0], font(LSIZE), LTR))
    for x_ui in DIVS_UI:
        x0, y0, x1, y1 = DIV_SRC; X = x_ui * 2 - (x1 - x0) // 2
        canvas[y0:y1, X:X + (x1 - x0)] = src[y0:y1, x0:x1]
    for t, a, b in TITLES: texts.append(title(None, canvas, src, t, a, b))
    # CHARACTER: echo knob tick-free disc area, dots + names around it
    cc = (CHAR_CX_UI * 2, CHAR_CY_UI * 2)
    small = font(int(round(LSIZE * 0.86))); str_ = LTR * 0.7   # the concept's names: cap ~22 canvas px
    R = int(CHAR_R_UI * 2)   # dot radius (canvas)
    dots = []
    for name, ang, rad in zip(CHARS, CHAR_ANG, CHAR_RAD):
        a = np.radians(ang); dx, dy = np.sin(a), -np.cos(a); R = rad * 2
        px, py = cc[0] + R * dx, cc[1] + R * dy; dots.append((px, py))
        lx, ly = cc[0] + (R + 19) * dx, cc[1] + (R + 19) * dy
        anchor = "c" if abs(dx) < 0.3 else ("l" if dx > 0 else "r")
        capH = small.getbbox("H")[3] - small.getbbox("H")[1]
        ty = (py - 16 - capH) if anchor == "c" else ly - capH / 2   # R&B sits just above its dot
        texts.append((name, lx, ty, small, str_, anchor))
    texts.append(("SYNC", SYNC_CX_UI * 2, TITLE_Y[0], font(LSIZE + 3), LTR + 0.6))
    # SYNC lamp (pedal A's unlit lamp) and its label below like a switch
    lx0, ly0, lx1, ly1 = LAMP_SRC; lw, lh = lx1 - lx0, ly1 - ly0
    sx, sy = SYNC_CX_UI * 2 - lw // 2, ECHO_C[1] - lh // 2
    canvas[sy:sy + lh, sx:sx + lw] = src[ly0:ly1, lx0:lx1]
    bw = BOX_PCT[2] - BOX_PCT[0]; bx = SYNC_CX_UI * 2 - bw // 2
    canvas[BOX_PCT[1]:BOX_PCT[3], bx:bx + bw] = src[BOX_PCT[1]:BOX_PCT[3], BOX_PCT[0]:BOX_PCT[2]]
    img = Image.fromarray(np.clip(canvas, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    for (px, py) in dots:
        d.ellipse((px - 7, py - 7, px + 7, py + 7), fill=LABEL_COL)
    for t in texts:
        draw_text(img, t[0], t[1], t[2], t[3], t[4], anchor=t[5] if len(t) > 5 else "c")
    out = np.asarray(img, np.float32)
    return out[ROW_TOP:RULE_Y], dict(char_c=cc, char_dots=dots, sync=(SYNC_CX_UI * 2, ECHO_C[1]),
                                    sync_box=(sx, sy, sx + lw, sy + lh))


def _button(src, x0, x1, text):
    """MIDI LEARN's frame stretched to [x0,x1) with new text; returns the strip rows BTN_SRC y range"""
    bx0, by0, bx1, by1 = BTN_SRC; b = src[by0:by1, bx0:bx1].astype(np.float32)
    e = 40; inner = b[:, e + 2:e + 12].mean(1, keepdims=True)
    W = x1 - x0; out = np.zeros((by1 - by0, W, 3), np.float32)
    out[:, :e] = b[:, :e]; out[:, W - e:] = b[:, -e:]
    mid = np.repeat(b[:, e:e + 1], W - 2 * e, axis=1)          # top/bottom border rows
    body = np.repeat(inner, W - 2 * e, axis=1)
    rows = np.arange(by1 - by0)
    edge = (rows < 8) | (rows > by1 - by0 - 9)
    out[:, e:W - e] = np.where(edge[:, None, None], mid, body)
    img = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))
    _init_fonts()
    f = font(LSIZE - 2); tr = LTR + 0.4
    capH = f.getbbox("H")[3] - f.getbbox("H")[1]
    w = ink_width(text, f, tr); tot = w + 12 + 16; x0t = (W - tot) / 2
    draw_text(img, text, x0t, (by1 - by0) / 2 - capH / 2, f, tr, anchor="l")
    # the little arrow after the word
    d = ImageDraw.Draw(img); ax = x0t + w + 12; ay = (by1 - by0) / 2
    tri = [(ax, ay + 6), (ax + 16, ay + 6), (ax + 8, ay - 7)] if text == "LESS" else [(ax, ay - 6), (ax + 16, ay - 6), (ax + 8, ay + 7)]
    d.polygon(tri, fill=LABEL_COL)
    return np.asarray(img, np.float32)


def groove_map(x):
    """old strip x -> new x for the slider groove (middle shortened by SHIFT)"""
    return np.where(x < GROOVE[0] + 60, x, np.where(x > GROOVE[1] - 60, x - SHIFT,
                    GROOVE[0] + 60 + (x - GROOVE[0] - 60) * (GROOVE[1] - GROOVE[0] - 120 - SHIFT) / (GROOVE[1] - GROOVE[0] - 120)))


def remap_groove(arr, x_off=0):
    """resample a strip region's columns so the groove's middle is shorter (arr spans canvas columns x_off..)"""
    a = arr.astype(np.float32); W = a.shape[1]
    newx = np.arange(W) + x_off
    # inverse map: for each new x, source x
    src_x = np.where(newx < GROOVE[0] + 60, newx, np.where(newx >= GROOVE[1] - 60 - SHIFT, newx + SHIFT,
                     GROOVE[0] + 60 + (newx - GROOVE[0] - 60) * (GROOVE[1] - GROOVE[0] - 120) / (GROOVE[1] - GROOVE[0] - 120 - SHIFT)))
    mapx = (src_x - x_off).astype(np.float32)[None, :].repeat(a.shape[0], 0)
    mapy = np.arange(a.shape[0], dtype=np.float32)[:, None].repeat(W, 1)
    return cv2.remap(a, mapx, mapy, cv2.INTER_CUBIC, borderMode=cv2.BORDER_REPLICATE)


def new_ticks():
    n = 12; a, b = TICKS0[0], TICKS0[1] - SHIFT
    return [a + (b - a) * i / (n - 1) for i in range(n)]


def strip(src, label):
    """the bottom strip rows STRIP_Y with the shorter slider, moved value box/divider/MIDI LEARN and a MORE/LESS button"""
    y0, y1 = STRIP_Y; s = src[y0:y1].astype(np.float32).copy(); out = s.copy()
    clean = clean_panel(s, src)
    # groove: everything from the groove start to the old value box gets the shortened groove; ticks redrawn
    gx0, gx1 = GROOVE[0] - 20, MOVE_X[0]
    g = remap_groove(s[:, gx0:gx1], gx0)
    out[:, gx0:gx1] = g
    ty0, ty1 = TICK_Y[0] - y0, TICK_Y[1] - y0
    out[ty0:ty1, gx0:gx1] = clean[ty0:ty1, gx0:gx1]
    t0 = TICKS0[0]; tick = s[ty0:ty1, t0 - 6:t0 + 7]
    for tx in new_ticks():
        X = int(round(tx)); out[ty0:ty1, X - 6:X + 7] = tick
    # value box, divider, MIDI LEARN slide left; the space they leave is clean panel
    mx0, mx1 = MOVE_X
    moved = s[:, mx0:mx1].copy()
    out[:, mx0 - SHIFT:mx1] = clean[:, mx0 - SHIFT:mx1]
    out[:, mx0 - SHIFT:mx1 - SHIFT] = moved
    # but keep the clean panel between the groove end and the moved value box
    by0 = BTN_SRC[1] - y0; btn = _button(src, BTN[0], BTN[1], label)
    out[by0:by0 + btn.shape[0], BTN[0]:BTN[1]] = btn
    return out


def apply_strip(bg):
    o = bg.astype(np.float32).copy(); o[STRIP_Y[0]:STRIP_Y[1]] = strip(bg, "MORE"); return o


def build_more(bg):
    """MORE deck rows Y0..2000 from the (original) background"""
    src = bg.astype(np.float32)
    row2, info = build_row2(src)
    out = np.zeros((2000 - Y0, src.shape[1], 3), np.float32)
    out[0:RULE_Y + 4 - FRAME_TOP] = src[FRAME_TOP:RULE_Y + 4]            # frame top + row 1 + its rule
    out[ROW_TOP - Y0:RULE_Y - Y0] = row2
    out[RULE_Y - Y0:] = src[RULE_Y:2000]
    out[STRIP_Y[0] - Y0:STRIP_Y[1] - Y0] = strip(src, "LESS")
    # the walnut sides: one continuous piece (row 1's side rows stretched over both rows) so no join shows
    side = src[ROW_TOP:RULE_Y]
    for xs in (slice(0, XI0 + 2), slice(XI1 - 2, src.shape[1])):
        y_a, y_b = ROW_TOP - D - Y0, RULE_Y - Y0          # dest rows covered by both rows
        st = cv2.resize(side[:, xs], (side[:, xs].shape[1], y_b - y_a), interpolation=cv2.INTER_CUBIC)
        out[y_a:y_b, xs] = st
    # the rows above the walnut frame are not deck: transparent (the steel, its legs and pull rods show through), with a
    # soft contact shadow just above the frame instead of a hard band (Joshua: legs missing, dark line across the rug)
    alpha = np.full(out.shape[:2], 255.0, np.float32)
    # the walnut frame's outline (a rounded rectangle measured on ui_bg) decides it, not colour: the rim's grain varies
    fx0, fx1, ftop, fr = FRAME_OUTLINE; xs = np.arange(out.shape[1], dtype=np.float32) + 0.5
    cx_ = np.clip(xs, fx0 + fr, fx1 - fr); dx_ = xs - cx_
    edge = np.where(np.abs(dx_) >= fr, 1e6, ftop + fr - np.sqrt(np.maximum(fr * fr - dx_ * dx_, 0.0))) - FRAME_TOP
    ys = np.arange(TOPBAND, dtype=np.float32)[:, None] + 0.5
    cover = np.clip(ys - edge[None, :] + 0.5, 0.0, 1.0)                       # anti-aliased frame coverage
    shade = 0.38 * np.clip(1.0 - (edge[None, :] - ys) / 12.0, 0.0, 1.0) ** 1.5 * (cover < 1)   # soft contact shadow above it
    top = out[:TOPBAND]
    alpha[:TOPBAND] = 255.0 * (cover + (1 - cover) * shade)
    out[:TOPBAND] = top * (cover / np.maximum(cover + (1 - cover) * shade, 1e-4))[..., None]
    return np.clip(np.dstack([out, alpha]), 0, 255), info


def layout():
    """UI px geometry for the editor"""
    return dict(D_ui=D / 2, deckTop_ui=Y0 / 2, frameTop_ui=(Y0 + RIM_ROW - FRAME_TOP) / 2, shift_ui=SHIFT / 2,
                btn=(BTN[0] / 2, BTN_SRC[1] / 2, (BTN[1] - BTN[0]) / 2, (BTN_SRC[3] - BTN_SRC[1]) / 2),
                row2=[(n, k, cx) for n, (k, cx) in R2.items()], char_c=(CHAR_CX_UI, float(CHAR_CY_UI)), char_scale=CHAR_SCALE,
                char_ang=CHAR_ANG, char_r=min(CHAR_RAD), char_rad=CHAR_RAD, sync_c=(SYNC_CX_UI, ECHO_C[1] / 2),
                lampA_c=((LAMP_SRC[0] + LAMP_SRC[2]) / 4, (LAMP_SRC[1] + LAMP_SRC[3]) / 4))


if __name__ == "__main__":   # preview: python more_deck.py <ui_bg.jpg> <out_dir>
    import sys
    bg = np.asarray(Image.open(sys.argv[1]).convert("RGB"), np.float32); od = sys.argv[2]; os.makedirs(od, exist_ok=True)
    nb = apply_strip(bg); Image.fromarray(nb.astype(np.uint8)).save(os.path.join(od, "normal.jpg"), quality=92)
    more, info = build_more(bg); full = nb.copy(); a_ = more[..., 3:] / 255
    full[Y0:] = more[..., :3] * a_ + full[Y0:] * (1 - a_)
    Image.fromarray(full.astype(np.uint8)).save(os.path.join(od, "more.jpg"), quality=92)
    print(_fit_label(), layout())
