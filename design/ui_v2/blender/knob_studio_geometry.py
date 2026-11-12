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

# Almost-flat caps with rolled shoulders: the lobes belong at the perimeter,
# not across the central face where they cause inflated, blotchy highlights.
FEEL_CAP = [(0.94, 0.0, 1), (1.0, 0.12, 1), (1.0, 0.46, 1),
            (0.985, 0.66, 1), (0.96, 0.80, 1), (0.92, 0.90, 1),
            (0.87, 0.96, 0.95), (0.82, 0.985, 0.65), (0.76, 0.997, 0.2),
            (0.68, 1.0, 0), (0.42, 1.006, 0), (0.0, 1.01, 0)]
SPACE_CAP = [(0.94, 0.0, 1), (1.0, 0.12, 1), (1.0, 0.45, 1),
             (0.99, 0.66, 1), (0.96, 0.82, 0.85), (0.92, 0.94, 0.5),
             (0.86, 1.0, 0.1), (0.78, 1.018, 0), (0.55, 1.03, 0), (0.0, 1.04, 0)]
VOICE_STUDIO = [(1,0,1),(1,.5,1),(.99,.68,1),(.96,.83,1),(.92,.94,1),
                (.86,.995,1),(.76,1.015,1),(.55,1.022,1),(0,1.025,1)]
FEEL_STUDIO = [(1,0,.9),(1,.35,1),(.99,.55,1),(.97,.75,1),(.93,.88,1),
               (.88,.95,.8),(.83,.985,.3),(.75,.997,0),(.5,1,0),(0,1,0)]
SPACE_STUDIO = [(1,0,1),(1,.2,1),(.985,.6,1),(.95,.8,.9),(.9,.9,.5),
                (.82,.96,.1),(.65,1,0),(.4,1.03,0),(0,1.045,0)]
MOLDED_CAP = [(1,0,1),(1,.55,1),(1,.75,1),(.99,.85,1),(.97,.94,1),
              (.93,.99,.8),(.86,1.02,.5),(.76,1.035,.15),(.6,1.045,0),(0,1.05,0)]
ROUND_FLUTED = [(1,0,.5),(1,.35,.6),(.995,.55,.8),(.98,.75,1),(.95,.88,.85),
                (.91,.96,.45),(.86,1.0,.08),(.78,1.02,0),(.6,1.035,0),(0,1.04,0)]
FEEL_DOMED = FLUTED[:8] + [(.765,1.02,.55),(.72,1.08,.25),(.66,1.12,.07),
                           (.55,1.17,0),(.35,1.22,0),(0,1.24,0)]
SPACE_DOMED = SPACE_STUDIO[:6] + [(.65,1.12,0),(.4,1.2,0),(0,1.24,0)]
BAKELITE_CAP = [(1,0,1),(1,.3,1),(.98,.65,1),(.94,.8,1),(.9,.85,.7),
                (.88,.855,0),(.88,.9,0),(.85,1,0),(.8,1.1,0),(.7,1.18,0),
                (.5,1.25,0),(0,1.28,0)]
DISHED_CAP = [(1,0,.5),(1,.4,.7),(.98,.7,1),(.93,.95,1),(.88,1.08,.8),
               (.8,1.12,.4),(.75,1.1,.1),(.6,1.05,0),(0,1.03,0)]

STYLES = {
    #          profile  H/R   lobes depth sharp  body mat      ptr mat      ptr r0..r1  ptr w  skirt  tilt
    "dome":    (DOME,    0.60, 0,    0.0,  1.0,  "cream_knob", "ptr_dark",  0.24, 0.88, 0.13, True,  18.0),
    "scallop": (FEEL_CAP,0.75, 9,    0.09, 1.0,  "cream_knob", "ptr_dark",  0.34, 0.86, 0.12, False, 6.0),
    "space":   (SPACE_CAP,0.48,8,    0.055,1.0,  "black_knob", "ptr_light", 0.06, 0.92, 0.095,False, 6.0),
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


def lathe(name, prof, R, H, lobes=0, depth=0.0, sharp=1.0, n=None, z0=0.0, crease=None, phase=0.0, axial_flutes=0.0):
    n = n or max(128, lobes * (12 if lobes > 30 else 48))   # knurls (many teeth) need fewer samples per tooth
    if lobes and sharp == -4:   # diamond knurl (knob_workshop): the pattern needs rings close together up the wall
        dense, gap = [], 2 * math.pi * R / lobes / 4
        for (ra, za, wa), (rb, zb, wb) in zip(prof, prof[1:]):
            k = max(1, int(abs(zb - za) * H / gap))
            dense += [(ra + (rb - ra) * j / k, za + (zb - za) * j / k, wa + (wb - wa) * j / k) for j in range(k)]
        prof = dense + [prof[-1]]
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
            if lobes and sharp == -4 and w > 0:   # two crossing 45-degree sets of V grooves
                u = lobes * t / (2 * math.pi); v = lobes * zf * H / R / (2 * math.pi)
                r *= 1 - depth * w * max(abs(2 * ((u + v) % 1) - 1), abs(2 * ((u - v) % 1) - 1))
            elif lobes and sharp == -3 and w > 0:
                # Exact circular finger scallops cut into a cylindrical blank.
                # Unlike a radial sine wave, each recess has a concave circular wall.
                pitch = 2 * math.pi / lobes
                delta = (tl + pitch/2) % pitch - pitch/2
                distance, hole_radius = 1.4, .4 + depth
                disc = hole_radius**2 - (distance*math.sin(delta))**2
                if disc >= 0:
                    boundary = min(1, distance*math.cos(delta)-math.sqrt(disc))
                    r *= 1-w*(1-boundary)
            elif lobes and sharp == -2 and w > 0:   # concave cylindrical scoop: depth*(1-u^2), u=-1..1 across a flute, crease at u=+-1
                u = min(1.0, abs(((lobes * tl / (2 * math.pi)) % 1.0) * 2 - 1) / (1 - LAND))   # flat land on each ridge
                r *= 1 - depth * w * (1 - u * u)
            elif lobes and sharp < 0: r *= 1 - depth * w * abs(math.sin(lobes * t / 2))
            elif lobes: r *= 1 - depth * w * ((1 - math.cos(lobes * tl)) / 2) ** sharp
            dz = axial_flutes * H * w * min(1, zf / .3) * (1 - math.cos(lobes * tl)) / 2 if lobes else 0
            ring.append(bm.verts.new((r * math.cos(t), r * math.sin(t), z0 + zf * H - dz)))
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


def pointer_mesh(name, prof, R, H, r0, r1, w, z0, steps=96, lift=0.012, sink=0.02, cap_round=1.0):
    """Thin inlaid line along +X that follows the cap's curvature, with rounded ends."""
    bm = bmesh.new()
    top, bot = [], []
    hw = w * R / 2
    for s in range(steps + 1):
        x = R * (r0 + (r1 - r0) * s / steps)
        end_distance = min(x - R*r0, R*r1 - x)
        cr = hw * cap_round
        wh = hw if end_distance >= cr else hw-cr+math.sqrt(max(0.000001*R*R, cr*cr - (cr-end_distance)**2))
        z = z0 + surface_z(prof, R, H, x)
        top.append((bm.verts.new((x, -wh, z + lift * R)), bm.verts.new((x, wh, z + lift * R))))
        bot.append((bm.verts.new((x, -wh, z - sink * R)), bm.verts.new((x, wh, z - sink * R))))
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
    if P.get("parts"):   # knob_workshop recipe: stacked lathes (Base, Top), each with its own outline and grip
        z = 0.0
        for part in P["parts"]:
            globals()["LAND"] = part.get("land", LAND)
            last = (part["prof"], R * part["width"], R * part["height"], z)
            _obj(coll, "KnobBody " + part["name"], lathe("body_%s_%s" % (style, part["name"]), last[0], last[1], last[2],
                 part["lobes"], part["depth"], part["sharp"], z0=z, phase=part.get("phase", 0.0)), M[part.get("mat", bm_)], spin)
            z += last[2]
        pointer = _obj(coll, "Pointer", pointer_mesh("ptr_" + style, last[0], last[1], last[2], r0, r1, pw, last[3],
                       cap_round=1.0 if pm_ == "ptr_light" else 0.2), M[pm_], spin)   # r0/r1/pw are fractions of the Top part
        bevel = pointer.modifiers.new("Rounded painted inlay edges", "BEVEL")
        bevel.width = last[1] * 0.015; bevel.segments = 3
        return root
    z0 = 0.0
    if skirt:   # fine-knurled black skirt the dome sits on
        _obj(coll, "Skirt", lathe("skirt", SKIRT, R * P.get("skirt_radius", 1.12), R * P.get("skirt_hr", .24), lobes=72, depth=P.get("skirt_depth", .018), sharp=1.0, n=576),
             M["knob_skirt"], spin)
        z0 = R * 0.2
        if P.get("collar", False):
            collar_prof=[(1,0,0),(1,.72,0),(.998,.87,0),(.985,.96,0),(.96,1,0),(.92,1.015,0),(0,1.015,0)]
            _obj(coll, "Raised black collar", lathe("collar",collar_prof,R*1.065,R*.61),M["knob_skirt"],spin)
    body = _obj(coll, "KnobBody", lathe("body_" + style, prof, R, H, lobes, depth, sharp, z0=z0, crease=P.get("crease"), phase=P.get("phase", 0.0), axial_flutes=P.get("axial_flutes", 0)), M[bm_], spin)
    if P.get("subdivision", 0):
        mod = body.modifiers.new("Smooth molded shoulder", "SUBSURF")
        mod.levels = P["subdivision"]; mod.render_levels = P["subdivision"]
    pointer = _obj(coll, "Pointer", pointer_mesh("ptr_" + style, prof, R, H, r0, r1, pw, z0,
                   cap_round=1.0 if pm_ == "ptr_light" else 0.2), M[pm_], spin)
    bevel = pointer.modifiers.new("Rounded painted inlay edges", "BEVEL")
    bevel.width = R * 0.015; bevel.segments = 3
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
    M["cream_knob"] = S.mat("CreamKnob", base=(0.66, 0.39, 0.18), rough=0.27, coat=0.2, coat_rough=0.19)
    M["black_knob"] = S.mat("BlackKnob", base=(0.009, 0.008, 0.007), rough=0.31, coat=0.16, coat_rough=0.25)
    M["knob_skirt"] = S.mat("KnobSkirt", base=(0.008, 0.007, 0.006), rough=0.55)
    next(n for n in M["knob_skirt"].node_tree.nodes if n.type == "BSDF_PRINCIPLED").inputs["Specular IOR Level"].default_value = 0.1
    M["ptr_dark"] = S.mat("PtrDark", base=(0.003, 0.0025, 0.002), rough=0.85)
    dark = next(n for n in M["ptr_dark"].node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    dark.inputs["Specular IOR Level"].default_value = 0.12
    M["ptr_light"] = S.mat("PtrLight", base=(0.95, 0.88, 0.74), rough=0.45)
    _b = [n for n in M["ptr_light"].node_tree.nodes if n.type == "BSDF_PRINCIPLED"][0]   # painted line reads bright like the ref
    _b.inputs["Emission Color"].default_value = (0.95, 0.86, 0.7, 1); _b.inputs["Emission Strength"].default_value = 0.35
    return M
