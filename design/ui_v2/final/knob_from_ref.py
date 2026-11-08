"""SPACE knobs from the reference's own ECHO knob (real pixels) instead of the Blender render, whose lower lobes shaded
into blotches. Every frame = the reference knob rotated to that angle (lobes, pointer and fine detail turn with it), with
its broad lighting held still on screen: the rotated image's low-frequency luminance is swapped for the unrotated one
(top highlight and dark base stay put, like a real knob under a fixed light). The silhouette comes from the matched
Blender frame at the reference angle. Then fix_knob_halo.py adds the contact shadow.
Writes strips/knob_space_orig.png (the input fix_knob_halo.py reads)."""
import json, os
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
import sys
STYLE = sys.argv[1] if len(sys.argv) > 1 else "space"
CFG = {"space":   dict(ref=-88.1, srcs=(("echo_ptr12_1", 88.85), ("echo_ptr3_2", 162.65)), ptr_bright=True, rim=True),
       "scallop": dict(ref=61.7, srcs=(("glide_ptr9_1", -163.65), ("glide_ptr3_2", 35.85)), ptr_bright=False, rim=False)}[STYLE]
S = json.load(open("strips/strips.json")); m = S[STYLE]; n = m["frames"]; fs = m["frame_px"][0]
hd = np.asarray(Image.open("parts/bg_hd.png"), np.float32)
BL = "strips/knob_%s_blender.png" % STYLE
blender = np.asarray(Image.open(BL if os.path.exists(BL) else "strips/knob_%s_orig.png" % STYLE).convert("RGBA"), np.float32)
if not os.path.exists(BL): Image.fromarray(blender.astype(np.uint8), "RGBA").save(BL)
inst = m["instances"][0]; A_REF = CFG["ref"]                  # as designed: the pointer's true angle (measured)
k0 = int(round((A_REF + 135) / 270 * (n - 1)))
alpha = blender[k0 * fs:(k0 + 1) * fs, :, 3] / 255.0
alpha = cv2.GaussianBlur(cv2.erode(alpha, np.ones((3, 3), np.uint8)), (0, 0), 0.6)   # stay inside the real outline
x0 = int(round(inst["cx"] * 2 - fs / 2)); y0 = int(round(inst["cy"] * 2 - fs / 2))
src = hd[y0:y0 + fs, x0:x0 + fs].copy()
c = (fs / 2.0 - 0.5, fs / 2.0 - 0.5)
SIG = 9.0


def broad(rgb, a):
    """Low-frequency luminance of the knob's body (normalized convolution; the panel and the bright pointer are left
    out, so neither the old pointer position nor the new one bends the lighting)."""
    L = rgb.mean(2)
    ptr = (L > 70) if CFG["ptr_bright"] else (L < 75)
    body = a * (cv2.dilate(ptr.astype(np.uint8), np.ones((7, 7), np.uint8)) == 0)
    den = np.maximum(cv2.GaussianBlur(body, (0, 0), SIG), 1e-3)
    return np.stack([cv2.GaussianBlur(rgb[..., ch] * body, (0, 0), SIG) / den for ch in range(3)], 2)   # per channel: hue stays too


# the knob at three real angles: the reference (-101.5) and ChatGPT's same-lighting redraws at 12 and 3 o'clock
def crop_of(job):
    a = json.load(open("parts/aligned/%s.json" % job)); gx0, gy0, _, _ = a["canvas_box"]
    g = np.asarray(Image.open("parts/aligned/%s.png" % job), np.float32)
    return g[y0 - gy0:y0 - gy0 + fs, x0 - gx0:x0 - gx0 + fs].copy()
def rot(img, deg, interp=cv2.INTER_CUBIC, border=cv2.BORDER_REFLECT):
    M = cv2.getRotationMatrix2D(c, -deg, 1.0)                     # clockwise on screen
    return cv2.warpAffine(img, M, (fs, fs), flags=interp, borderMode=border)
SOURCES = [(src, A_REF, alpha)]
for job, off in CFG["srcs"]:   # measured pointer offsets from the reference
    ang = A_REF + off
    if os.path.exists("parts/aligned/%s.png" % job):
        SOURCES.append((crop_of(job), ang, rot(alpha, ang - A_REF, cv2.INTER_LINEAR, cv2.BORDER_CONSTANT)))
SOURCES.sort(key=lambda t: t[1])
for sx in SOURCES: print("source at %.1f deg" % sx[1])

def from_source(sx, ang):
    img, a0, al = sx
    r = rot(img, ang - a0); ra = np.clip(rot(al, ang - a0, cv2.INTER_LINEAR, cv2.BORDER_CONSTANT), 0, 1)
    ratio = np.clip(broad(img, al) / np.maximum(broad(r, ra), 1.0), 0.4, 2.5)
    return np.clip(r * ratio, 0, 255), ra

frames = []
for k in range(n):
    ang = -135 + 270 * k / (n - 1)
    angs = [sx[1] for sx in SOURCES]
    if ang <= angs[0]: parts = [(SOURCES[0], 1.0)]
    elif ang >= angs[-1]: parts = [(SOURCES[-1], 1.0)]
    else:
        i = max(j for j in range(len(angs)) if angs[j] <= ang); t = (ang - angs[i]) / (angs[i + 1] - angs[i])
        parts = [(SOURCES[i], 1 - t), (SOURCES[i + 1], t)]
    lit = np.zeros((fs, fs, 3), np.float32); ra = np.zeros((fs, fs), np.float32)
    rendered = [(from_source(sx, ang), w) for sx, w in parts]
    for (l_, a_), w in rendered: lit += l_ * w; ra += a_ * w
    if len(rendered) == 2:   # the pointer comes from the nearer source only (two slightly different pointers would ghost)
        (l_n, _), w_n = max(rendered, key=lambda t: t[1])
        pm = np.zeros((fs, fs), np.float32)
        for (l_, _), _w in rendered: pm = np.maximum(pm, ((l_.mean(2) > 95) if CFG["ptr_bright"] else (l_.mean(2) < 75)).astype(np.float32))
        pm = np.clip(cv2.GaussianBlur(cv2.dilate(pm, np.ones((5, 5), np.uint8)), (0, 0), 1.2) * 1.5, 0, 1)[..., None]
        lit = l_n * pm + lit * (1 - pm)
    if not CFG["rim"]: frames.append(np.dstack([lit, ra * 255])); continue
    inside = (ra > 0.5).astype(np.uint8)
    dist = cv2.distanceTransform(inside, cv2.DIST_L2, 5)
    band = np.clip((14.0 - dist) / 6.0, 0, 1) * (ra > 0.5) * np.clip((80.0 - lit.mean(2)) / 20.0, 0, 1)
    Lp = lit.mean(2, keepdims=True); core = (dist > 12) & (lit.mean(2) < 70)
    tint = (lit[core].mean(0) / max(lit[core].mean(), 1.0)) if core.sum() > 50 else np.ones(3)
    lit = lit * (1 - band[..., None]) + (Lp * tint) * band[..., None]
    frames.append(np.dstack([lit, ra * 255]))
out = np.concatenate(frames, 0)
Image.fromarray(out.astype(np.uint8), "RGBA").save("strips/knob_%s_orig.png" % STYLE)
print("space strip from the reference knob:", out.shape)
