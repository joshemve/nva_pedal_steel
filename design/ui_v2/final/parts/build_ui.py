"""Assemble the plugin's 2x UI from the ChatGPT parts (all registered to APPROVED_main.png by align_parts.py).

  bg_hd.png      the reference at 3200x2000, sharpened by the HD tiles (feathered blend, low-frequency locked to the ref)
  bg_clean.png   bg_hd with every moving part removed: knobs, display text, meter level, slider thumb, value text,
                 mode text, trim knob, pedal lights/press (this is what the plugin draws first; also the knob strips' plate)
  sprites/*.png  RGBA layers drawn over bg_clean (pedal_<X>_on, meter_on, slider_thumb), each full canvas-box crop,
                 alpha = where that state differs from bg_clean; sprites.json gives their canvas boxes.
  _ref_state.png bg_clean + the reference-state sprites (no knobs/text) for checking.
"""
import json, os, glob
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
CW, CH = 3200, 2000
REF = np.asarray(Image.open("../../APPROVED_main.png").convert("RGB").resize((CW, CH), Image.LANCZOS), np.float32)
A = lambda n: json.load(open("aligned/%s.json" % n))
img = lambda n: np.asarray(Image.open("aligned/%s.png" % n).convert("RGB"), np.float32)
blur = lambda a, s: cv2.GaussianBlur(a, (0, 0), s)
os.makedirs("sprites", exist_ok=True)


def feather_box(h, w, f):
    y = np.minimum(np.arange(h), np.arange(h)[::-1])[:, None]; x = np.minimum(np.arange(w), np.arange(w)[::-1])[None, :]
    return np.clip(np.minimum(x, y) / f, 0, 1).astype(np.float32)


def hd_base():
    acc = np.zeros((CH, CW, 3), np.float32); wsum = np.zeros((CH, CW, 1), np.float32)
    for p in sorted(glob.glob("aligned/hd_tile_*.png")):
        n = os.path.basename(p)[:-4]; x0, y0, x1, y1 = A(n)["canvas_box"]
        t = img(n); x0c, y0c = max(x0, 0), max(y0, 0); x1c, y1c = min(x1, CW), min(y1, CH)
        t = t[y0c - y0:y1c - y0, x0c - x0:x1c - x0]
        w = feather_box(t.shape[0], t.shape[1], 60)[..., None]
        # keep the reference's low frequencies (tone, glare, position); take only the tile's fine detail
        r = REF[y0c:y1c, x0c:x1c]; t = blur(r, 2.5) + (t - blur(t, 2.5))
        acc[y0c:y1c, x0c:x1c] += t * w; wsum[y0c:y1c, x0c:x1c] += w
    out = np.where(wsum > 1e-3, acc / np.maximum(wsum, 1e-3), REF)
    # wherever a tile disagrees with the reference at mid frequencies (a hallucinated detail), fall back to the ref
    bad = np.abs(blur(out, 1.2) - blur(REF, 1.2)).mean(2) > 18
    bad = blur(cv2.dilate(bad.astype(np.uint8), np.ones((7, 7), np.uint8)).astype(np.float32), 3)[..., None]
    print("hd: fallback to reference on %.2f%% of pixels" % (100 * (bad > 0.5).mean()))
    out = np.clip(out * (1 - bad) + REF * bad, 0, 255)
    return backproject(out)


REF0 = np.asarray(Image.open("../../APPROVED_main.png").convert("RGB"), np.float32)


def backproject(x, iters=4):
    """Make x (canvas res) scale back down to exactly the reference, keeping its extra fine detail."""
    for _ in range(iters):
        low = np.asarray(Image.fromarray(x.clip(0, 255).astype(np.uint8)).resize(REF0.shape[1::-1], Image.LANCZOS), np.float32)
        res = REF0 - low
        up = np.asarray(Image.fromarray(np.clip(res + 128, 0, 255).astype(np.uint8)).resize((CW, CH), Image.BICUBIC), np.float32) - 128
        x = np.clip(x + up, 0, 255)
    low = np.asarray(Image.fromarray(x.clip(0, 255).astype(np.uint8)).resize(REF0.shape[1::-1], Image.LANCZOS), np.float32)
    print("hd: scaled back to the reference, mean |err| %.2f" % np.abs(low - REF0).mean())
    return x


def state_patch(name, base, thr=10, grow=9, soft=4, box=None, text=False):
    """(canvas box, rgb, alpha) of job `name` where it differs from the reference (optionally limited to a canvas box)."""
    a = A(name); x0, y0, x1, y1 = a["canvas_box"]; g = img(name)
    x0c, y0c, x1c, y1c = max(x0, 0), max(y0, 0), min(x1, CW), min(y1, CH)
    g = g[y0c - y0:y1c - y0, x0c - x0:x1c - x0]; r = REF[y0c:y1c, x0c:x1c]
    d = (np.abs(blur(g, 2) - blur(r, 2)).mean(2) > thr).astype(np.uint8)
    if text:   # only where text was removed: bright strokes gone; thin outline differences don't count
        d = (np.abs(r.mean(2) - g.mean(2)) > 30).astype(np.uint8)   # light text on the deck, dark text on the cream bar
        d = cv2.morphologyEx(d, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    if box is not None:
        lim = np.zeros_like(d); bx0, by0, bx1, by1 = box
        lim[max(by0 - y0c, 0):max(by1 - y0c, 0), max(bx0 - x0c, 0):max(bx1 - x0c, 0)] = 1; d &= lim
    al = blur(cv2.dilate(d, np.ones((grow, grow), np.uint8)).astype(np.float32), soft)
    # detail from the HD base where the patch didn't change anything; generated pixels where it did
    b = base[y0c:y1c, x0c:x1c]
    return (x0c, y0c, x1c, y1c), g, np.clip(al * 1.5, 0, 1), b


def apply(canvas, name, **kw):
    (x0, y0, x1, y1), g, al, _ = state_patch(name, canvas, **kw)
    canvas[y0:y1, x0:x1] = g * al[..., None] + canvas[y0:y1, x0:x1] * (1 - al[..., None])
    return al


def sprite(name, src, base_clean, box, thr=8):
    """RGBA sprite of `src` (canvas-space image) over `base_clean` inside canvas box: alpha from their difference."""
    x0, y0, x1, y1 = box; s = src[y0:y1, x0:x1]; b = base_clean[y0:y1, x0:x1]
    d = (np.abs(blur(s, 1.5) - blur(b, 1.5)).mean(2) > thr).astype(np.uint8)
    al = np.clip(blur(cv2.dilate(d, np.ones((5, 5), np.uint8)).astype(np.float32), 2.5) * 1.4, 0, 1)
    Image.fromarray(np.dstack([s, al * 255]).astype(np.uint8), "RGBA").save("sprites/%s.png" % name)
    return {"box": [int(v) for v in box]}


def paste_job(canvas_like, name):
    out = canvas_like.copy(); a = A(name); x0, y0, x1, y1 = a["canvas_box"]; g = img(name)
    x0c, y0c, x1c, y1c = max(x0, 0), max(y0, 0), min(x1, CW), min(y1, CH)
    out[y0c:y1c, x0c:x1c] = g[y0c - y0:y1c - y0, x0c - x0:x1c - x0]; return out


if __name__ == "__main__":
    base = hd_base(); Image.fromarray(base.astype(np.uint8)).save("bg_hd.png")
    clean = base.copy()
    for n in ["noptr_TONE", "noptr_PICK", "knobclean_GLIDE", "knobclean_SWELL", "knobclean_ECHO", "knobclean_REVERB",
              "display_empty", "meter_off", "noptr_TRIM", "groove_empty", "modebox_empty", "pctbox_empty",
              "trimdb_empty", "pillempty_TONE", "pillempty_PICK", "pillempty_GLIDE", "pillempty_SWELL", "pillempty_ECHO", "pillempty_REVERB"]:
        if os.path.exists("aligned/%s.png" % n):
            b = A(n)["part_box_canvas"]; m = 24 if n.startswith("pill") else 6   # generous: the diff mask picks the pixels
            txt = n.startswith(("pill", "modebox", "pctbox", "trimdb"))
            apply(clean, n, box=[b[0] - m, b[1] - m, b[2] + m, b[3] + m], text=txt, grow=7 if txt else 9, soft=2 if txt else 4)
        else: print("missing", n)
    pb = A("pedals_unlit")["part_box_canvas"]; apply(clean, "pedals_unlit", box=pb)
    # unlit pedal section: ChatGPT redrew the whole section from the reference ("keep everything, A and B off and raised
    # like C"); used whole (not diff-masked), feathered into the panel at its edges, so the panel texture is one piece
    if os.path.exists("aligned/pedalsec_unlit_1.png"):
        a_ = A("pedalsec_unlit_1"); gx0, gy0, gx1, gy1 = a_["canvas_box"]; g_ = img("pedalsec_unlit_1")
        sx0, sy0, sx1, sy1 = round(948 * CW / 1568), round(712 * CH / 1003), round(1234 * CW / 1568), round(884 * CH / 1003)
        sec = g_[sy0 - gy0:sy1 - gy0, sx0 - gx0:sx1 - gx0]; h_, w_ = sec.shape[:2]
        fx = np.clip(np.minimum(np.arange(w_), w_ - 1 - np.arange(w_)) / 18.0, 0, 1)
        fy = np.clip(np.minimum(np.arange(h_), h_ - 1 - np.arange(h_)) / 14.0, 0, 1)
        f_ = (fx[None, :] * fy[:, None])[..., None]
        clean[sy0:sy1, sx0:sx1] = sec * f_ + clean[sy0:sy1, sx0:sx1] * (1 - f_)
    RX_, RY_ = CW / 1568, CH / 1003
    def cbox(x0r, y0r, x1r, y1r): return [round(x0r * RX_), round(y0r * RY_), round(x1r * RX_), round(y1r * RY_)]
    import bar_plate; clean = bar_plate.rebuild(clean)   # no tone bar in the plate: it is drawn live
    RX_, RY_ = CW / 1568, CH / 1003
    def paste_region(job, box_ref, feather=12):   # a whole region of an aligned ChatGPT image, soft edges
        if not os.path.exists("aligned/%s.png" % job): return False
        a_ = A(job); gx0, gy0, _, _ = a_["canvas_box"]; g_ = img(job)
        sx0, sy0, sx1, sy1 = [round(v * s_) for v, s_ in zip(box_ref, (CW / 1568, CH / 1003, CW / 1568, CH / 1003))]
        sec = g_[sy0 - gy0:sy1 - gy0, sx0 - gx0:sx1 - gx0]; h_, w_ = sec.shape[:2]
        fx = np.clip(np.minimum(np.arange(w_), w_ - 1 - np.arange(w_)) / feather, 0, 1)
        fy = np.clip(np.minimum(np.arange(h_), h_ - 1 - np.arange(h_)) / feather, 0, 1)
        f_ = (fx[None, :] * fy[:, None])[..., None]
        clean[sy0:sy1, sx0:sx1] = sec * f_ + clean[sy0:sy1, sx0:sx1] * (1 - f_); return True
    # realistic end pieces (Joshua: the keyhead and changer felt boxy): sculpted keyhead, rounded machined changer
    paste_region("ends_walnut_1", (100, 138, 468, 318)); paste_region("ends_walnut_1", (1278, 138, 1404, 318))
    # then a much smaller, understated keyhead (Joshua: the keyheads were big and clunky), real-steel style
    paste_region("keyhead_walnut_1", (96, 132, 476, 322))
    # the deck's own background (Joshua: "one proper background without lighting, knobs sit on top"): two whole ChatGPT
    # redraws of the reference deck with every moving part removed, lamps off, pedals raised; used whole, one texture
    def paste_plate(job, x0r, x1r, feather_l, feather_r):
        a_ = A(job); gx0, gy0, _, _ = a_["canvas_box"]; g_ = img(job)
        sx0, sx1 = round(x0r * CW / 1568), round(x1r * CW / 1568); sy0, sy1 = round(670 * CH / 1003), round(992 * CH / 1003)
        sec = g_[sy0 - gy0:sy1 - gy0, sx0 - gx0:sx1 - gx0]; h_, w_ = sec.shape[:2]
        xs = np.arange(w_, dtype=np.float32)
        fx = np.minimum(np.clip(xs / max(feather_l, 1), 0, 1) if feather_l else 1.0, np.clip((w_ - 1 - xs) / max(feather_r, 1), 0, 1) if feather_r else 1.0)
        fx = np.broadcast_to(fx, (w_,)) if np.ndim(fx) else np.full(w_, fx, np.float32)
        ys = np.arange(h_, dtype=np.float32); fy = np.clip(np.minimum(ys, h_ - 1 - ys) / 8.0, 0, 1)
        f_ = (fx[None, :] * fy[:, None])[..., None]
        clean[sy0:sy1, sx0:sx1] = sec * f_ + clean[sy0:sy1, sx0:sx1] * (1 - f_)
    if os.path.exists("aligned/deckL_plate_2.png") and os.path.exists("aligned/deckR_plate_1.png"):
        paste_plate("deckL_plate_2", 12, 642, 0, 24)        # VOICE + FEEL + MODE (seam in the panel left of the SPACE divider)
        paste_plate("deckR_plate_1", 618, 1268, 24, 16)     # SPACE + pedals + slider + % box
    else:
        import retexture; clean, _ = retexture.retexture(clean, base)   # fallback: patched panel, real colour + grain
    Image.fromarray(clean.astype(np.uint8)).save("bg_clean.png")
    S = {}
    # pedals: three columns of the pedal box; lit A/B from the (HD) reference, lit C from the generated all-lit image
    x0, y0, x1, y1 = pb; cw = (x1 - x0) / 3
    # lit + pressed pedals: the reference's own A and B (real lamp glow and pressed block), and A moved onto C (the
    # pedals are identical, lamps 2055 / 2228 / 2388 canvas px); alpha = the pressed block plus wherever it glows
    lamps = {"A": 2055, "B": 2228, "C": 2388}
    for i, k in enumerate("ABC"):
        box = (round(x0 + i * cw), y0, round(x0 + (i + 1) * cw), y1)
        src_x = lamps["A" if k == "C" else k]; dx_ = lamps[k] - src_x
        lit = clean.copy(); bx0, by0, bx1, by1 = box
        lit[by0:by1, bx0:bx1] = base[by0:by1, bx0 - dx_:bx1 - dx_]
        Lg = blur(lit.mean(2), 1.5) - blur(clean.mean(2), 1.5)
        a_glow = np.clip((Lg - 6.0) / 22.0, 0, 1)
        blk = np.zeros(a_glow.shape, np.float32); blk[1488:1752, lamps[k] - 74:lamps[k] + 74] = 1   # the pressed block
        yy_, xx_ = np.mgrid[0:blk.shape[0], 0:blk.shape[1]]
        blk[(xx_ - lamps[k]) ** 2 + (yy_ - 1466) ** 2 < 26 ** 2] = 1                                   # the whole lamp
        a_ = np.clip(np.maximum(a_glow, blur(blk, 3.0)), 0, 1)[by0:by1, bx0:bx1]
        Image.fromarray(np.dstack([lit[by0:by1, bx0:bx1], a_ * 255]).astype(np.uint8), "RGBA").save("sprites/pedal_%s_on.png" % k)
        S["pedal_%s_on" % k] = {"box": [int(v) for v in box]}
    mb = A("meter_on")["part_box_canvas"]; S["meter_on"] = sprite("meter_on", paste_job(base, "meter_on"), clean, mb, thr=6)
    sb = A("slider_empty")["part_box_canvas"]
    # handle only: reference vs the same slider with its fill but no handle; fill: whole groove filled vs empty groove
    S["slider_thumb"] = sprite("slider_thumb", base, paste_job(base, "slider_empty"), sb)
    S["slider_fill"] = sprite("slider_fill", paste_job(base, "groove_full"), clean, sb, thr=6)
    # handle layer: only the handle (largest cream blob) and its shadow; fill layer: only the groove band, no ticks
    th = np.asarray(Image.open("sprites/slider_thumb.png"), np.float32)
    cream = ((th[..., 0] > 150) & (th[..., 0] - th[..., 2] > 25) & (th[..., 3] > 128)).astype(np.uint8)
    n_, lab_, st_, _ = cv2.connectedComponentsWithStats(cream); i_ = 1 + int(np.argmax(st_[1:, cv2.CC_STAT_AREA]))
    x_, y_, w_, h_ = st_[i_, :4]; keep = np.zeros(th.shape[:2], np.float32); keep[max(y_ - 8, 0):y_ + h_ + 10, max(x_ - 8, 0):x_ + w_ + 10] = 1
    th[..., 3] *= blur(keep, 2); Image.fromarray(th.astype(np.uint8), "RGBA").save("sprites/slider_thumb.png")
    S["slider_thumb"]["handle_cx"] = float(S["slider_thumb"]["box"][0] + x_ + w_ / 2)
    fl = np.asarray(Image.open("sprites/slider_fill.png"), np.float32)
    rows = (fl[..., 3] > 128).mean(1); band = np.flatnonzero(rows > 0.5); keep = np.zeros(fl.shape[:2], np.float32)
    keep[max(band.min() - 4, 0):band.max() + 5] = 1; fl[..., 3] *= blur(keep, 1.5)
    Image.fromarray(fl.astype(np.uint8), "RGBA").save("sprites/slider_fill.png")
    print("slider handle centre (canvas) %.1f, groove rows %d..%d" % (S["slider_thumb"]["handle_cx"], band.min(), band.max()))
    # knee levers engaged (one generated image, split between the two levers) and MIDI LEARN lit
    if os.path.exists("aligned/levers_on_1.png"):
        lev = paste_job(clean, "levers_on_1")
        S["lever_L_on"] = sprite("lever_L_on", lev, clean, cbox(1250, 712, 1368, 868))
        S["lever_R_on"] = sprite("lever_R_on", lev, clean, cbox(1372, 712, 1492, 868))
    if os.path.exists("aligned/midi_on_2.png"):
        S["midi_on"] = sprite("midi_on", paste_job(clean, "midi_on_2"), clean, cbox(1318, 900, 1498, 962), thr=5)
    # the steel: under-neck knee levers pushed, floor pedals pressed (split per pedal), tone bar (sprite) on a bar-free plate
    for nm, bx in (("kl_left_pushed", (505, 395, 590, 515)), ("kl_right_pushed", (840, 395, 925, 515))):
        if os.path.exists("aligned/%s_2.png" % nm):
            S[nm] = sprite(nm, paste_job(clean, nm + "_2"), clean, cbox(*bx))
    S["tone_bar"] = sprite("tone_bar", base, clean, [bar_plate.X0 - 4, bar_plate.Y0, bar_plate.X1 + 4, bar_plate.Y1], thr=6)
    tb = np.asarray(Image.open("sprites/tone_bar.png"), np.float32); xs_ = np.arange(tb.shape[1]) + bar_plate.X0 - 4
    bx_ = S["tone_bar"]["box"]; dd = np.abs(blur(base, 1.2) - blur(clean, 1.2))[bx_[1]:bx_[3], bx_[0]:bx_[2]].mean(2)
    soft_a = np.clip(dd / 28.0, 0, 1)                                   # reflection / shadow: partial, by how much it changed
    ys_ = np.arange(tb.shape[0]) + bx_[1]; body = (ys_ >= 314) & (ys_ <= 548)   # the bar body itself is opaque
    soft_a[body] = np.maximum(soft_a[body], 1.0)
    cap = (ys_ > 548) & (ys_ < 580); bb_ = base[bx_[1]:bx_[3], bx_[0]:bx_[2]]
    copper = ((bb_[..., 0] > 70) & (bb_[..., 0] - bb_[..., 2] > 18)).astype(np.float32)   # rounded copper-lit bottom cap only
    soft_a[cap] = blur(copper, 1.0)[cap]
    wood = ys_ >= 578; ramp = np.clip((ys_[wood] - 578) / 30.0, 0, 1); ramp = ramp * ramp * (3 - 2 * ramp)
    soft_a[wood] *= ramp[:, None]                                                        # reflection fades in below the board
    tail = np.clip((bar_plate.Y1 - ys_[wood]) / 24.0, 0, 1); soft_a[wood] *= tail[:, None]   # ...and out at its far end
    tb[..., 3] = 255 * blur(soft_a.astype(np.float32), 0.8)
    colm = np.clip(np.minimum(xs_ - bar_plate.BAR_X0 - 0.5, bar_plate.BAR_X1 - xs_ + 1.5) / 2.0, 0, 1)   # only the bar's own columns
    colw = np.clip(np.minimum(xs_ - bar_plate.BAR_X0 + 2, bar_plate.BAR_X1 - xs_ + 2) / 9.0, 0, 1)   # softer sides on the wood
    colm = np.where((ys_ >= 578)[:, None], (colw * colw * (3 - 2 * colw))[None, :], colm[None, :])
    tb[..., 3] *= colm; Image.fromarray(tb.astype(np.uint8), "RGBA").save("sprites/tone_bar.png")
    S["tone_bar"]["bar_cx"] = (bar_plate.BAR_X0 + bar_plate.BAR_X1) / 2
    # the strip as the reference has it, for the bar at its home fret (fades out as the bar leaves): exact at rest
    hx0, hy0, hx1, hy1 = bar_plate.X0 - 6, bar_plate.Y0, bar_plate.X1 + 6, bar_plate.Y1
    ha = np.ones((hy1 - hy0, hx1 - hx0), np.float32)
    fx = np.clip(np.minimum(np.arange(hx1 - hx0), hx1 - hx0 - 1 - np.arange(hx1 - hx0)) / 4.0, 0, 1); ha *= fx[None, :]
    Image.fromarray(np.dstack([base[hy0:hy1, hx0:hx1], ha * 255]).astype(np.uint8), "RGBA").save("sprites/tone_bar_home.png")
    S["tone_bar_home"] = {"box": [hx0, hy0, hx1, hy1]}
    if os.path.exists("aligned/floor_pressed_2.png"):
        fp = paste_job(clean, "floor_pressed_2")
        for k, (a, b) in zip("ABC", ((628, 708), (712, 795), (799, 880))):
            S["floor_%s" % k] = sprite("floor_%s" % k, fp, clean, cbox(a, 400, b, 660))
    json.dump(S, open("sprites/sprites.json", "w"), indent=1)
    # reference state without knobs/text, for checking
    st = clean.copy()
    for k in ("pedal_A_on", "pedal_B_on", "slider_thumb"):
        sp = np.asarray(Image.open("sprites/%s.png" % k), np.float32); bx = S[k]["box"]; a_ = sp[..., 3:] / 255
        st[bx[1]:bx[3], bx[0]:bx[2]] = sp[..., :3] * a_ + st[bx[1]:bx[3], bx[0]:bx[2]] * (1 - a_)
    Image.fromarray(st.astype(np.uint8)).save("_ref_state.png")
    print("done; sprites:", list(S))
