"""The floor pedal assembly as real 3D (Joshua: "carpet bending or textures patchy around pedals, make sure it's real
blender 3d models"): three pedals pivoting on the cross bar, their pull rods, the cross bar, and the two knee levers,
rendered in Cycles through a camera solved from APPROVED_main.png (canvas = 3200x2000), with a shadow-catcher floor so
every layer carries its own soft shadow and drops onto the one unchanging rug plate (parts/floor_3d.py).

blender --factory-startup -b -P pedals_3d.py -- OUT_DIR [chrome|brass] [samples]
Writes OUT_DIR/<layer>_<frame>.png (RGBA, the canvas region REG) and OUT_DIR/render.json.

Camera: orthographic, pitched PHI below horizontal. Canvas mapping (fitted to the reference's pedals, rods and bar):
    x = X0 + S*X        y = C - S*(Y*sin(PHI) + Z*cos(PHI))        (metres; X right, Y away from the player, Z up)
"""
import bpy, json, math, os, sys
from pathlib import Path
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = Path(args[0]).resolve(); out.mkdir(parents=True, exist_ok=True)
METAL = args[1] if len(args) > 1 else "chrome"
SAMPLES = int(args[2]) if len(args) > 2 else 128
FRAMES = 7                                   # press travel 0..1
S, PHI, C, X0 = 1719.0, math.radians(69.5), 1243.0, 1537.0
REG = (990, 700, 2010, 1340)                 # canvas region rendered (x0, y0, x1, y1)
sP, cP = math.sin(PHI), math.cos(PHI)
U = Vector((0, sP, cP)); D = Vector((0, cP, -sP))

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = not os.environ.get("NO_DENOISE")
prefs = bpy.context.preferences.addons["cycles"].preferences
try:
    prefs.compute_device_type = "OPTIX"; prefs.get_devices()
    for dv in prefs.devices: dv.use = dv.type == "OPTIX"
except Exception:
    sc.cycles.device = "CPU"
sc.render.film_transparent = True
sc.render.resolution_x = REG[2] - REG[0]; sc.render.resolution_y = REG[3] - REG[1]; sc.render.resolution_percentage = 100
sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
sc.view_settings.view_transform = "Standard"; sc.view_settings.exposure = 0.0
sc.cycles.film_exposure = 1.0

world = bpy.data.worlds.new("Room"); sc.world = world; world.use_nodes = True
bgn = world.node_tree.nodes["Background"]; bgn.inputs[0].default_value = (0.05, 0.036, 0.027, 1); bgn.inputs[1].default_value = 1.0

# camera
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = "ORTHO"; cam.data.ortho_scale = (REG[2] - REG[0]) / S
icx, icy = (REG[0] + REG[2]) / 2, (REG[1] + REG[3]) / 2
cam.location = Vector(((icx - X0) / S, 0, 0)) + U * ((C - icy) / S) - D * 4.0
cam.rotation_euler = (math.pi / 2 - PHI, 0, 0)
cam.data.clip_end = 20


def mat(name, base, metal=0.0, rough=0.5, coat=0.0, aniso=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if coat: b.inputs["Coat Weight"].default_value = coat
    if aniso: b.inputs["Anisotropic"].default_value = aniso
    return m


M_METAL = mat("Chrome", (0.86, 0.86, 0.87), 1.0, 0.09) if METAL == "chrome" else mat("Brass", (0.86, 0.62, 0.30), 1.0, 0.16)
M_RUBBER = mat("Rubber", (0.008, 0.007, 0.006), 0.0, 0.62)
M_PADDLE = mat("Paddle", (0.010, 0.009, 0.008), 0.0, 0.30)
M_LIP = mat("RubberLip", (0.006, 0.005, 0.005), 0.0, 0.62)
M_RIB = mat("RubberRib", (0.012, 0.010, 0.009), 0.0, 0.42)
for m_, sp_ in ((M_RUBBER, 0.07), (M_LIP, 0.18), (M_RIB, 0.22), (M_PADDLE, 0.2)):   # rubber reflects far less than the 0.5 default
    m_.node_tree.nodes["Principled BSDF"].inputs["Specular IOR Level"].default_value = sp_
M_FLOOR = mat("Rug", (0.10, 0.035, 0.022), 0.0, 0.9)


def obj(name, me_op, material, loc, scale=(1, 1, 1), rot=(0, 0, 0), bevel=0.0, seg=4, parent=None):
    me_op(); o = bpy.context.object; o.name = name
    o.location = loc; o.scale = scale; o.rotation_euler = rot
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        bm = o.modifiers.new("bevel", "BEVEL"); bm.width = bevel; bm.segments = seg; bm.limit_method = "ANGLE"
    for p in o.data.polygons: p.use_smooth = True
    if not bevel:   # cylinders: keep the end caps out of the side normals (smoothing them in tilted every rod's
        o.data.set_sharp_from_angle(angle=math.radians(30))   # shading normal along its length: wrong reflections)
    o.data.materials.append(material)
    if parent: o.parent = parent; o.matrix_parent_inverse = parent.matrix_world.inverted()
    return o


def cube(): bpy.ops.mesh.primitive_cube_add(size=1)
def cyl(v=48): return lambda: bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=1, depth=1)
def sph(): bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=1)


def rod(name, p0, p1, r, material, parent=None):
    v = p1 - p0; L = v.length
    o = obj(name, cyl(32), material, (p0 + p1) / 2, (r, r, L))
    o.rotation_mode = "QUATERNION"; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(v)
    if parent: o.parent = parent; o.matrix_parent_inverse = parent.matrix_world.inverted()
    return o


def empty(name, loc):
    e = bpy.data.objects.new(name, None); sc.collection.objects.link(e); e.location = loc; return e


# floor: shadow catcher (also what the chrome reflects below)
fl = obj("Floor", lambda: bpy.ops.mesh.primitive_plane_add(size=4), M_FLOOR, (0, 0.3, 0))
fl.is_shadow_catcher = True

layers = {}
# cross bar (static): rounded ends
BAR_Z, BAR_R, BAR_HALF = 0.035, 0.0064, 0.2304
bar = [obj("Bar", cyl(48), M_METAL, (0, 0, BAR_Z), (BAR_R, BAR_R, BAR_HALF * 2), rot=(0, math.pi / 2, 0))]
for sgn in (-1, 1):
    bar.append(obj("BarEnd", sph, M_METAL, (sgn * BAR_HALF, 0, BAR_Z), (BAR_R * 0.7, BAR_R, BAR_R)))
layers["bar"] = {"objects": bar, "anim": None}

# pedals: housing pivoting on the bar, ribbed rubber pad, pull rod from the toe (far end) straight up
PED_X = {"A": (1355 - X0) / S, "B": 0.0, "C": (1717 - X0) / S}
PW, PY0, PY1, PZ0, PZ1 = 0.064, -0.0367, 0.0565, 0.017, 0.050
ROD_Y, ROD_R, ROD_TOP = 0.031, 0.0037, 0.86
PRESS_DEG = 13.0
RODS = bpy.data.collections.new("RodReceivers")   # the pull rods: see the RodGlint strips (below) only
ROD_PX = []
PADS = bpy.data.collections.new("PadReceivers")   # lit by the low front pad light only (no rug shadows from it)
for k, px in PED_X.items():
    piv = empty("Pivot_" + k, (px, 0, BAR_Z))
    parts = [obj("Housing_" + k, cube, M_METAL, (px, (PY0 + PY1) / 2, (PZ0 + PZ1) / 2), (PW, PY1 - PY0, PZ1 - PZ0), bevel=0.0035, seg=5, parent=piv)]
    parts.append(obj("Pad_" + k, cube, M_RUBBER, (px, (PY0 + PY1) / 2 + 0.001, PZ1 + 0.0012), (PW - 0.009, PY1 - PY0 - 0.008, 0.0026), bevel=0.0012, seg=3, parent=piv))
    nrib = 15
    for r_ in range(nrib):
        rx = px - (PW - 0.014) / 2 + (PW - 0.014) * r_ / (nrib - 1)
        # rounded ridges: each crest catches a thin line of light, the grooves between stay dark (as on the reference)
        rib = obj("Rib", cyl(16), M_RIB, (rx, (PY0 + PY1) / 2 + 0.0026, PZ1 + 0.0025), (0.00085, 0.00085, PY1 - PY0 - 0.0172), rot=(math.pi / 2, 0, 0), parent=piv)
        parts.append(rib)
    # the pad's smooth black rubber lip rolled over its front edge, before the chrome (as on the reference)
    lip = obj("Lip_" + k, cyl(48), M_LIP, (px, PY0 + 0.0058, PZ1 + 0.0017), (0.0038, 0.0038, PW - 0.010), rot=(0, math.pi / 2, 0), parent=piv)
    parts.append(lip)
    for p_ in parts:   # pads, ribs and the chrome housing (its front edge catches it); not the black lip
        if p_ is not lip: PADS.objects.link(p_)
    lean = 0.04 * px                      # the reference's rods converge slightly toward the floor
    top = Vector((px + lean * (ROD_TOP - PZ1) / 0.58, ROD_Y, ROD_TOP))
    rd = [rod("Rod_" + k, Vector((px, ROD_Y, PZ1 + 0.002)), top, ROD_R, M_METAL)]
    RODS.objects.link(rd[0])
    # where the rod enters the pad: a small black rubber socket (as on the reference), not a chrome collar
    rd.append(obj("Socket_" + k, sph, M_LIP, (px, ROD_Y, PZ1 + 0.0032), (0.0054, 0.0054, 0.0032)))
    ROD_PX.append(px)
    layers["pedal_" + k] = {"objects": parts + rd, "anim": ("pedal", piv, rd)}

# knee levers: an arm running under the body toward the player, a black paddle facing the knee; swings about a
# vertical pivot under the body
KN_X = {"L": (1113 - X0) / S, "R": (1804 - X0) / S}
KN_Z, KN_PIVY, PAD_W, PAD_H = 0.60, 0.20, 0.045, 0.048
pad_c_pu = (C - 958) / S
for k, kx in KN_X.items():
    piv = empty("KneePivot_" + k, (kx, KN_PIVY, KN_Z))
    py = (pad_c_pu - KN_Z * cP) / sP
    arm_end = py + 0.016
    parts = [rod("Arm_" + k, Vector((kx, KN_PIVY, KN_Z + 0.032)), Vector((kx, arm_end, KN_Z + 0.032)), 0.0072, M_METAL, parent=piv)]
    parts.append(obj("Elbow_" + k, sph, M_METAL, (kx, arm_end, KN_Z + 0.032), (0.0072, 0.0072, 0.0072), parent=piv))
    parts.append(rod("Drop_" + k, Vector((kx, arm_end, KN_Z + 0.032)), Vector((kx, arm_end - 0.004, KN_Z + 0.012)), 0.0062, M_METAL, parent=piv))
    pad = obj("Paddle_" + k, cube, M_PADDLE, (kx, py, KN_Z), (PAD_W, 0.009, PAD_H), bevel=0.006, seg=6)
    pad.rotation_mode = "QUATERNION"; pad.rotation_quaternion = Vector((0, 1, 0)).rotation_difference(D)
    pad.parent = piv; pad.matrix_parent_inverse = piv.matrix_world.inverted()
    layers["knee_" + k] = {"objects": parts + [pad], "anim": ("knee", piv, -1 if k == "L" else 1)}

# lights: warm room key from front-left above, broad soft top, cool-ish faint back rim
def area(name, loc, size, power, col, target=(0, 0.02, 0.1)):
    ld = bpy.data.lights.new(name, "AREA"); ld.energy = power; ld.shape = "RECTANGLE"; ld.size, ld.size_y = size; ld.color = col
    o = bpy.data.objects.new(name, ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
area("Key", (-1.3, -0.45, 1.05), (0.45, 0.35), 30, (1.0, 0.76, 0.52))      # warm room lamp, front left
# two warm lamps behind the steel, left and right: each rod throws a soft V of shadow forward across its pedal pad
# and onto the cross bar on both sides (Joshua's red lines on the reference)
area("BackL", (-0.75, 0.95, 0.95), (0.35, 0.3), 26, (1.0, 0.78, 0.55))
area("BackR", (0.75, 0.95, 0.95), (0.35, 0.3), 22, (1.0, 0.80, 0.58))
area("Top", (0.1, 0.2, 1.8), (1.6, 1.0), 7, (1.0, 0.88, 0.74))         # broad fill: contact shading
# low warm light from the player's side, close: its falloff across each pad leaves the front of the rubber lighter
# and the back darker (as on the reference); linked to the pads only, so it adds no shadows on the rug
pl = area("PadFront", (0.0, -0.30, 0.16), (0.7, 0.12), 9, (1.0, 0.80, 0.60), target=(0, 0.01, 0.05))
pl.light_linking.receiver_collection = PADS
# the room the chrome reflects (glossy rays only, lights nothing): warm, bright above, dimmer toward the horizon
bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=6, location=(0, 0, 0))
dome = bpy.context.object; dome.name = "RoomDome"
for p in dome.data.polygons: p.use_smooth = True
dm = bpy.data.materials.new("RoomDome"); dm.use_nodes = True; nt = dm.node_tree; nt.nodes.clear()
o_ = nt.nodes.new("ShaderNodeOutputMaterial"); em = nt.nodes.new("ShaderNodeEmission")
geo = nt.nodes.new("ShaderNodeNewGeometry"); sep = nt.nodes.new("ShaderNodeSeparateXYZ")
nt.links.new(geo.outputs["Position"], sep.inputs[0])
def mr(sock, a0, a1, b0, b1):
    n = nt.nodes.new("ShaderNodeMapRange"); n.clamp = True
    n.inputs[1].default_value, n.inputs[2].default_value, n.inputs[3].default_value, n.inputs[4].default_value = a0, a1, b0, b1
    nt.links.new(sock, n.inputs[0]); return n.outputs[0]
def mul(a, b):
    n = nt.nodes.new("ShaderNodeMath"); n.operation = "MULTIPLY"; nt.links.new(a, n.inputs[0]); nt.links.new(b, n.inputs[1]); return n.outputs[0]
up = mr(sep.outputs["Z"], 0.6, 5.5, 0.0, 1.0)            # dark below the horizon, brighter overhead
side = mr(sep.outputs["X"], 5.0, -5.0, 0.35, 1.0)        # the lamp side (left) brighter
front = mr(sep.outputs["Y"], 5.0, -5.0, 0.6, 1.0)       # the room behind the player brighter than the wall
v = mul(mul(up, side), front)
pw = nt.nodes.new("ShaderNodeMath"); pw.operation = "POWER"; nt.links.new(v, pw.inputs[0]); pw.inputs[1].default_value = 1.6
ramp = nt.nodes.new("ShaderNodeValToRGB"); nt.links.new(pw.outputs[0], ramp.inputs[0])
ramp.color_ramp.elements[0].position = 0.0; ramp.color_ramp.elements[0].color = (0.02, 0.014, 0.01, 1)
ramp.color_ramp.elements[1].position = 1.0; ramp.color_ramp.elements[1].color = (1.0, 0.80, 0.58, 1)
nt.links.new(ramp.outputs[0], em.inputs["Color"]); em.inputs["Strength"].default_value = 2.8
nt.links.new(em.outputs[0], o_.inputs[0]); dome.data.materials.append(dm)
dome.visible_camera = False; dome.visible_diffuse = False; dome.visible_shadow = False
dome.visible_transmission = False; dome.visible_volume_scatter = False; dome.visible_glossy = True

# the pull rods (Joshua: "silver reflective here more in reference than ours"). Seen from this camera a vertical
# chrome rod mirrors the floor just in front of and below it (the dark rug), so it read dark. Each rod gets a warm
# emitter strip on the floor exactly where its left-of-centre facets look: glossy rays only, light-linked to the rods,
# so it shows up as a bright silver line running down each rod and touches nothing else. Every point of a
# cylinder at facet angle th reflects the same direction r(th); a wedge (width ~ height) keeps the line even.
dv = Vector((0, math.cos(PHI), -math.sin(PHI)))
def refl(th):
    n = Vector((math.sin(th), -math.cos(th), 0)); return dv - 2 * dv.dot(n) * n
def glint_mat(name, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt_ = m.node_tree; nt_.nodes.clear()
    e = nt_.nodes.new("ShaderNodeEmission"); e.inputs["Color"].default_value = (1.0, 0.88, 0.72, 1)
    e.inputs["Strength"].default_value = strength
    # emission over a fully transparent surface: everything that is not linked to it (the housings, the bar) looks
    # straight through to the rug instead of seeing a black strip
    tr = nt_.nodes.new("ShaderNodeBsdfTransparent"); ad = nt_.nodes.new("ShaderNodeAddShader")
    nt_.links.new(e.outputs[0], ad.inputs[0]); nt_.links.new(tr.outputs[0], ad.inputs[1])
    nt_.links.new(ad.outputs[0], nt_.nodes.new("ShaderNodeOutputMaterial").inputs[0])
    try: m.emission_sampling = "BOTH"
    except Exception: pass
    return m
GK = float(os.environ.get("ROD_GLINT", "1.0"))
# broad soft strip: the silver body of the rod (both flanks grey, not black); tight strip: the white core line
GLINTS = [(glint_mat("RodSheen", 0.3 * GK), math.radians(-12), math.radians(52)),
          (glint_mat("RodCore", 0.9 * GK), math.radians(-20), math.radians(9))]
for i, px in (enumerate(ROD_PX) if not os.environ.get("NO_GLINT_OBJ") else []):
    for j, (gm, th0, thw) in enumerate(GLINTS):
        # built as a strip of short segments: one long sliver triangle pair samples badly in Cycles
        # at the height of the pad top, not the floor: the rod's lowest stretch (over the pad) mirrors the pad, so
        # floor strips left it dark; every non-rod surface looks straight through these strips anyway
        GZ = PZ1 + 0.006; NS = 24; vs = []; fs = []
        zt = ROD_TOP * float(os.environ.get("GLINT_EXT", "1"))
        for si in range(NS + 1):
            z = GZ + 0.003 + (zt - GZ - 0.003) * si / NS
            for th in (th0 - thw, th0 + thw):
                r = refl(th); t = (z - GZ) / -r.z
                vs.append((px + r.x * t, ROD_Y + r.y * t, GZ + 0.0004 * j))
            if si: fs.append((2 * si - 2, 2 * si - 1, 2 * si + 1, 2 * si))
        me = bpy.data.meshes.new("RodGlint"); me.from_pydata(vs, [], fs); me.update(); me.materials.append(gm)
        g = bpy.data.objects.new("RodGlint_%d_%d" % (i, j), me); sc.collection.objects.link(g)
        g.visible_camera = bool(os.environ.get("GLINT_DEBUG")); g.visible_diffuse = False; g.visible_shadow = False
        g.visible_transmission = False; g.visible_volume_scatter = False; g.visible_glossy = True
        g.light_linking.receiver_collection = RODS

allobjs = [o for L in layers.values() for o in L["objects"]]
meta = {"region": REG, "frames": FRAMES, "metal": METAL, "press_deg": PRESS_DEG, "layers": {}}
def render_to(path):
    sc.render.filepath = str(path); bpy.ops.render.render(write_still=True)


ONLY = [x for x in os.environ.get("PEDAL_LAYERS", "").split(",") if x]   # re-render just these layers
for name, L in layers.items():
    if ONLY and name not in ONLY: continue
    for o in allobjs: o.hide_render = o not in L["objects"]; o.visible_camera = True
    if name == "bar":   # the bar also receives the pedals' and rods' shadows: render it once more with them as
        render_to(out / "bar_solo_00.png")   # shadow casters only; floor_3d.py takes the bar's own pixels from that
        for o in allobjs:
            if o not in L["objects"]: o.hide_render = False; o.visible_camera = False
        render_to(out / "bar_lit_00.png")
        for o in allobjs: o.visible_camera = True; o.hide_render = o not in L["objects"]
        meta["layers"][name] = 1; continue
    # the bar is a holdout here: these layers' rug shadows must not darken the bar (bar_lit carries the shadows on
    # it); it casts nothing itself (its rug shadow is in the bar layer)
    for o in layers["bar"]["objects"]:
        o.hide_render = False; o.is_holdout = True; o.visible_shadow = False
    nfr = 1 if L["anim"] is None else FRAMES
    for f in range(nfr):
        t = f / (FRAMES - 1) if nfr > 1 else 0.0
        if L["anim"] and L["anim"][0] == "pedal":
            _, piv, rd = L["anim"]
            piv.rotation_euler = (-math.radians(PRESS_DEG) * t, 0, 0)
            # the toe end (rod attachment) drops: move the rod with it
            a = -math.radians(PRESS_DEG) * t
            y0, z0 = ROD_Y, PZ1 + 0.002 - BAR_Z
            dz = (y0 * math.sin(a) + z0 * math.cos(a)) - z0; dy = (y0 * math.cos(a) - z0 * math.sin(a)) - y0
            for o in rd:
                o.delta_location = (0, dy, dz)
        elif L["anim"] and L["anim"][0] == "knee":
            _, piv, sgn = L["anim"]
            piv.rotation_euler = (0, 0, sgn * math.radians(4.5) * t)
        bpy.context.view_layer.update()
        sc.render.filepath = str(out / ("%s_%02d.png" % (name, f)))
        bpy.ops.render.render(write_still=True)
    meta["layers"][name] = nfr
    if L["anim"] and L["anim"][0] == "pedal":
        L["anim"][1].rotation_euler = (0, 0, 0)
        for o in L["anim"][2]: o.delta_location = (0, 0, 0)
    elif L["anim"]:
        L["anim"][1].rotation_euler = (0, 0, 0)
for o in allobjs: o.hide_render = False
bpy.ops.wm.save_as_mainfile(filepath=str(out / "pedals_3d.blend"))
old = json.loads((out / "render.json").read_text()) if (ONLY and (out / "render.json").exists()) else {"layers": {}}
meta["layers"] = {**old["layers"], **meta["layers"]}
(out / "render.json").write_text(json.dumps(meta, indent=1))
