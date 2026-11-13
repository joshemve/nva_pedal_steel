"""Cream domes (TONE, PICK, TRIM) are rotationally symmetric, so the background keeps the reference's own knob body
(pointer erased by ChatGPT) and the plugin draws only the pointer, cut from the reference and rotated about the dome's
centre. Writes ../strips/ptr_<NAME>.png (128 RGBA pointer-only frames, stacked) and ../strips/ptr_strips.json."""
import json, os
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
hd = np.asarray(Image.open("bg_hd.png"), np.float32)
KN = {"TONE": ("noptr_TONE", 35.0), "PICK": ("noptr_PICK", -50.0), "TRIM": ("noptr_TRIM", 0.0)}
N = 128; out = {}


def nop_canvas(name):
    a = json.load(open("aligned/%s.json" % name)); x0, y0, x1, y1 = a["canvas_box"]
    return (x0, y0, x1, y1), np.asarray(Image.open("aligned/%s.png" % name), np.float32)


for K, (job, a_ref) in KN.items():
    (x0, y0, x1, y1), nop = nop_canvas(job); ref = hd[y0:y1, x0:x1]
    d = (nop.mean(2) - ref.mean(2))                          # pointer is darker than the cream it sits on
    core = cv2.morphologyEx((d > 25).astype(np.uint8), cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    n, lab, stats, cent = cv2.connectedComponentsWithStats(core)
    hh, ww = core.shape; best = max(range(1, n), key=lambda i: stats[i, cv2.CC_STAT_AREA] - 0.02 * np.hypot(cent[i][0] - ww / 2, cent[i][1] - hh / 2) ** 2)
    m = cv2.dilate((lab == best).astype(np.uint8), np.ones((9, 9), np.uint8)) > 0
    # dome centre = centroid of the cream top (pointer erased); rotation axis
    cream = (nop[..., 0] > 150) & (nop[..., 0] - nop[..., 2] > 30)
    if K == "TRIM":   # cream knob on a cream bar: its top face is a touch darker (170-195) than the bar (~200+)
        L_ = cv2.GaussianBlur(nop.mean(2), (0, 0), 1.0); cream = (L_ > 168) & (L_ < 195)
        cream = cv2.morphologyEx(cream.astype(np.uint8), cv2.MORPH_OPEN, np.ones((5, 5), np.uint8)) > 0
    nc, lc, sc_, cc = cv2.connectedComponentsWithStats(cream.astype(np.uint8))
    ci = max(range(1, nc), key=lambda i: sc_[i, cv2.CC_STAT_AREA] - 0.05 * np.hypot(cc[i][0] - ww / 2, cc[i][1] - hh / 2) ** 2)
    ys, xs = np.where(lc == ci); cx, cy = xs.mean(), ys.mean(); asp = (ys.max() - ys.min()) / (xs.max() - xs.min())
    # pointer RGBA: dark ink colour c, alpha from how far each pixel moved from the plain dome toward c
    dark = ref[m & (d > np.percentile(d[m], 80))]; c = np.median(dark, 0)
    al = np.clip(((nop - ref) / np.maximum(nop - c, 1)).mean(2), 0, 1) * m
    al = cv2.GaussianBlur(al.astype(np.float32), (0, 0), 0.6)
    if K == "TRIM":   # the pointer's tip touches the TRIM label: keep only the bar itself, not the label's specks (Joshua 2026-11-12)
        nb, lb = cv2.connectedComponents((al > 0.08).astype(np.uint8))
        keep = max(range(1, nb), key=lambda i: ((lb == i) & (lab == best)).sum())
        al = al * (cv2.dilate((lb == keep).astype(np.uint8), np.ones((3, 3), np.uint8)) > 0)
    col = np.where(al[..., None] > 0.05, np.clip((ref - (1 - al[..., None]) * nop) / np.maximum(al[..., None], 0.05), 0, 255), c)
    rgba = np.dstack([col, al * 255]).astype(np.float32)
    # frame: square around the rotation centre, big enough for the pointer at any angle
    ys_, xs_ = np.where(al > 0.02); rmax = np.hypot(xs_ - cx, (ys_ - cy) / asp).max() + 4; half = int(np.ceil(rmax))   # unsquashed: turned sideways the pointer reaches further (TRIM's tip was clipped near +/-110 deg)
    gx, gy = x0 + cx, y0 + cy                               # canvas coords of the axis
    fx0, fy0 = int(np.floor(gx)) - half, int(np.floor(gy)) - half; fs = 2 * half + 2
    pcx, pcy = gx - fx0, gy - fy0                           # axis inside the frame (sub-pixel)
    src = np.zeros((fs, fs, 4), np.float32)
    sx0, sy0 = fx0 - x0, fy0 - y0
    src_c = rgba[max(sy0, 0):sy0 + fs, max(sx0, 0):sx0 + fs]; src[max(-sy0, 0):max(-sy0, 0) + src_c.shape[0], max(-sx0, 0):max(-sx0, 0) + src_c.shape[1]] = src_c
    pm = src.copy(); pm[..., :3] *= pm[..., 3:] / 255        # rotate premultiplied
    S_ = np.array([[1, 0, 0], [0, 1 / asp, pcy * (1 - 1 / asp)], [0, 0, 1]])   # undo the tilt squash, rotate, redo it
    frames = []
    for k in range(N):
        ang = -135 + 270 * k / (N - 1); phi = np.radians(ang - a_ref)
        R = np.array([[np.cos(phi), -np.sin(phi), pcx - pcx * np.cos(phi) + pcy * np.sin(phi)],
                      [np.sin(phi), np.cos(phi), pcy - pcx * np.sin(phi) - pcy * np.cos(phi)], [0, 0, 1]])
        Mx = np.linalg.inv(S_) @ R @ S_
        f = cv2.warpAffine(pm, Mx[:2], (fs, fs), flags=cv2.INTER_LINEAR, borderValue=0)
        a_ = f[..., 3:] / 255; f[..., :3] = np.where(a_ > 1e-3, f[..., :3] / np.maximum(a_, 1e-3), 0)
        frames.append(np.clip(f, 0, 255).astype(np.uint8))
    Image.fromarray(np.concatenate(frames, 0), "RGBA").save("../strips/ptr_%s.png" % K, optimize=True)
    out[K] = dict(frames=N, frame_px=fs, origin_canvas=[fx0, fy0], axis_canvas=[gx, gy], ref_angle=a_ref, aspect=round(float(asp), 4),
                  ink=[float(v) for v in c])
    print("%-5s axis %.1f,%.1f  frame %d  aspect %.3f  ink %s" % (K, gx / 2, gy / 2, fs, asp, np.round(c).astype(int)))
json.dump(out, open("../strips/ptr_strips.json", "w"), indent=1)
