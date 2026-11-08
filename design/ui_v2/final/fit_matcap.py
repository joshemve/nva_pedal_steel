"""Fit a knob's lathe profile to APPROVED_main.png through the reference-fitted lit-sphere shading.
python final/fit_matcap.py <style> [rounds]
For each candidate geometry: render normal + id passes at the reference angle (Blender), fit the matcap on it, and
score the composite (knob over plate) against the reference inside a disk of 1.08 R. Lower = better. Lighting and
material are absorbed by the matcap, so the score measures shape (silhouette, crown, flutes).
Cache: final/knobs/mfit/<style>.json. The best profile is printed as a KNOB_<STYLE> json."""
import json, os, subprocess, sys, hashlib, threading
LOCK = threading.Lock()
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from PIL import Image

VARIANT = sys.argv[1]; STYLE = VARIANT.rstrip("0123456789"); ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 2
H = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(H)
OUTD = os.path.join(H, "knobs", "mfit"); os.makedirs(OUTD, exist_ok=True)
CACHE = os.path.join(OUTD, VARIANT + ".json"); cache = json.load(open(CACHE)) if os.path.exists(CACHE) else {}
BL = r"C:/Program Files/Blender Foundation/Blender 5.0/blender.exe"
REFANG = {"dome": 35, "scallop": 30, "space": -100}
sys.argv = [sys.argv[0]]
MS = {"__file__": os.path.join(H, "matcap_strips.py"), "__name__": "ms"}
exec(open(MS["__file__"]).read(), MS)

# reference patch with the alignment fixed from the current colour render
_F = MS["fit"](STYLE); REFP = _F["ref"]; PLATE = _F["pc"]; META = _F["meta"]
fs = META["frame_px"]; yy, xx = np.mgrid[0:fs, 0:fs]; RR = np.hypot(yy - (fs - 1) / 2, xx - (fs - 1) / 2)
RPX = {"dome": 29.0, "scallop": 44.4, "space": 38.0}[STYLE] * META["scale"]
DISK = RR < 1.08 * RPX * (1.18 if STYLE == "dome" else 1.0) if VARIANT not in ("space2", "space3", "space4") else RR < 1.2 * RPX   # space2: include the flared base
PTRMASK = _F["ptr"]
MS["CS"] = MS.get("CS")
if VARIANT in ("space3", "space4"):
    _g = MS["CS"]["gblur"](REFP.mean(2), 1.0); _gy, _gx = np.gradient(_g); GREF = np.hypot(_gx, _gy)


def profile(p):
    """Superellipse lathe: z = z0 + (1-z0) (1 - r^pw)^(1/pw); flute weight fades from wb at the base to 1 on the wall
    and to 0 at r = rf on the top."""
    z0, pw, rf, wb, fw, fl = p["z0"], p["pw"], p["rf"], p.get("wb", 0.5), p.get("fw", 0.12), p.get("fl", 0.0)
    if fl: return [tuple(x) for x in _sprof(z0, pw, rf, wb, fw, fl)]
    pts = [(1.0, 0.0, wb)]
    rs = [1.0, 0.9995, 0.998, 0.995, 0.99, 0.98, 0.965, 0.945, 0.92, 0.89, 0.85, 0.8, 0.74, 0.67, 0.58, 0.47, 0.34, 0.2, 0.0]
    for r in rs:
        z = z0 + (1 - z0) * max(0.0, 1 - r ** pw) ** (1 / pw)
        t = np.clip((r - rf) / max(fw, 1e-3), 0, 1); w = t * t * (3 - 2 * t)
        if r == 1.0: z = z0 * 0.5
        pts.append((r, round(float(z), 5), round(float(w), 4)))
    return pts

def _sprof(z0, pw, rf, wb, fw, fl):
    src = open(os.path.join(ROOT, "blender", "knob_lib.py"), encoding="utf-8").read()
    i = src.index("def sprofile("); j = src.index(chr(10) * 3, i); ns = {"math": __import__("math")}; exec(src[i:j], ns)
    return ns["sprofile"](z0, pw, rf, wb, fw, fl=fl)


def profile2(rface, crown, e, z0, rf, fw=0.12, wb=0.5, **_):
    """Face + shoulder: domed top face out to rface (crown = its rise), then a shoulder z = z0 + (zs-z0)(1-u)^e down to
    the base wall (u = 0 at the face edge, 1 at R). e < 1 gives a defined face edge and a steep lower wall."""
    zs = 1 - crown; pts = [(1.0, 0.0, wb)]
    rs = [1.0, 0.9995, 0.998, 0.995, 0.99] + [rface + (0.985 - rface) * (1 - i / 9) for i in range(10)] +          [rface * (1 - i / 7) for i in range(1, 8)]
    for r in sorted(set(round(x, 5) for x in rs), reverse=True):
        if r >= rface: u = (r - rface) / (1 - rface); z = z0 + (zs - z0) * max(0.0, 1 - u) ** e
        else: z = zs + crown * (1 - (r / rface) ** 2)
        if r == 1.0: z = z0 * 0.5
        t = min(1.0, max(0.0, (r - rf) / max(fw, 1e-3))); w = t * t * (3 - 2 * t)
        pts.append((r, round(z, 5), round(w, 4)))
    return pts


def env_for(p):
    e = dict(os.environ)
    q = {k: v for k, v in p.items() if k in ("hr", "lobes", "depth", "sharp", "phase", "tilt", "LAND")}
    if "z0" in p: q["prof"] = profile2(**p) if "rface" in p else profile(p)
    e["KNOB_" + STYLE.upper()] = json.dumps(q); e["KNOB_RSCALE_" + STYLE.upper()] = str(p.get("rscale", 1.0))
    return e


def render(p):
    key = json.dumps(p, sort_keys=True); tag = hashlib.md5(key.encode()).hexdigest()[:10]
    pre = os.path.join(OUTD, STYLE + "_" + tag)
    procs = []
    for ps in ("normal", "id"):
        if os.path.exists(pre + "_%s_0000.png" % ps): continue
        e = env_for(p); e["KNOB_PASS"] = ps; e["STRIP"] = STYLE + ":1"; e["STRIP_ANGLE"] = str(REFANG[STYLE]); e["KNOB_PASS_SAMPLES"] = "16"
        procs.append(subprocess.Popen([BL, "-b", "-P", "blender/final_ui.py", "--", pre + "_" + ps, "4", "2.0"], cwd=ROOT, env=e,
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    for pr in procs:
        try: pr.wait(timeout=240)
        except subprocess.TimeoutExpired: pr.kill(); print("TIMEOUT", key, flush=True)
    return pre


def score(p):
    key = json.dumps(p, sort_keys=True)
    if key in cache: return cache[key]
    pre = render(p)
    try:
        n, al = MS["load"](pre + "_normal_0000.png"); n = n * 2 - 1
        ids, _ = MS["load"](pre + "_id_0000.png"); ids = ids / np.maximum(ids.sum(-1, keepdims=True), 1e-6)
    except Exception as ex:
        print("FAILED", key, ex); return 1e9
    if n.shape[0] != fs: print("SIZE", key, n.shape); return 1e9
    lin = (REFP / 255.0) ** 2.2
    er = al > 0.99
    for _ in range(4): er = er & np.roll(er, 1, 0) & np.roll(er, -1, 0) & np.roll(er, 1, 1) & np.roll(er, -1, 1)
    sel = er & (ids[..., 1] > 0.97) & ~PTRMASK
    S, Wt = MS["splat"](n[..., 0][sel], n[..., 1][sel], lin[sel], np.ones(int(sel.sum())))
    M = MS["fill"](S, Wt, 0.6)
    body = np.clip(MS["bilerp"](M, n[..., 0], n[..., 1]), 0, 1) ** (1 / 2.2) * 255
    ptrc = np.median(REFP[PTRMASK & (ids[..., 0] > 0.9)], 0) if (PTRMASK & (ids[..., 0] > 0.9)).sum() > 10 else body
    rgb = body * ids[..., 1:2] + ids[..., :1] * ptrc
    sk = er & (ids[..., 2] > 0.97)
    if sk.sum() > 30:   # knurled skirt (dome): its own lit sphere
        S2, W2 = MS["splat"](n[..., 0][sk], n[..., 1][sk], lin[sk], np.ones(int(sk.sum())))
        rgb = rgb + ids[..., 2:3] * np.clip(MS["bilerp"](MS["fill"](S2, W2, 0.6), n[..., 0], n[..., 1]), 0, 1) ** (1 / 2.2) * 255
    else: rgb = rgb + ids[..., 2:3] * body
    comp = rgb * al[..., None] + PLATE * (1 - al[..., None])
    m = DISK & ~PTRMASK
    s = float(np.abs(comp - REFP)[m].mean())
    if VARIANT in ("space3", "space4"):   # black on dark: the outline barely moves a colour score, so also compare edge maps
        def gm(a):
            g = MS["CS"]["gblur"](a.mean(2), 1.0); gy, gx = np.gradient(g); return np.hypot(gx, gy)
        s += 2.0 * float(np.abs(gm(comp) - GREF)[m].mean())
    with LOCK: cache[key] = s; json.dump(cache, open(CACHE, "w"), indent=0)
    Image.fromarray(np.concatenate([REFP, comp], 1).clip(0, 255).astype(np.uint8)).save(pre + "_cmp.png")
    print("  %-110s %.2f" % (key, s), flush=True)
    return s


SPACES = {
    "scallop": {"rf": [0.7, 0.75, 0.8, 0.85, 0.9], "depth": [0.03, 0.04, 0.05, 0.06, 0.07], "z0": [0.05, 0.1, 0.2, 0.3],
                "pw": [1.8, 2.2, 2.5, 2.8, 3.2], "hr": [0.4, 0.5, 0.65, 0.8], "phase": [0.6875, 0.75, 0.8125],
                "rscale": [0.97, 0.985, 1.0, 1.015, 1.03], "fw": [0.06, 0.12, 0.25], "wb": [0.2, 0.5, 1.0],
                "sharp": [-2, 1.0, 2.0], "lobes": [8, 9, 10]},
    "scallop2": {"rface": [0.6, 0.66, 0.72, 0.78], "crown": [0.0, 0.04, 0.08, 0.14], "e": [0.5, 0.7, 1.0, 1.4],
                 "z0": [0.1, 0.25, 0.4], "rf": [0.5, 0.6, 0.7, 0.8], "depth": [0.05, 0.07, 0.09, 0.12], "hr": [0.5, 0.65, 0.8],
                 "tilt": [0.0, 6.0, 12.0], "phase": [0.6875, 0.75, 0.8125], "rscale": [0.985, 1.0, 1.015], "sharp": [-2, 1.0, 2.0]},
    "scallop3": {"rf": [0.86, 0.9, 0.94, 0.98, 1.02], "fw": [0.2, 0.25, 0.32, 0.4, 0.5], "tilt": [0.0, 3.0, 6.0, 10.0],
                 "z0": [0.0, 0.05, 0.1], "depth": [0.05, 0.06, 0.07], "pw": [2.0, 2.2, 2.4], "wb": [0.0, 0.2, 0.4],
                 "rscale": [0.975, 0.985, 0.995], "phase": [0.65, 0.6875, 0.725]},
    "dome2": {"z0": [0.6, 0.65, 0.72, 0.8, 0.88], "pw": [1.6, 1.8, 2.0, 2.3], "hr": [0.5, 0.6, 0.7, 0.85],
              "rscale": [0.97, 0.985, 1.0, 1.015], "tilt": [0.0, 8.0, 18.0, 25.0]},
    "space": {"z0": [0.2, 0.35, 0.5, 0.65], "pw": [1.6, 2.0, 2.5, 3.2, 4.0], "rf": [0.3, 0.5, 0.7, 0.85],
              "depth": [0.04, 0.055, 0.065, 0.08, 0.1], "hr": [0.5, 0.65, 0.8, 1.0], "rscale": [0.97, 0.985, 1.0, 1.015, 1.03],
              "sharp": [1.0, 1.8, 3.0], "phase": [0.0, 0.25, 0.5, 0.75], "fw": [0.06, 0.15, 0.3]},
    "space2": {"fl": [0.03, 0.06, 0.09, 0.12], "wb": [0.5, 1.0, 1.5, 2.0], "rscale": [0.96, 0.975, 0.99, 1.0, 1.015],
               "depth": [0.03, 0.04, 0.055, 0.07], "sharp": [0.6, 1.0, 1.8], "phase": [0.2, 0.25, 0.3], "z0": [0.1, 0.2, 0.3],
               "hr": [0.65, 0.8, 1.0]},
    "space3": {"depth": [0.05, 0.07, 0.09, 0.12, 0.15], "sharp": [-2, 0.6, 1.0, 1.8], "phase": [0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875],
               "rscale": [0.97, 0.985, 1.0, 1.015, 1.03, 1.05], "rf": [0.7, 0.8, 0.85, 0.9], "fw": [0.1, 0.15, 0.25],
               "wb": [0.5, 1.0, 1.5], "z0": [0.1, 0.2, 0.3], "pw": [1.6, 2.0, 2.5], "hr": [0.65, 0.8, 1.0], "lobes": [7, 8, 9]},
    "dome": {"z0": [0.3, 0.45, 0.55, 0.65], "pw": [1.6, 2.0, 2.5, 3.2, 4.0], "hr": [0.45, 0.6, 0.75, 0.9],
             "rscale": [0.96, 0.975, 0.985, 1.0]},
}
START = {"scallop": dict(z0=0.2, pw=2.5, rf=0.8, depth=0.05, hr=0.65, phase=0.8125, rscale=1.0, fw=0.12, wb=0.5, sharp=1.0, lobes=9),
         "scallop2": dict(rface=0.72, crown=0.06, e=0.7, z0=0.25, rf=0.6, depth=0.09, hr=0.65, tilt=6.0, phase=0.75, rscale=1.0,
                          sharp=1.0, lobes=9, fw=0.12, wb=0.5),
         "scallop3": dict(z0=0.05, pw=2.2, rf=0.9, depth=0.06, hr=0.65, phase=0.6875, rscale=0.985, fw=0.25, wb=0.2, sharp=-2,
                          lobes=9, tilt=6.0),
         "dome2": dict(z0=0.65, pw=2.0, hr=0.6, rscale=0.985, lobes=0, depth=0.0, rf=0.0, tilt=18.0),
         "space": dict(z0=0.45, pw=2.5, rf=0.5, depth=0.065, hr=0.8, rscale=1.0, sharp=1.8, phase=0.0, fw=0.15, lobes=8),
         "space2": dict(z0=0.2, pw=2.0, rf=0.85, fw=0.15, depth=0.04, hr=0.8, phase=0.25, sharp=1.0, lobes=8, rscale=1.015, wb=0.5, fl=0.0),
         "space3": dict(z0=0.2, pw=2.0, rf=0.85, fw=0.15, depth=0.04, hr=0.8, phase=0.25, sharp=1.0, lobes=8, rscale=1.015, wb=0.5),
         "dome": dict(z0=0.55, pw=2.5, hr=0.6, rscale=0.985, lobes=0, depth=0.0, rf=0.0)}[VARIANT]
if os.environ.get("START"): START.update(json.loads(os.environ["START"]))

best = dict(START); best_s = score(best)
pool = ThreadPoolExecutor(int(os.environ.get("PAR", "3")))
for r in range(ROUNDS):
    improved = False
    for name, vals in SPACES[VARIANT].items():
        cands = [dict(best, **{name: v}) for v in vals if v != best.get(name)]
        for c, s in zip(cands, pool.map(score, cands)):
            if s < best_s - 1e-3: best, best_s, improved = c, s, True
        print("round %d after %s: best %.2f %s" % (r, name, best_s, json.dumps(best)), flush=True)
    if not improved: break
print("BEST", best_s, json.dumps(best))
json.dump(dict(best=best, score=best_s, prof=(profile2(**best) if "rface" in best else profile(best)) if "z0" in best else None), open(os.path.join(OUTD, VARIANT + "_best.json"), "w"), indent=1)
