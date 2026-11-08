"""Knob halos (Joshua: "patchy spotting ... around knobs"). The FEEL/SPACE filmstrips carried a ring of baked plate +
residual outside each knob's silhouette (matcap rim layer); it came out ragged and blotchy at the knob's lower edge.
Replace it in every frame with a smooth contact shadow measured from APPROVED_main.png: shadow strength as a function of
distance from the silhouette and direction around the knob (heavily smoothed), drawn as black with that alpha, so the
(retextured) panel under it shows through. Inside the silhouette nothing changes.
Usage: python fix_knob_halo.py  (reads strips/knob_<style>_orig.png if present, else backs up knob_<style>.png)"""
import json, os, shutil
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
hd = np.asarray(Image.open("parts/bg_hd.png"), np.float32); Lref = hd.mean(2)
S = json.load(open("strips/strips.json"))
REF_ANGLE = {"scallop": (34.8, 33.8), "space": (-88.1, -55.5)}   # the design's pointer angles (PluginEditor anchors)
DMAX, NB = 30, 36
blur = lambda a, s: cv2.GaussianBlur(a, (0, 0), s)

for style in ("scallop", "space"):
    src = "strips/knob_%s.png" % style; orig = "strips/knob_%s_orig.png" % style
    if not os.path.exists(orig): shutil.copy(src, orig)
    strip = np.asarray(Image.open(orig).convert("RGBA"), np.float32)
    m = S[style]; n = m["frames"]; fs = m["frame_px"][0]; c = fs / 2.0
    yy, xx = np.mgrid[0:fs, 0:fs]; theta = (np.degrees(np.arctan2(yy - c + 0.5, xx - c + 0.5)) + 360) % 360
    tb = np.minimum((theta / (360 / NB)).astype(int), NB - 1)

    def geom(frame):
        body = frame[..., 3] > 0.9 * 255
        d = cv2.distanceTransform((~body).astype(np.uint8), cv2.DIST_L2, 5)
        return body, d

    def wide_base(fr):
        # SPACE: the reference's black knob sits on one smooth, dark, wide base at the bottom, not separate lobes with
        # gaps; fill the lower half out to the lobes' outer radius in the knob's own dark base colour (feathered)
        if style != "space" or os.path.exists("strips/knob_space_blender.png"): return fr   # real knob: nothing to fix
        fr = fr.copy(); body = fr[..., 3] > 0.9 * 255
        rr = np.hypot(xx - c + 0.5, yy - c + 0.5)
        rmax = np.percentile(rr[body], 99.0)
        lower = np.clip(np.sin(np.radians(theta)) * 1.6, 0, 1)          # 1 at the bottom, 0 at the sides and above
        lower = lower * lower * (3 - 2 * lower)
        disc = np.clip(rmax - rr + 0.5, 0, 1)                            # anti-aliased round edge
        band = body & (rr > rmax - 9) & (lower > 0.5)
        base = np.median(fr[band][:, :3], 0) if band.sum() > 20 else np.array([18.0, 15.0, 12.0])
        a_new = disc * lower
        add = (~body) & (a_new > 0.01)
        a0 = fr[..., 3] / 255.0
        a_tot = np.maximum(a0, a_new)
        col = np.where(add[..., None], (fr[..., :3] * a0[..., None] + base * (a_tot - a0)[..., None]) / np.maximum(a_tot, 1e-3)[..., None], fr[..., :3])
        fr[..., :3] = col; fr[..., 3] = a_tot * 255
        # the lower lobes' alternating dark/light facets read as blotches: smooth a band inside the lower edge into
        # the reference's even dark base (stronger toward the edge)
        w = np.clip((rr - (rmax - 18)) / 12.0, 0, 1) * lower * (fr[..., 3] > 0.5 * 255)
        sm = cv2.GaussianBlur(fr[..., :3], (0, 0), 4.0)
        tgt = sm * 0.6 + base * 0.4
        fr[..., :3] = fr[..., :3] * (1 - w[..., None]) + tgt * w[..., None]
        return fr

    strip = np.concatenate([wide_base(strip[k * fs:(k + 1) * fs]) for k in range(n)], 0)
    # measure: shadow s = 1 - L / L_panel around the reference knobs, binned by (distance, direction)
    acc = np.zeros((DMAX + 1, NB)); cnt = np.zeros((DMAX + 1, NB))
    for inst, ang in zip(m["instances"], REF_ANGLE[style]):
        k = int(round((ang + 135) / 270 * (n - 1))); fr = strip[k * fs:(k + 1) * fs]
        body, d = geom(fr)
        x0 = int(round(inst["cx"] * 2 - fs / 2)); y0 = int(round(inst["cy"] * 2 - fs / 2))
        L = Lref[y0:y0 + fs, x0:x0 + fs]
        # panel level per direction: the ring just beyond the shadow, ignoring bright ticks
        ring = (d > DMAX + 4) & (d < DMAX + 16) & (L < 60)
        panel = np.array([np.median(L[ring & (tb == b)]) if (ring & (tb == b)).sum() > 10 else np.median(L[ring]) for b in range(NB)])
        outside = (~body) & (d >= 1) & (d <= DMAX) & (L < 60)          # ticks excluded
        s = np.clip(1 - L / np.maximum(panel[tb], 1), 0, 0.95)
        di = np.round(d).astype(int)
        np.add.at(acc, (di[outside], tb[outside]), s[outside]); np.add.at(cnt, (di[outside], tb[outside]), 1)
    prof = acc / np.maximum(cnt, 1)
    # smooth: around the knob (circular) and along the distance; shadow must fade to 0 at DMAX
    prof = np.concatenate([prof[:, -6:], prof, prof[:, :6]], 1)
    prof = cv2.GaussianBlur(prof.astype(np.float32), (0, 0), sigmaX=2.0, sigmaY=1.0)[:, 6:-6]
    prof *= np.clip((DMAX - np.arange(DMAX + 1)) / 8.0, 0, 1)[:, None]
    prof = np.clip(prof, 0, 0.9)
    print(style, "shadow at d=2: min %.2f max %.2f (by direction)" % (prof[2].min(), prof[2].max()))

    out = strip.copy()
    for k in range(n):
        fr = strip[k * fs:(k + 1) * fs]; body, d = geom(fr)
        di = np.clip(np.round(d).astype(int), 0, DMAX)
        sh = prof[di, tb] * (~body)
        edge = (~body) & (d < 1.5) & (fr[..., 3] > 0.05 * 255)        # the silhouette's own anti-aliasing stays
        o = out[k * fs:(k + 1) * fs]
        o[..., :3] = np.where(edge[..., None], fr[..., :3], np.where(body[..., None], fr[..., :3], 0))
        o[..., 3] = np.where(body | edge, fr[..., 3], sh * 255)
    Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA").save(src, optimize=True)
    print("wrote", src)
