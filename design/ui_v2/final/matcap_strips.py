"""Knob strips shaded by a lit-sphere (matcap) fitted to APPROVED_main.png.
python final/matcap_strips.py [styles...]            fit at the reference angle and write strips/frames_fixed/
    env PREVIEW=1: fit + report + preview only;  NOFIT_PLATE=1: leave the plate alone
Camera and lights are fixed and only the knob turns, so the colour of a knob pixel is (to a good approximation) a
function of its surface normal: c = M_mat(n) * ao^g. Fitting M per material (pointer, body, skirt) from the
reference at the reference angle and evaluating it on the rendered normals of every frame gives frames that match the
reference where the geometry matches, with highlights that stay put while the flutes and pointer turn under them.
Needs strips/frames/<style>[_ref]_{normal,id,ao}_####.png (final/render_passes.sh) and <style>_ref_0000.png (colour, for
alignment)."""
import glob, json, os, sys
import numpy as np
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__)); D_ = os.path.join(H, "strips"); FR = os.path.join(D_, "frames")
__file__cs = os.path.join(H, "correct_strips.py")
CS = {"__file__": __file__cs, "__name__": "cs"}
exec(open(__file__cs).read().split("\nfor st in")[0], CS)          # reuse alignment helpers
NB = 40                                                            # matcap bins per axis over n_x, n_y in [-1, 1]


def load(path):
    a = np.asarray(Image.open(path).convert("RGBA"), float) / 255.0
    return a[..., :3], a[..., 3]


def passes(prefix, k):
    n, al = load("%s_normal_%04d.png" % (prefix, k)); n = n * 2 - 1
    i, _ = load("%s_id_%04d.png" % (prefix, k)); ao, _ = load("%s_ao_%04d.png" % (prefix, k))
    i = i / np.maximum(i.sum(-1, keepdims=True), 1e-6)
    return n, i, ao[..., 0], al


def bilerp(Mc, nx, ny):
    """Sample a NBxNBx3 matcap at continuous normals."""
    u = np.clip((nx + 1) / 2 * (NB - 1), 0, NB - 1.001); v = np.clip((ny + 1) / 2 * (NB - 1), 0, NB - 1.001)
    u0, v0 = u.astype(int), v.astype(int); fu, fv = (u - u0)[..., None], (v - v0)[..., None]
    return (Mc[v0, u0] * (1 - fu) * (1 - fv) + Mc[v0, u0 + 1] * fu * (1 - fv) +
            Mc[v0 + 1, u0] * (1 - fu) * fv + Mc[v0 + 1, u0 + 1] * fu * fv)


def splat(nx, ny, val, w):
    """Bilinear splat of samples into the bin grid: returns (sum, weight)."""
    u = np.clip((nx + 1) / 2 * (NB - 1), 0, NB - 1.001); v = np.clip((ny + 1) / 2 * (NB - 1), 0, NB - 1.001)
    u0, v0 = u.astype(int), v.astype(int); fu, fv = u - u0, v - v0
    S = np.zeros((NB * NB, val.shape[-1])); Wt = np.zeros(NB * NB)
    for du, dv, ww in ((0, 0, (1 - fu) * (1 - fv)), (1, 0, fu * (1 - fv)), (0, 1, (1 - fu) * fv), (1, 1, fu * fv)):
        idx = (v0 + dv) * NB + (u0 + du); ww = ww * w
        np.add.at(Wt, idx, ww)
        for c in range(val.shape[-1]): np.add.at(S[:, c], idx, ww * val[:, c])
    return S.reshape(NB, NB, -1), Wt.reshape(NB, NB)


def fill(S, Wt, smooth=0.6):
    """Bin means, lightly smoothed, holes filled by diffusion from their neighbours."""
    if smooth:
        S = np.stack([CS["gblur"](S[..., c], smooth) for c in range(S.shape[-1])], -1); Wt = CS["gblur"](Wt, smooth)
    have = Wt > 0.3; M = np.where(have[..., None], S / np.maximum(Wt, 1e-6)[..., None], 0)
    if not have.any(): return M
    known = have.copy()
    for _ in range(4 * NB):
        if known.all(): break
        acc = np.zeros_like(M); cnt = np.zeros(Wt.shape)
        Mk = np.pad(M * known[..., None], ((1, 1), (1, 1), (0, 0))); Kp = np.pad(known, 1).astype(float)   # no wrap-around
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            acc += Mk[1 - dy:1 - dy + NB, 1 - dx:1 - dx + NB]; cnt += Kp[1 - dy:1 - dy + NB, 1 - dx:1 - dx + NB]
        new = ~known & (cnt > 0); M[new] = acc[new] / cnt[new][:, None]; known |= new
    return M


def view(style, tag, dxy=(0.0, 0.0)):
    """One reference knob: the aligned reference patch, the plate under it and our passes at its pointer angle.
    tag "_ref" = first knob of the style, "_ref2" = second (dxy = its centre offset from the first, UI px)."""
    meta = json.load(open(os.path.join(FR, style + "_ref.json")))
    fs, sc = meta["frame_px"], meta["scale"]; org = [meta["origin_px"][0] + dxy[0], meta["origin_px"][1] + dxy[1]]
    plate = Image.open(os.path.join(D_, "plate_walnut.png")).convert("RGB")
    pc = np.asarray(plate.crop((round(org[0] * sc), round(org[1] * sc), round(org[0] * sc) + fs, round(org[1] * sc) + fs)), float)
    f0 = np.asarray(Image.open(os.path.join(FR, style + tag + "_0000.png")).convert("RGBA"), float); a0 = f0[..., 3:] / 255
    comp = f0[..., :3] * a0 + pc * (1 - a0)
    if style == "space":   # black knob: align on the knob body, ignoring the light pointer (its length differs)
        ref, shift = CS["aligned_ref"](org, fs / sc, fs, comp, sc, M=int(os.environ.get("SPACE_M", 6)), mask=CS["dilate"](a0[..., 0] > 0.5, 3) & ~CS["dilate"](comp.mean(2) > 90, 4))
    else: ref, shift = CS["aligned_ref_cream"](org, fs / sc, fs, comp, sc, own=a0[..., 0] > 0.5)
    n, ids, ao, al = passes(os.path.join(FR, style + tag), 0)
    er = al > 0.99                                                   # trusted: fully covered, away from the silhouette
    for _ in range(int(2 * sc)): er = er & np.roll(er, 1, 0) & np.roll(er, -1, 0) & np.roll(er, 1, 1) & np.roll(er, -1, 1)
    pa = CS["pointer_angle"](ref, al > 0.97, style != "space", 0.6 if style == "dome" else 0.72)
    ref_ptr = CS["DEBUG_PM"].copy() if pa is not None else np.zeros_like(er)
    our_ptr = ids[..., 0] > 0.05
    print("%-8s %-5s ref shift %s  pointer %s" % (style, tag, shift, pa and round(pa, 1)))
    return dict(ref=ref, lin=(ref / 255.0) ** 2.2, pc=pc, comp=comp, a0=a0, f0=f0, n=n, ids=ids, ao=ao, al=al, er=er,
                ref_ptr=ref_ptr, our_ptr=our_ptr, ptr=CS["dilate"](ref_ptr | our_ptr, int(3 * sc)), meta=meta, sc=sc, shift=shift)


def views(style):
    V = [view(style, "_ref")]
    if os.path.exists(os.path.join(FR, style + "_ref2_normal_0000.png")) and not os.environ.get("ONEVIEW"):
        J = json.load(open(os.path.join(D_, "strips.json")))[style]["instances"]
        op = os.path.join(D_, "offsets.json"); o = json.load(open(op)).get(style, [[0, 0], [0, 0]]) if os.path.exists(op) else [[0, 0], [0, 0]]
        # tick-ring centres (strips.json instances carry the measured offsets; take them back out)
        V.append(view(style, "_ref2", ((J[1]["cx"] - o[1][0]) - (J[0]["cx"] - o[0][0]), (J[1]["cy"] - o[1][1]) - (J[0]["cy"] - o[0][1]))))
    return V


def fit(style):
    V = views(style)
    Ms, G = [], []
    for m in range(3):
        cols = {k: [] for k in ("nx", "ny", "a", "c")}
        for v in V:
            sel = v["er"] & (v["ids"][..., m] > 0.97)
            if m == 0: sel &= v["ref_ptr"]                           # pointer colour where both pointers overlap
            else: sel &= ~CS["dilate"](v["ref_ptr"] | v["our_ptr"], int(3 * v["sc"]))
            cols["nx"].append(v["n"][..., 0][sel]); cols["ny"].append(v["n"][..., 1][sel])
            cols["a"].append(np.clip(v["ao"][sel], 0.05, 1)); cols["c"].append(v["lin"][sel])
        nx, ny, a_, c_ = (np.concatenate(cols[k]) for k in ("nx", "ny", "a", "c"))
        if len(nx) < 30: Ms.append(None); G.append(0.0); continue
        g = 0.0
        for it in range(3):                                          # alternate: matcap given g, then g given matcap
            S, Wt = splat(nx, ny, c_ / a_[:, None] ** g, np.ones(len(nx)))
            M = fill(S, Wt, 0.0 if m == 0 else 0.6)
            pred = bilerp(M, nx, ny); x = np.log(a_); y = np.log(np.maximum(c_, 1e-4) / np.maximum(pred, 1e-4)).mean(1)
            g = float(np.clip(g + (x * y).sum() / max((x * x).sum(), 1e-6), 0, 3)) if m > 0 and x.std() > 0.01 else g
        if m == 0: M[:] = np.median(c_, 0)                           # pointer: flat colour (a thin inlay)
        if m == 2: M = np.minimum(M, np.percentile(c_, 80, axis=0))  # knurled skirt: no glints brighter than the ref's skirt
        Ms.append(M); G.append(g)
    print("%-8s views %d  ao gamma %s" % (style, len(V), [round(x, 2) for x in G]))
    v0 = V[0]
    return dict(M=Ms, G=G, V=V, ref=v0["ref"], comp=v0["comp"], pc=v0["pc"], a0=v0["a0"], ptr=v0["ptr"], meta=v0["meta"])


NT, DMAX, DOUT = 180, int(os.environ.get("RIM_DMAX", 48)), 8                                        # rim layer: 2-degree bins, depth -DOUT..DMAX frame px


def _erode(c):
    return c & np.roll(c, 1, 0) & np.roll(c, -1, 0) & np.roll(c, 1, 1) & np.roll(c, -1, 1)


def rimcoords(al, smooth=False):
    """Per pixel: angle bin around the frame centre and signed depth (px) from the knob silhouette: 0 = outermost
    covered pixel (any coverage), positive inward, negative outside. Returns index k = depth + DOUT in [0, DOUT+DMAX]."""
    m = al > 0.02; d = np.full(m.shape, DMAX, int); cur = m.copy()
    for k in range(DMAX):
        nb = _erode(cur)
        if k % 2: nb = nb & np.roll(np.roll(cur, 1, 0), 1, 1) & np.roll(np.roll(cur, -1, 0), -1, 1) & np.roll(np.roll(cur, 1, 0), -1, 1) & np.roll(np.roll(cur, -1, 0), 1, 1)
        d[cur & ~nb] = k; cur = nb
    out = ~m
    for k in range(1, DOUT + 1):
        nb = out & ~_erode(out)                                      # outside pixels touching the current inside
        d[nb] = -k; out = out & ~nb
    d[out] = -DOUT - 1
    if smooth:                                                       # sub-pixel depth: soften the integer rings
        df = CS["gblur"](np.clip(d, -DOUT - 1, DMAX).astype(float), 1.0)
        fr = np.clip(CS["gblur"](al, 0.7), 0, 1)                     # coverage nudges the edge by its fraction
        d = np.where(np.abs(d) <= 2, df + (fr - 0.5) * 0.8, df)
    h, w = m.shape; yy, xx = np.mgrid[0:h, 0:w]
    th = (np.degrees(np.arctan2(xx - (w - 1) / 2, -(yy - (h - 1) / 2))) % 360) / 360 * NT
    return d + DOUT, th.astype(int) % NT


SIGMIN = 0.0   # per style: minimum angular smoothing (bins of 2 deg); set in process()


def rim_table(items):
    """Screen-fixed rim light: median residual (ref - ours, linear) per (angle, signed depth), smoothed around the ring.
    Indexed by distance to the current frame's own silhouette, so it follows the lobes as the knob turns."""
    K = DOUT + DMAX; T = np.zeros((NT, K, 3)); W = np.zeros((NT, K))
    pre = []
    for res, al, excl in items:                                      # several reference views: pool their samples
        kk_, th = rimcoords(al); sel = ~excl & (kk_ >= 0) & (kk_ < K); pre.append((res, kk_, th, sel))
    for t in range(NT):
        sts = [(res, kk_, sel & (np.abs((th - t + NT // 2) % NT - NT // 2) <= 2)) for res, kk_, th, sel in pre]   # +-4 deg
        for k in range(K):
            v = np.concatenate([res[st & (kk_ == k)] for res, kk_, st in sts])
            if len(v) >= 3: T[t, k] = np.median(v, 0); W[t, k] = 1
    for k in range(K):                                               # fill gaps along the ring
        if W[:, k].any():
            idx = np.where(W[:, k] > 0)[0]
            for c in range(3): T[:, k, c] = np.interp(np.arange(NT), np.r_[idx - NT, idx, idx + NT], np.tile(T[idx, k, c], 3))
    T = np.median(np.stack([np.roll(T, s, 0) for s in range(-4, 5)]), 0)  # drop narrow specks (pointer glints, ticks)
    # smooth around the ring: tight at the edge (rim light), wider deeper in, so screen-fixed glare arcs survive but
    # lobe-by-lobe shape mismatch (which would ghost as the knob turns) is averaged away
    for k in range(K):
        dep = k - DOUT; sg = max(SIGMIN, 1.5 if dep < 8 else min(1.5 + (dep - 8) * 0.35, 7.0))
        r = int(3 * sg) + 1; g = np.exp(-np.arange(-r, r + 1) ** 2 / (2 * sg * sg)); g /= g.sum()
        T[:, k] = sum(np.roll(T[:, k], s, 0) * w for s, w in zip(range(-r, r + 1), g))
    fade = np.ones(K); fade[DOUT:] = np.clip((DMAX - np.arange(DMAX)) / 8.0, 0, 1); fade[:DOUT] = np.clip(np.arange(DOUT) / 3.0 + 1 / 3, 0, 1)
    return T * fade[None, :, None]


def rim_apply(T, al, lin, plate_lin):
    """Inside the silhouette add the rim residual; just outside it, paint plate+residual with fading alpha (the strip is
    always drawn over the same plate spot, so baking those plate pixels in is exact)."""
    K = DOUT + DMAX; kk_i, th = rimcoords(al); kf, _ = rimcoords(al, True)
    ok = (kf >= 0) & (kf < K - 1)
    k0 = np.clip(np.floor(kf), 0, K - 2).astype(int); f = np.clip(kf - k0, 0, 1)[..., None]
    add = (T[th, k0] * (1 - f) + T[th, k0 + 1] * f) * ok[..., None]
    kk_ = kk_i; inside = (kk_ >= DOUT)[..., None]
    rgb = np.where(inside, np.clip(lin + add, 0, 1), np.clip(plate_lin + add, 0, 1))
    a_out = np.where(ok & (kk_ < DOUT), np.clip((kk_ + 1) / 3.0, 0, 1) * (np.abs(add).max(-1) > 0.002), 0)
    # partly covered silhouette pixels: bake the plate (+ outer residual) behind them so the edge stays smooth
    edge = inside[..., 0] & ok & (al < 0.999)
    rgb = np.where(edge[..., None], al[..., None] * rgb + (1 - al[..., None]) * np.clip(plate_lin + add, 0, 1), rgb)
    return rgb, np.where(edge, 1.0, np.where(inside[..., 0], al, a_out))


def shade(F, n, ids, ao, al, fallback_rgb=None):
    out = np.zeros(n.shape)
    for m in range(3):
        M = F["M"][m]
        if M is None:
            c = fallback_rgb if fallback_rgb is not None else np.zeros(n.shape)
            if m == 2 and F["M"][1] is not None: c = bilerp(F["M"][1], n[..., 0], n[..., 1]) * np.clip(ao, .05, 1)[..., None] ** F["G"][1]
        else: c = bilerp(M, n[..., 0], n[..., 1]) * np.clip(ao, 0.05, 1)[..., None] ** F["G"][m]
        out += ids[..., m:m + 1] * c
    out = np.clip(out, 0, 1); alpha = al
    if F.get("R") is not None and not os.environ.get("NORIM"):
        rgb, alpha = rim_apply(F["R"], al, out, (F["pc"] / 255.0) ** 2.2)
        p = ids[..., :1] * (al[..., None] > 0.5)
        out = rgb * (1 - p) + out * p
    F["alpha"] = alpha
    return out ** (1 / 2.2) * 255


def pointer_matcap(F, D):
    """The pointer is part of the knob, under the same light: shade it with the lit sphere of the matching material from
    another knob (cream pointer <- cream FEEL body, dark pointer <- glossy black SPACE body), scaled per channel so its
    median matches the reference pointer. Highlights then move across the rounded bar as the knob turns."""
    Md = D["M"][1]; pr, tg = [], []
    for v in F["V"]:
        sel = (v["ids"][..., 0] > 0.97) & v["ref_ptr"] & (v["al"] > 0.99)
        if sel.sum() < 10: continue
        pr.append(bilerp(Md, v["n"][..., 0][sel], v["n"][..., 1][sel]) * np.clip(v["ao"][sel], .05, 1)[:, None] ** D["G"][1]); tg.append(v["lin"][sel])
    if not pr: return F["M"][0], F["G"][0]
    # one brightness scale + the reference pointer's own tint (per-channel scales blew the highlights out to white)
    W = np.array([0.2126, 0.7152, 0.0722]); tgm = np.median(np.concatenate(tg), 0)
    lt, lp = np.concatenate(tg) @ W, np.concatenate(pr) @ W   # match the bright top of the bar, not the dark sides
    kl = np.percentile(lt, 85) / max(np.percentile(lp, 85), 1e-4); tint = tgm / max(tgm @ W, 1e-4)
    print("pointer shaded from the other material, scale %.2f tint %s" % (kl, np.round(tint, 2)))
    return (Md @ W)[..., None] * kl * tint[None, None, :], D["G"][1]


def process(style, F=None):
    F = F or fit(style)
    v0 = F["V"][0]; n, ids, ao, al = v0["n"], v0["ids"], v0["ao"], v0["al"]; f0 = v0["f0"]
    F["R"] = None
    rim_on = not os.environ.get("NORIM") and style not in os.environ.get("NORIM_STYLES", "").split(",") and not F.get("notrim_rim")
    if rim_on:   # 2D edge layer copied from the reference: off for SPACE (it made a hard, cut-out looking edge)
        items = []
        for v in F["V"]:
            F["pc"] = v["pc"]; r_ = shade(F, v["n"], v["ids"], v["ao"], v["al"], v["f0"][..., :3] / 255.0)
            c0 = r_ * v["al"][..., None] + v["pc"] * (1 - v["al"][..., None])
            tick = (v["ref"].mean(2) - v["pc"].mean(2) > 35) & (v["al"] < 0.5)   # printed ticks on the ref plate: not edge shading
            tick = CS["dilate"](tick, 5)   # ...and their drop shadows
            items.append(((v["ref"] / 255.0) ** 2.2 - (c0 / 255.0) ** 2.2, v["al"], v["ptr"] | tick))
        F["pc"] = v0["pc"]
        global SIGMIN; SIGMIN = {"space": 3.0, "scallop": 3.0}.get(style, 0.0)   # wash out lobe-shaped leftovers
        F["R"] = rim_table(items)
    for v in F["V"][1:]:                                             # report the second view too
        F["pc"] = v["pc"]; r_ = shade(F, v["n"], v["ids"], v["ao"], v["al"], v["f0"][..., :3] / 255.0); A_ = F["alpha"][..., None]
        b_ = (v["al"] > 0.97) & ~v["ptr"]
        print("%-8s mean|err| knob body, 2nd view: %.1f" % (style, np.abs(r_ * A_ + v["pc"] * (1 - A_) - v["ref"])[b_].mean()))
    F["pc"] = v0["pc"]
    rgb = shade(F, n, ids, ao, al, f0[..., :3] / 255.0)
    a0 = F["alpha"][..., None]
    fixed0 = rgb * a0 + F["pc"] * (1 - a0)
    body = (al > 0.97) & ~F["ptr"]
    e0 = np.abs(F["comp"] - F["ref"])[body].mean(); e1 = np.abs(fixed0 - F["ref"])[body].mean()
    print("%-8s mean|err| knob body at ref angle: rendered %.1f  matcap %.1f (0-255)" % (style, e0, e1))
    S = 300
    tiles = [F["ref"], F["comp"], fixed0, np.abs(fixed0 - F["ref"]) * 3]
    Mviz = [np.clip(M, 0, 1) ** (1 / 2.2) * 255 if M is not None else np.zeros((NB, NB, 3)) for M in F["M"][1:]]
    row = np.concatenate([np.asarray(Image.fromarray(np.clip(x, 0, 255).astype(np.uint8)).resize((S, S), Image.LANCZOS)) for x in tiles + Mviz], 1)
    Image.fromarray(row).save(os.path.join(H, "knobs", "_matcap_%s.png" % style))
    tl = []
    for v in F["V"]:                                                 # ref | ours for every reference view
        F["pc"] = v["pc"]; r_ = shade(F, v["n"], v["ids"], v["ao"], v["al"], v["f0"][..., :3] / 255.0); A_ = F["alpha"][..., None]
        for x in (v["ref"], r_ * A_ + v["pc"] * (1 - A_)):
            tl.append(np.asarray(Image.fromarray(np.clip(x, 0, 255).astype(np.uint8)).resize((360, 360), Image.LANCZOS)))
    F["pc"] = F["V"][0]["pc"]
    Image.fromarray(np.concatenate(tl, 1)).save(os.path.join(H, "knobs", "_views_%s.png" % style))
    if os.environ.get("PREVIEW"): return
    os.makedirs(os.path.join(D_, "frames_fixed"), exist_ok=True)
    ks = sorted(int(p[-8:-4]) for p in glob.glob(os.path.join(FR, style + "_normal_[0-9][0-9][0-9][0-9].png")))
    for k in ks:
        n, ids, ao, al = passes(os.path.join(FR, style), k)
        src = os.path.join(FR, "%s_%04d.png" % (style, k))
        fb = np.asarray(Image.open(src).convert("RGB"), float) / 255 if os.path.exists(src) else None
        rgb = shade(F, n, ids, ao, al, fb)
        Image.fromarray(np.dstack([rgb, F["alpha"] * 255]).astype(np.uint8), "RGBA").save(os.path.join(D_, "frames_fixed", "%s_%04d.png" % (style, k)))
    open(os.path.join(D_, "frames_fixed", style + ".json"), "w").write(open(os.path.join(FR, style + "_normal.json")).read())
    print("%-8s wrote %d frames" % (style, len(ks)))


if __name__ == "__main__":
    styles = sys.argv[1:] or ["dome", "scallop", "space"]
    Fs = {st: fit(st) for st in sorted((set(styles) - {"trim"}) | {"scallop", "space", "dome"} if "trim" in styles else set(styles) | {"scallop", "space"})}
    # where each reference knob really sits relative to its tick-ring centre (UI px): the plugin draws the strip there
    op = os.path.join(D_, "offsets.json"); off = json.load(open(op)) if os.path.exists(op) else {}
    for st in styles:
        if st != "trim": off[st] = [[v["shift"][0] / v["sc"], v["shift"][1] / v["sc"]] for v in Fs[st]["V"]]
    json.dump(off, open(op, "w"), indent=1)
    if "trim" in styles:   # TRIM: same cream bakelite as the VOICE domes, shaded with their lit sphere, no edge layer
        Ft = dict(Fs["dome"]); Ft["M"] = list(Fs["dome"]["M"]); Ft["G"] = list(Fs["dome"]["G"]); Ft["notrim_rim"] = True
        Ft["M"][0], Ft["G"][0] = pointer_matcap(Ft, Fs["space"]); process("trim", Ft)
    for st in [s_ for s_ in styles if s_ != "trim"]:
        donor = "scallop" if st == "space" else "space"
        Fs[st]["M"][0], Fs[st]["G"][0] = pointer_matcap(Fs[st], Fs[donor])
        process(st, Fs[st])
