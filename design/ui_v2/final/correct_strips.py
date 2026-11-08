"""Correct knob filmstrip frames toward APPROVED_main.png, per style.
python final/correct_strips.py [styles...]
Needs: final/strips/frames/<style>_####.png (+ <style>.json), final/strips/frames/<style>_ref_0000.png (+ _ref.json):
one frame rendered at the reference pointer angle, and final/strips/plate_walnut.png (NO_KNOBS plate, same scale).
Per style, at the reference angle a0:  ratio = ref / (frame over plate), inside the knob.
  lighting part L = masked gaussian blur of the ratio (sigma ~6 UI px): fixed in screen space, like the light
  detail part   D = ratio / L                                       : rotates with the knob (flutes, pointer)
Frame at angle a gets  rgb * L * rotate(D, a - a0). At a0 this reproduces the reference inside the silhouette.
Writes corrected frames to final/strips/frames_fixed/ and a report of the error before/after at a0."""
import glob, json, os, sys
import numpy as np
from PIL import Image

D_ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "strips")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = Image.open(os.path.join(ROOT, "APPROVED_main.png")).convert("RGB")
REF_ANG = {"dome": 35, "scallop": 30, "space": -100}
SIGMA_UI = 6.0


def gblur(a, sigma):
    """Separable gaussian blur of a 2D float array (numpy only)."""
    r = int(3 * sigma) + 1; x = np.arange(-r, r + 1); k = np.exp(-x * x / (2 * sigma * sigma)); k /= k.sum()
    a = np.apply_along_axis(lambda v: np.convolve(np.pad(v, r, mode="edge"), k, "valid"), 0, a)
    return np.apply_along_axis(lambda v: np.convolve(np.pad(v, r, mode="edge"), k, "valid"), 1, a)


def ref_patch(origin, size_ui, px):
    """Reference pixels covering the frame's UI-px box, resampled to px x px (ref is 1568x1003 for the 1600x1000 layout)."""
    sx, sy = REF.size[0] / 1600, REF.size[1] / 1000
    box = (origin[0] * sx, origin[1] * sy, (origin[0] + size_ui) * sx, (origin[1] + size_ui) * sy)
    return np.asarray(REF.resize((px, px), Image.LANCZOS, box=box), float)


def aligned_ref(org, size_ui, fs, comp, sc, M=20, mask=None):
    """Reference patch shifted so its printed ticks/knob line up with our composite (integer search at frame px)."""
    big = ref_patch((org[0] - M / sc, org[1] - M / sc), size_ui + 2 * M / sc, fs + 2 * M)
    g = comp.mean(2); gb = big.mean(2); best = (1e18, 0, 0)
    for dy in range(-M, M + 1):
        for dx in range(-M, M + 1):
            dif = np.abs(gb[M + dy:M + dy + fs, M + dx:M + dx + fs] - g)
            e = dif[mask].mean() if mask is not None else dif.mean()
            if e < best[0]: best = (e, dx, dy)
    _, dx, dy = best
    return big[M + dy:M + dy + fs, M + dx:M + dx + fs], (dx, dy)


def cream_mask(a):
    return (a[..., 0] > 95) & (a[..., 0] - a[..., 2] > 25)


def aligned_ref_cream(org, size_ui, fs, comp, sc, M=24, own=None):
    """Cream knobs: shift so the cream bakelite centroids coincide (robust to skirt/tick differences)."""
    big = ref_patch((org[0] - M / sc, org[1] - M / sc), size_ui + 2 * M / sc, fs + 2 * M)
    mo = cream_mask(comp); mb = cream_mask(big)
    if own is not None: mo &= own   # only our knob body (cream ticks on the plate would bias the centroid)
    # keep the blob nearest the centre of the big patch (drop ticks/labels)
    h, w = mb.shape; yy, xx = np.mgrid[0:h, 0:w]; mb &= np.hypot(yy - h / 2, xx - w / 2) < fs * 0.45
    oy, ox = np.argwhere(mo).mean(0); by, bx = np.argwhere(mb).mean(0)
    dx, dy = int(round(bx - M - ox)), int(round(by - M - oy))
    return big[M + dy:M + dy + fs, M + dx:M + dx + fs], (dx, dy)


def pointer_angle(ref, inside, dark, rmax=0.72):
    """Pointer angle (deg clockwise from 12) in the aligned reference: dark or light stripe inside the knob."""
    h, w = inside.shape; cy, cx = (h - 1) / 2, (w - 1) / 2
    ys, xs = np.where(inside); R = np.hypot(ys - cy, xs - cx).max()
    lum = ref.mean(2)
    rr = np.hypot(*(np.mgrid[0:h, 0:w] - np.array([cy, cx])[:, None, None]))
    sel = inside & (rr > 0.15 * R) & (rr < rmax * R)
    pm = sel & ((lum < 55) if dark else (lum > 150))
    if pm.sum() < 10: return None
    # largest connected blob = the pointer stripe; its principal axis gives the angle, its far end the sign
    lab = np.zeros(pm.shape, int); best = []
    for y0, x0 in zip(*np.where(pm)):
        if lab[y0, x0]: continue
        st, comp_ = [(y0, x0)], []; lab[y0, x0] = 1
        while st:
            y, x = st.pop(); comp_.append((y, x))
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < h and 0 <= xx < w and pm[yy, xx] and not lab[yy, xx]: lab[yy, xx] = 1; st.append((yy, xx))
        if len(comp_) > len(best): best = comp_
    pts = np.array(best, float); m = pts.mean(0); u, sv, vt = np.linalg.svd(pts - m, full_matrices=False); d = vt[0]
    far = pts[np.argmax(np.hypot(pts[:, 0] - cy, pts[:, 1] - cx))]
    if np.dot(far - m, d) < 0: d = -d
    global DEBUG_PM; DEBUG_PM = np.zeros(pm.shape, bool); DEBUG_PM[tuple(pts.astype(int).T)] = True
    return float(np.degrees(np.arctan2(d[1], -d[0])))


def dilate(m, r):
    out = m.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy <= r * r: out |= np.roll(np.roll(m, dy, 0), dx, 1)
    return out


def rot(a, deg_cw):
    return np.asarray(Image.fromarray(a.astype(np.float32), "F").rotate(-deg_cw, Image.BILINEAR, fillcolor=1.0), float)


def process(style):
    meta = json.load(open(os.path.join(D_, "frames", style + "_ref.json")))
    fs, sc, org = meta["frame_px"], meta["scale"], meta["origin_px"]
    plate = Image.open(os.path.join(D_, "plate_walnut.png")).convert("RGB")
    pc = np.asarray(plate.crop((round(org[0] * sc), round(org[1] * sc), round(org[0] * sc) + fs, round(org[1] * sc) + fs)), float)
    f0 = np.asarray(Image.open(os.path.join(D_, "frames", style + "_ref_0000.png")).convert("RGBA"), float)
    a0 = f0[..., 3:] / 255.0
    comp = f0[..., :3] * a0 + pc * (1 - a0)
    inside = a0[..., 0] > 0.97
    if style == "space": ref, shift = aligned_ref(org, fs / sc, fs, comp, sc, mask=a0[..., 0] > 0.5)   # knob on knob
    else: ref, shift = aligned_ref_cream(org, fs / sc, fs, comp, sc)
    pa = pointer_angle(ref, inside, style != "space", 0.6 if style == "dome" else 0.72)
    ref_ptr = DEBUG_PM.copy() if pa is not None else np.zeros_like(inside)
    lum0 = comp.mean(2); h_, w_ = inside.shape
    rr_ = np.hypot(*(np.mgrid[0:h_, 0:w_] - np.array([(h_ - 1) / 2, (w_ - 1) / 2])[:, None, None]))
    our_ptr = inside & (rr_ < 0.8 * rr_[inside].max()) & ((lum0 < 70) if style != "space" else (lum0 > 140))
    ptr = dilate(ref_ptr | our_ptr, int(3 * sc))
    print("%-8s ref aligned by %s px; reference pointer at %s deg (rendered at %s)" % (style, shift, None if pa is None else round(pa, 1), meta.get("angle")))
    if os.environ.get("ANGLES_ONLY"):
        dbg = ref.copy(); dbg[DEBUG_PM] = (255, 0, 0)
        Image.fromarray(dbg.astype(np.uint8)).resize((fs * 2, fs * 2)).save(os.path.join(D_, "_ptr_%s.png" % style)); return
    ratio = np.ones_like(comp)
    use = inside & ~ptr                 # pointer pixels (ours and the reference's) take the lighting part only
    ratio[use] = (ref[use] + 2) / (comp[use] + 2)
    ratio = np.clip(ratio, 0.4, 2.5)
    sig = SIGMA_UI * sc; w = gblur(inside.astype(float), sig) + 1e-6
    w = gblur(use.astype(float), sig) + 1e-6
    L = np.stack([gblur(ratio[..., c] * use, sig) / w for c in range(3)], -1)
    Dt = np.where(use[..., None], ratio / L, 1.0)
    # rotation-invariant part of the detail: its average around each ring (radius) about the frame centre
    hh, ww = use.shape; rr = np.hypot(*(np.mgrid[0:hh, 0:ww] - np.array([(hh - 1) / 2, (ww - 1) / 2])[:, None, None]))
    rb = np.round(rr).astype(int); Rad = np.ones_like(Dt)
    for c in range(3):
        num = np.bincount(rb[use], Dt[..., c][use], minlength=rb.max() + 1); den = np.bincount(rb[use], minlength=rb.max() + 1)
        prof = np.where(den > 0, num / np.maximum(den, 1), 1.0)
        Rad[..., c] = np.where(a0[..., 0] > 0.0, prof[rb], 1.0)   # whole body incl. where the pointer was excluded
    inside = use                       # report error on the knob body (pointer excluded)
    err0 = np.abs(comp - ref)[inside].mean()
    fixed0 = np.clip(f0[..., :3] * L * (Dt if os.environ.get("DETAIL") else Rad), 0, 255) * a0 + pc * (1 - a0)
    err1 = np.abs(fixed0 - ref)[inside].mean()
    if os.environ.get("PREVIEW"):
        S = 260
        row = np.concatenate([np.asarray(Image.fromarray(np.clip(x, 0, 255).astype(np.uint8)).resize((S, S), Image.LANCZOS)) for x in (ref, comp, fixed0)], 1)
        Image.fromarray(row).save(os.path.join(D_, "_preview_%s.png" % style))
        print("%-8s mean|err| on knob body at ref angle: before %.1f  after %.1f (0-255)" % (style, err0, err1)); return
    # apply to the strip
    fr = sorted(glob.glob(os.path.join(D_, "frames", style + "_[0-9][0-9][0-9][0-9].png")))
    os.makedirs(os.path.join(D_, "frames_fixed"), exist_ok=True)
    n = len(fr); A0 = REF_ANG[style]
    for i, f in enumerate(fr):
        ang = -135 + 270 * i / (n - 1)
        im = np.asarray(Image.open(f).convert("RGBA"), float)
        # the detail part carries reference lighting, so rotating it makes the light swing: lighting part only
        Dr = np.stack([rot(Dt[..., c], ang - A0) for c in range(3)], -1) if os.environ.get("DETAIL") else Rad
        rgb = np.clip(im[..., :3] * L * Dr, 0, 255)
        Image.fromarray(np.dstack([rgb, im[..., 3:]]).astype(np.uint8), "RGBA").save(f.replace("frames", "frames_fixed"))
    # copy the strip json
    open(os.path.join(D_, "frames_fixed", style + ".json"), "w").write(open(os.path.join(D_, "frames", style + ".json")).read())
    print("%-8s frames=%d  mean|err| inside knob at ref angle: before %.1f  after %.1f (0-255)" % (style, n, err0, err1))


for st in (sys.argv[1:] or ["dome", "scallop", "space"]):
    process(st)
