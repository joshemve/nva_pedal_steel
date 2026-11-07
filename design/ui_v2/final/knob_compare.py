"""Measure a Blender knob render against the same knob in APPROVED_main.png.
python final/knob_compare.py <render.png> <render_box_1600px x0,y0,x1,y1> <ref_box_1568px x0,y0,x1,y1> <out.png> [dark]
Finds the knob silhouette in each (flood fill from the centre over pixels unlike the black panel), scales the
render to the reference's pixel size, and reports width/height, centroid offset, silhouette IoU and mean colour.
The output image: ref | render | render with the ref outline in red | abs difference, all at 4x."""
import sys
import numpy as np
from PIL import Image

render, rbox, fbox, out = sys.argv[1], [float(v) for v in sys.argv[2].split(",")], [float(v) for v in sys.argv[3].split(",")], sys.argv[4]
DARK_KNOB = len(sys.argv) > 5     # black knob on black panel: segment by "not panel-flat" instead of brightness


def mask_of(a):
    g = a.astype(float)
    if DARK_KNOB:
        lum = g.mean(2); hx = np.abs(np.diff(lum, axis=1, prepend=lum[:, :1])) + np.abs(np.diff(lum, axis=0, prepend=lum[:1]))
        m = (lum > 14) | (hx > 6)
    else:
        m = (g[..., 0] > 95) & (g[..., 0] - g[..., 2] > 25)    # cream bakelite: bright and warm
    # flood fill from the centre so the printed ticks (separate islands) are dropped
    h, w = m.shape; seen = np.zeros_like(m); stack = [(h // 2, w // 2)]
    while stack:
        y, x = stack.pop()
        if y < 0 or x < 0 or y >= h or x >= w or seen[y, x] or not m[y, x]: continue
        seen[y, x] = True; stack += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
    # close small holes (pointer, specular) by filling rows/cols between extremes
    filled = seen.copy()
    for y in range(h):
        xs = np.where(seen[y])[0]
        if len(xs): filled[y, xs[0]:xs[-1] + 1] = True
    return filled


ref_im = Image.open("APPROVED_main.png").convert("RGB")
ren_im = Image.open(render).convert("RGB")
scale_r = ren_im.size[0] / (rbox[2] - rbox[0])           # render px per 1600-UI px
ref = np.array(ref_im.crop(tuple(int(v) for v in fbox)))
# render crop -> reference pixel scale (1568 / 1600 of the UI px)
target = (ref.shape[1], ref.shape[0])
ren = np.array(ren_im.resize((round(ren_im.size[0] / scale_r * 1568 / 1600), round(ren_im.size[1] / scale_r * 1568 / 1600)), Image.LANCZOS))
# both crops cover the same knob; pad/crop render to the ref crop size around its own centre
mr, mf = mask_of(ren), mask_of(ref)


def stats(m, a):
    ys, xs = np.where(m)
    return dict(w=xs.max() - xs.min() + 1, h=ys.max() - ys.min() + 1, cx=xs.mean(), cy=ys.mean(),
                col=a[m].mean(0).round(1).tolist(), x0=xs.min(), y0=ys.min())


sr, sf = stats(mr, ren), stats(mf, ref)
# align render to reference by mask centroid
dy, dx = round(sf["cy"] - sr["cy"]), round(sf["cx"] - sr["cx"])
H, W = ref.shape[:2]
canvas = np.zeros_like(ref); cm = np.zeros((H, W), bool)
ys, xs = np.mgrid[0:H, 0:W]; yy, xx = ys - dy, xs - dx
ok = (yy >= 0) & (yy < ren.shape[0]) & (xx >= 0) & (xx < ren.shape[1])
canvas[ok] = ren[yy[ok], xx[ok]]; cm[ok] = mr[yy[ok], xx[ok]]
iou = (cm & mf).sum() / max(1, (cm | mf).sum())
both = cm & mf
lum_diff = np.abs(canvas[both].astype(float).mean(1) - ref[both].astype(float).mean(1)).mean() if both.any() else -1
print("REF    w=%d h=%d col=%s" % (sf["w"], sf["h"], sf["col"]))
print("RENDER w=%d h=%d col=%s" % (sr["w"], sr["h"], sr["col"]))
print("IoU=%.3f  size ratio w=%.3f h=%.3f  mean |lum diff| inside=%.1f" % (iou, sr["w"] / sf["w"], sr["h"] / sf["h"], lum_diff))
# shading zones (luminance), measured on each image's own mask so misalignment doesn't skew them
def zones(a, m):
    ys, xs = np.where(m); cy, cx = ys.mean(), xs.mean(); r = np.hypot(ys - cy, xs - cx); R = r.max()
    lum = a[ys, xs].astype(float).mean(1)
    z = dict(top=lum[ys < cy - 0.2 * R].mean(), bottom=lum[ys > cy + 0.2 * R].mean(), centre=lum[r < 0.45 * R].mean(),
             rim=lum[r > 0.8 * R].mean(), rim_bottom=lum[(r > 0.8 * R) & (ys > cy)].mean(), spread=lum.std())
    return {k: round(v, 1) for k, v in z.items()}
zr, zf = zones(canvas, cm), zones(ref, mf)
print("SHADE ref    ", zf)
print("SHADE render ", zr)
print("SHADE delta  ", {k: round(zr[k] - zf[k], 1) for k in zf})
# outline of the reference mask
edge = mf & ~(np.roll(mf, 1, 0) & np.roll(mf, -1, 0) & np.roll(mf, 1, 1) & np.roll(mf, -1, 1))
ov = canvas.copy(); ov[edge] = (255, 40, 40)
diff = np.clip(np.abs(canvas.astype(int) - ref.astype(int)) * 2, 0, 255).astype(np.uint8)
strip = np.concatenate([ref, canvas, ov, diff], 1)
Image.fromarray(strip).resize((strip.shape[1] * 4, strip.shape[0] * 4), Image.LANCZOS).save(out)
