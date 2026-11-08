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
S = json.load(open("strips/strips.json")); m = S["space"]; n = m["frames"]; fs = m["frame_px"][0]
hd = np.asarray(Image.open("parts/bg_hd.png"), np.float32)
blender = np.asarray(Image.open("strips/knob_space_blender.png" if os.path.exists("strips/knob_space_blender.png")
                                else "strips/knob_space_orig.png").convert("RGBA"), np.float32)
if not os.path.exists("strips/knob_space_blender.png"):
    Image.fromarray(blender.astype(np.uint8), "RGBA").save("strips/knob_space_blender.png")
inst = m["instances"][0]; A_REF = -101.5                      # ECHO as designed
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
    body = a * (cv2.dilate((L > 70).astype(np.uint8), np.ones((7, 7), np.uint8)) == 0)
    den = np.maximum(cv2.GaussianBlur(body, (0, 0), SIG), 1e-3)
    return np.stack([cv2.GaussianBlur(rgb[..., ch] * body, (0, 0), SIG) / den for ch in range(3)], 2)   # per channel: hue stays too


B0 = broad(src, alpha)
frames = []
for k in range(n):
    ang = -135 + 270 * k / (n - 1)
    M = cv2.getRotationMatrix2D(c, -(ang - A_REF), 1.0)          # clockwise on screen
    r = cv2.warpAffine(src, M, (fs, fs), flags=cv2.INTER_CUBIC, borderMode=cv2.BORDER_REFLECT)
    ra = cv2.warpAffine(alpha, M, (fs, fs), flags=cv2.INTER_LINEAR, borderValue=0)
    Br = broad(r, ra)
    ratio = np.clip(B0 / np.maximum(Br, 1.0), 0.4, 2.5)
    lit = np.clip(r * ratio, 0, 255)
    # the warm reflection along the real knob's lower rim belongs to the screen, not the knob: in a thin band inside
    # the outline keep brightness but use the body's own average tint, so it doesn't ride round to the top
    inside = (ra > 0.5).astype(np.uint8)
    dist = cv2.distanceTransform(inside, cv2.DIST_L2, 5)
    band = np.clip((14.0 - dist) / 6.0, 0, 1) * (ra > 0.5) * np.clip((80.0 - lit.mean(2)) / 20.0, 0, 1)   # not the pointer
    Lp = lit.mean(2, keepdims=True); core = (dist > 12) & (lit.mean(2) < 70)
    tint = (lit[core].mean(0) / max(lit[core].mean(), 1.0)) if core.sum() > 50 else np.ones(3)
    lit = lit * (1 - band[..., None]) + (Lp * tint) * band[..., None]
    frames.append(np.dstack([lit, ra * 255]))
out = np.concatenate(frames, 0)
Image.fromarray(out.astype(np.uint8), "RGBA").save("strips/knob_space_orig.png")
print("space strip from the reference knob:", out.shape)
