"""Deck knobs matching APPROVED_main.png (round26/x6_1_sp_c_2).
  dome    : VOICE  - cream bakelite dome on a fine-knurled black skirt, dark pointer line
  scallop : FEEL   - tall cream fluted knob (9 concave flutes, creased ridges), large domed top face, dark pointer
  space   : SPACE  - black scalloped knob (8 soft lobes), satin domed cap, long cream pointer line
build(coll, M, R, style) returns an empty at the panel face (axis +Z towards the viewer) with a child
'Spin' empty that carries the rotating parts; spin(root, ang) turns it (ang = degrees clockwise from
12 o'clock, -135..135). Uses M["cream_knob"], M["black_knob"], M["knob_skirt"], M["ptr_dark"], M["ptr_light"]."""
import bpy, bmesh, math

# lathe profiles: (radius fraction of R, height fraction of H, lobe weight)
DOME = [(1.0, 0.0, 1), (1.0, 0.50, 1), (0.985, 0.66, 1), (0.95, 0.78, 1), (0.88, 0.88, 1),
        (0.76, 0.95, 1), (0.58, 1.0, 1), (0.36, 1.03, 1), (0.16, 1.045, 1), (0.0, 1.05, 1)]
SCALLOP = [(1.0, 0.0, 1.0), (1.0, 0.42, 1.0), (0.985, 0.60, 1.0), (0.95, 0.74, 0.95), (0.89, 0.85, 0.85),
           (0.79, 0.93, 0.65), (0.64, 0.99, 0.4), (0.45, 1.03, 0.18), (0.24, 1.05, 0.05), (0.0, 1.06, 0.0)]
# FEEL (pixel analysis of APPROVED_main GLIDE/SWELL, 2026-11-06): a tall cylinder whose wall is cut by deep concave
# flutes with creased ridges; the flutes run up through a small rounded shoulder and bite into the rim of a large,
# gently domed top face (~0.8 R). Seen tilted, so ~0.4 R of the fluted wall shows under the face.
# seen nearly head-on: a sloped fluted shoulder from R in to a crisp-edged flat face at ~0.75 R
FLUTED = [(1.0, 0.0, 0.45), (1.0, 0.30, 0.5), (0.988, 0.46, 0.65), (0.96, 0.60, 0.85), (0.925, 0.72, 1.0),
          (0.885, 0.81, 1.0), (0.845, 0.875, 1.0), (0.805, 0.925, 0.85), (0.765, 0.955, 0.55), (0.72, 0.973, 0.25),
          (0.66, 0.984, 0.07), (0.55, 0.991, 0.0), (0.35, 0.997, 0.0), (0.0, 1.0, 0.0)]
SKIRT = [(1.0, 0.0, 1), (1.0, 0.62, 1), (0.97, 0.86, 1), (0.9, 1.0, 1), (0.0, 1.0, 0)]

STYLES = {
    #          profile  H/R   lobes depth sharp  body mat      ptr mat      ptr r0..r1  ptr w  skirt  tilt
    "dome":    (DOME,    0.60, 0,    0.0,  1.0,  "cream_knob", "ptr_dark",  0.24, 0.88, 0.13, True,  18.0),
    "scallop": (FLUTED,  0.65, 9,    0.09, 1.0,  "cream_knob", "ptr_dark",  0.34, 0.86, 0.14, False, 6.0),
    "space":   (SCALLOP, 0.80, 8,    0.065, 1.8, "black_knob", "ptr_light", 0.06, 0.92, 0.11, False, 6.0),
}
LAND = 0.10   # fraction of each flute pitch left as a flat ridge land (crisp but not spiky)
PHASE = {"scallop": 0.75}   # flute phase (fraction of a flute pitch) fitted to the reference
CREASE = {}   # degrees: edges sharper than this render as hard creases
KEYS = ("prof", "hr", "lobes", "depth", "sharp", "body", "ptr", "r0", "r1", "pw", "skirt", "tilt")


def params(style):
    import os, json
    p = dict(zip(KEYS, STYLES[style])); p["crease"] = CREASE.get(style); p["phase"] = PHASE.get(style, 0.0)
    p.update(json.loads(os.environ.get("KNOB_" + style.upper(), "{}")))
    if isinstance(p["prof"], str): p["prof"] = globals()[p["prof"]]
    if "LAND" in p: globals()["LAND"] = p["LAND"]
    return p


def lathe(name, prof, R, H, lobes=0, depth=0.0, sharp=1.0, n=None, z0=0.0, crease=None, phase=0.0):
    n = n or max(128, lobes * 48)
    bm = bmesh.new(); uv = bm.loops.layers.uv.new()
    rings = []
    for rf, zf, w in prof:
        if rf == 0.0:
            rings.append(bm.verts.new((0, 0, z0 + zf * H))); continue
        ring = []
        for i in range(n):
            t = 2 * math.pi * i / n
            tl = t + phase * 2 * math.pi / max(1, lobes)   # flute phase relative to the pointer (+X)
            r = R * rf
            if lobes and sharp == -2 and w > 0:   # concave cylindrical scoop: depth*(1-u^2), u=-1..1 across a flute, crease at u=+-1
                u = min(1.0, abs(((lobes * tl / (2 * math.pi)) % 1.0) * 2 - 1) / (1 - LAND))   # flat land on each ridge
                r *= 1 - depth * w * (1 - u * u)
            elif lobes and sharp < 0: r *= 1 - depth * w * abs(math.sin(lobes * t / 2))
            elif lobes: r *= 1 - depth * w * ((1 - math.cos(lobes * tl)) / 2) ** sharp
            ring.append(bm.verts.new((r * math.cos(t), r * math.sin(t), z0 + zf * H)))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            vs = (a[i], a[j], b) if not isinstance(b, list) else (a[i], a[j], b[j], b[i])
            f = bm.faces.new(vs)
            for l in f.loops:   # planar top projection (bakelite texture)
                l[uv].uv = (0.5 + l.vert.co.x / (2.4 * R), 0.5 + l.vert.co.y / (2.4 * R))
    # bottom cap
    f = bm.faces.new(list(reversed(rings[0])))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = True
    if crease: me.set_sharp_from_angle(angle=math.radians(crease))   # hard bakelite edges stay crisp
    return me


def surface_z(prof, R, H, r):
    """Height of the (lobe-free) profile at radius r, for laying the pointer on the cap."""
    pts = sorted(((rf * R, zf * H) for rf, zf, _ in prof if zf > 0.3), key=lambda p: p[0])
    for (ra, za), (rb, zb) in zip(pts, pts[1:]):
        if ra <= r <= rb: return za + (zb - za) * (r - ra) / (rb - ra)
    return pts[-1][1] if r > pts[-1][0] else pts[0][1]


def pointer_mesh(name, prof, R, H, r0, r1, w, z0, steps=24, lift=0.012, sink=0.02):
    """Thin inlaid line along +X that follows the cap's curvature, with rounded ends."""
    bm = bmesh.new()
    top, bot = [], []
    hw = w * R / 2
    for s in range(steps + 1):
        x = R * (r0 + (r1 - r0) * s / steps)
        z = z0 + surface_z(prof, R, H, x)
        top.append((bm.verts.new((x, -hw, z + lift * R)), bm.verts.new((x, hw, z + lift * R))))
        bot.append((bm.verts.new((x, -hw, z - sink * R)), bm.verts.new((x, hw, z - sink * R))))
    for s in range(steps):
        (a, b), (c, d) = top[s], top[s + 1]; bm.faces.new((a, c, d, b))
        (a2, b2), (c2, d2) = bot[s], bot[s + 1]; bm.faces.new((a2, b2, d2, c2))
        bm.faces.new((bot[s][0], bot[s + 1][0], top[s + 1][0], top[s][0]))
        bm.faces.new((top[s][1], top[s + 1][1], bot[s + 1][1], bot[s][1]))
    bm.faces.new((top[0][0], top[0][1], bot[0][1], bot[0][0]))
    bm.faces.new((bot[-1][0], bot[-1][1], top[-1][1], top[-1][0]))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return me


def _obj(coll, name, me, mat, parent):
    o = bpy.data.objects.new(name, me); coll.objects.link(o); me.materials.append(mat); o.parent = parent; return o


def build(coll, M, R, style, parent=None):
    P = params(style)
    prof, hr, lobes, depth, sharp, bm_, pm_, r0, r1, pw, skirt, TILT = (P[k] for k in KEYS)
    H = R * hr
    root = bpy.data.objects.new("Knob_" + style, None); coll.objects.link(root); root.parent = parent
    # tilt the body so its lower wall shows, like the approved screen (seen from slightly above); printed ticks stay flat
    tilt = bpy.data.objects.new("Tilt", None); coll.objects.link(tilt); tilt.parent = root
    tilt.rotation_euler = (math.radians(-TILT), 0, 0)
    spin = bpy.data.objects.new("Spin", None); coll.objects.link(spin); spin.parent = tilt
    z0 = 0.0
    if skirt:   # fine-knurled black skirt the dome sits on
        _obj(coll, "Skirt", lathe("skirt", SKIRT, R * 1.18, R * 0.24, lobes=72, depth=0.035, sharp=1.0, n=576),
             M["knob_skirt"], spin)
        z0 = R * 0.2
    _obj(coll, "KnobBody", lathe("body_" + style, prof, R, H, lobes, depth, sharp, z0=z0, crease=P.get("crease"), phase=P.get("phase", 0.0)), M[bm_], spin)
    _obj(coll, "Pointer", pointer_mesh("ptr_" + style, prof, R, H, r0, r1, pw, z0), M[pm_], spin)
    return root


def spin(root, ang_cw):
    """Pointer is modelled along +X; turn it to `ang_cw` degrees clockwise from 12 o'clock."""
    s = [c for c in root.children_recursive if c.name.startswith("Spin")][0]
    s.rotation_euler = (0, 0, math.radians(90 - ang_cw))


def add_ao(m, dist, floor):
    import os
    dist = float(os.environ.get("KNOB_AO_DIST", dist)); floor = float(os.environ.get("KNOB_AO_FLOOR", floor))
    nt = m.node_tree; b = [n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"][0]
    src = b.inputs["Base Color"].links[0].from_socket if b.inputs["Base Color"].is_linked else None
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion"); ao.inputs["Distance"].default_value = dist; ao.samples = 16
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[3].default_value = floor; mr.inputs[4].default_value = 1.0
    nt.links.new(ao.outputs["AO"], mr.inputs[0])
    mul = nt.nodes.new("ShaderNodeMix"); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"; mul.inputs[0].default_value = 1.0
    if src: nt.links.new(src, mul.inputs[6])
    else: mul.inputs[6].default_value = b.inputs["Base Color"].default_value
    nt.links.new(mr.outputs[0], mul.inputs[7]); nt.links.new(mul.outputs[2], b.inputs["Base Color"])


def materials(S, M):
    M["cream_knob"] = S.mat("CreamKnob", tex="cream_bakelite_1.png", rough=0.36, coat=0.25, coat_rough=0.22,
                            sat=1.0, tint=(0.9,0.58,0.4))
    add_ao(M["cream_knob"], 0.0015, 0.45)
    M["black_knob"] = S.mat("BlackKnob", base=(0.022, 0.02, 0.02), rough=0.42, coat=0.35, coat_rough=0.34)
    M["knob_skirt"] = S.mat("KnobSkirt", base=(0.022, 0.02, 0.019), rough=0.38)
    M["ptr_dark"] = S.mat("PtrDark", base=(0.02, 0.015, 0.012), rough=0.5)
    M["ptr_light"] = S.mat("PtrLight", base=(0.95, 0.88, 0.74), rough=0.45)
    _b = [n for n in M["ptr_light"].node_tree.nodes if n.type == "BSDF_PRINCIPLED"][0]   # painted line reads bright like the ref
    _b.inputs["Emission Color"].default_value = (0.95, 0.86, 0.7, 1); _b.inputs["Emission Strength"].default_value = 0.35
    return M
