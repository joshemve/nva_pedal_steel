"""Workshop recipe -> blender/knob_studio.py config, so a finished recipe renders in Cycles exactly as tweaked.

python recipe_to_studio.py feel [base_config.json] > out.json
Starts from the base config (default final/knobs/native_v2.json) and replaces the section for the recipe's style.
A recipe knob is two stacked parts (Base, Top); knob_studio_geometry.build() lathes each from geometry["parts"].
base_profile()/top_profile() mirror baseProfile()/topProfile() in app.js. The room light (ambient) and exposure are
global in knob_studio.py, so they are set for the whole render.
"""
import json, math, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SHARP = {"scoop": -2, "scallop": -3, "ridge": -1, "diamond": -4}   # grip type -> knob_studio_geometry sharp code
BODY = {"scallop": "cream_knob", "dome": "cream_knob", "space": "black_knob"}
PTR = {"scallop": "ptr_dark", "dome": "ptr_dark", "space": "ptr_light"}
ARC, WALL = 16, 8


def clamp(v, a, b):
    return min(b, max(a, v))


def ease(f):
    return f * f * (3 - 2 * f)


def fall(u):
    return 1 - u * u * u * (10 - 15 * u + 6 * u * u)


def blend(P0, C, P2, u):
    """Quintic Bezier from the wall into the shoulder with zero curvature at both ends (mirror of blend() in app.js)."""
    mix = lambda A, B, t: (A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t)
    Q = [P0, mix(P0, C, 0.5), mix(P0, C, 0.94), mix(P2, C, 0.94), mix(P2, C, 0.5), P2]
    k = [c * u ** i * (1 - u) ** (5 - i) for i, c in enumerate((1, 5, 10, 10, 5, 1))]
    return sum(k[i] * Q[i][0] for i in range(6)), sum(k[i] * Q[i][1] for i in range(6))


def wall_and_shoulder(rt, zs, sr, sz, wt):
    """Mirror of wallAndShoulder() in app.js (without the live-view contact shading)."""
    zw = zs - sz; rw = 1 + (rt - 1) * zw / zs
    pts = [[1, 0, 1]]
    pts += [[round(1 + (rw - 1) * j / WALL, 5), round(j / WALL * zw, 5), round(1 - (1 - wt) * ease(j / WALL), 5)] for j in range(1, WALL)]
    for i in range(ARC + 1):
        u = i / ARC; r, z = blend((rw, zw), (rt, zs), (rt - sr, zs), u)
        pts.append([round(r, 5), round(z, 5), round(wt * fall(u), 5)])
    return pts


def base_profile(p):
    rt, wt = 1 - clamp(p.get("taper", 0), 0, 0.8), 1 - clamp(p["grip"].get("fade", 0), 0, 1)
    br, bz = clamp(p["bevel"] / p["width"], 0.004, rt * 0.95), clamp(p["bevel"] / p["height"], 0.004, 0.95)
    return wall_and_shoulder(rt, 1, br, bz, wt) + [[0, 1, 0]]


def top_profile(p):
    c = clamp(p["dome"] / p["height"], -0.5, 0.6); zs = 1 - max(c, 0)
    rt, wt = 1 - clamp(p.get("taper", 0), 0, 0.8), 1 - clamp(p["grip"].get("fade", 0), 0, 1)
    sr, sz = clamp(p["round"] / p["width"], 0.004, rt * 0.95), clamp(p["round"] / p["height"], 0.004, zs * 0.95)
    pts = wall_and_shoulder(rt, zs, sr, sz, wt); rf = rt - sr
    for k in range(1, 9):
        r = rf * (1 - k / 8)
        pts.append([round(r, 5), round(zs + c * (1 - (r / rf) ** 2), 5), 0])
    return pts


def part(name, p, prof):
    g = p["grip"]
    return {"name": name, "prof": prof, "width": p["width"], "height": p["height"],
            "lobes": 0 if g["style"] == "smooth" else g.get("count", 0), "depth": g.get("depth", 0),
            "sharp": SHARP.get(g["style"], g.get("sharpness", 1)), "phase": g.get("phase", 0), "land": g.get("land", 0.1)}


def convert(recipe, base):
    P, p, m, l, v = (recipe[k] for k in ("parts", "pointer", "material", "light", "view"))
    style = recipe["style"]
    parts = ([part("Base", P["base"], base_profile(P["base"]))] if P["base"]["on"] else []) + [part("Top", P["top"], top_profile(P["top"]))]
    cfg = dict(base)
    cfg["ambient"] = l["ambient"]; cfg["exposure"] = l["exposure"]
    cfg[style] = {
        "rscale": v["size"], "y": v.get("offset_y", 0),
        "geometry": {"parts": parts, "tilt": v["tilt"], "r0": p["start"], "r1": p["end"], "pw": p["width"]},
        "key": {"power": l["key_power"], "pos": l["key_pos"], "size": l["key_size"]},
        "rim": {"power": l["rim_power"], "pos": l["rim_pos"], "size": l["rim_size"]},
        "materials": {
            BODY[style]: {"Base Color": [*m["color"], 1], "Roughness": m["roughness"],
                          "Coat Weight": m["coat"], "Coat Roughness": m["coat_roughness"]},
            PTR[style]: {"Base Color": [*p["color"], 1], "Roughness": p["roughness"]},
        },
    }
    return cfg


if __name__ == "__main__":
    recipe = json.loads((HERE / "recipes" / (sys.argv[1] + ".json")).read_text())
    base = Path(sys.argv[2]) if len(sys.argv) > 2 else HERE.parent / "final" / "knobs" / "native_v2.json"
    print(json.dumps(convert(recipe, json.loads(base.read_text())), indent=1))
