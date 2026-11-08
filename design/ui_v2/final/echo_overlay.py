"""Overlay our ECHO silhouette (blue) and Joshua's traced outline (red) on the brightened reference crop."""
import numpy as np, json, os
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
J = np.asarray(Image.open("knobs/gpt/joshua_echo_outline.jpg").convert("RGB"), float)
red = (J[..., 0] > 150) & (J[..., 1] < 100) & (J[..., 2] < 100)
m = json.load(open("strips/frames/space_ref.json")); org = m["origin_px"]
al = np.asarray(Image.open("strips/frames/space_ref_normal_0000.png").convert("RGBA"), float)[..., 3] / 255
col = np.asarray(Image.open("strips/frames/space_ref_0000.png").convert("RGBA"), float)
x0, y0 = 746.4 - 56, 777.0 - 56
# resample our alpha into the 512 crop
yy, xx = np.mgrid[0:512, 0:512]; ux = x0 + xx * 112 / 512; uy = y0 + yy * 112 / 512
fj = ((ux - org[0]) * 2).astype(int); fi = ((uy - org[1]) * 2).astype(int)
ok = (fi >= 0) & (fi < al.shape[0]) & (fj >= 0) & (fj < al.shape[1])
big = np.zeros((512, 512), bool); big[ok] = al[fi[ok], fj[ok]] > 0.5
e = big & ~(big & np.roll(big, 1, 0) & np.roll(big, -1, 0) & np.roll(big, 1, 1) & np.roll(big, -1, 1))
ref = np.asarray(Image.open("knobs/gpt/ref_echo.png").convert("RGB").resize((512, 512)), float)
g = np.clip((ref / 255) ** 0.5 * 255, 0, 255); g[e] = [60, 140, 255]; g[red] = [255, 40, 40]
# our raw Blender colour render (brightened the same way) in the same crop, for a side-by-side
ours = np.zeros((512, 512, 3)); a = np.zeros((512, 512, 1))
ours[ok] = col[fi[ok], fj[ok], :3]; a[ok] = col[fi[ok], fj[ok], 3:] / 255
ob = np.clip(((ours * a + ref * (1 - a)) / 255) ** 0.5 * 255, 0, 255)
Image.fromarray(np.concatenate([g, ob], 1).astype(np.uint8)).save("knobs/_echo_vs_joshua.png")
