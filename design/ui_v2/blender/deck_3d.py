"""The deck's foot switches A/B/C and the LKL/LKR knee-lever toggles as real 3D models (Joshua: "do all": the deck
pedals were image swaps with horizontal ribs, the two toggles identical, nothing moved between off and on).

Same parts and materials as the floor pedals (pedals_3d.py): a chrome housing with soft bevels, black ribbed rubber pad
with vertical ribs and a matte black lip at the front. LKL and LKR are mirror images. Each control is rendered alone
over a shadow-catcher panel, FRAMES frames from rest to engaged (pedals tip forward into the panel about their back
edge; levers swing outward about their ring mount). Lights and camera never move.

blender --factory-startup -b -P deck_3d.py -- OUT_DIR [samples]
Env DECK_LAYERS=a,b re-renders only those layers.

Camera: orthographic, looking down PHI below horizontal. Canvas mapping (3200x2000, fitted to the deck's controls):
    x = X0 + S*X        y = C - S*(Y*sin(PHI) + Z*cos(PHI))        (metres; X right, Y away from the player, Z up)
"""
import bpy, json, math, os, sys
from pathlib import Path
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = Path(args[0]).resolve(); out.mkdir(parents=True, exist_ok=True)
SAMPLES = int(args[1]) if len(args) > 1 else 128
S, PHI, C, X0 = 2000.0, math.radians(60.0), 1728.0, 2050.0
sP, cP = math.sin(PHI), math.cos(PHI)
REG = (1950, 1420, 3070, 1770)        # canvas render region
FRAMES = 7

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
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

world = bpy.data.worlds.new("Room"); sc.world = world; world.use_nodes = True
bgn = world.node_tree.nodes["Background"]; bgn.inputs[0].default_value = (0.04, 0.03, 0.022, 1); bgn.inputs[1].default_value = 1.0

U = Vector((0, sP, cP)); D = Vector((0, cP, -sP))
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = "ORTHO"; cam.data.ortho_scale = (REG[2] - REG[0]) / S
icx, icy = (REG[0] + REG[2]) / 2, (REG[1] + REG[3]) / 2
cam.location = Vector(((icx - X0) / S, 0, 0)) + U * ((C - icy) / S) - D * 4.0
cam.rotation_euler = (math.pi / 2 - PHI, 0, 0)
cam.data.clip_end = 20


def mat(name, base, metal=0.0, rough=0.5, spec=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if spec is not None: b.inputs["Specular IOR Level"].default_value = spec
    return m


M_METAL = mat("Chrome", (0.86, 0.86, 0.87), 1.0, 0.09)
M_RUBBER = mat("Rubber", (0.008, 0.007, 0.006), 0.0, 0.62, 0.07)
M_RIB = mat("RubberRib", (0.012, 0.010, 0.009), 0.0, 0.42, 0.22)
M_LIP = mat("RubberLip", (0.003, 0.0025, 0.0025), 0.0, 0.8, 0.04)
M_HOLE = mat("Hole", (0.002, 0.002, 0.002), 0.0, 0.9, 0.0)
def halo_mat(name):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt_ = m.node_tree; nt_.nodes.clear()
    e = nt_.nodes.new("ShaderNodeEmission"); e.inputs["Color"].default_value = (1.0, 0.55, 0.16, 1)
    e.inputs["Strength"].default_value = 0.0
    g_ = nt_.nodes.new("ShaderNodeBsdfPrincipled"); g_.inputs["Base Color"].default_value = (0.05, 0.03, 0.02, 1)
    g_.inputs["Roughness"].default_value = 0.15
    ad = nt_.nodes.new("ShaderNodeAddShader"); nt_.links.new(g_.outputs[0], ad.inputs[0]); nt_.links.new(e.outputs[0], ad.inputs[1])
    nt_.links.new(ad.outputs[0], nt_.nodes.new("ShaderNodeOutputMaterial").inputs[0]); return m, e
M_HALO, HALO_EM = {}, {}
for k_ in ("L", "R"): M_HALO[k_], HALO_EM[k_] = halo_mat("Halo_" + k_)
M_PANEL = mat("Panel", (0.022, 0.016, 0.012), 0.0, 0.55)   # the leather as the chrome sees it (shadow catcher)


def obj(name, me_op, material, loc, scale=(1, 1, 1), rot=(0, 0, 0), bevel=0.0, seg=4, parent=None):
    me_op(); o = bpy.context.object; o.name = name
    o.location = loc; o.scale = scale; o.rotation_euler = rot
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        bm = o.modifiers.new("bevel", "BEVEL"); bm.width = bevel; bm.segments = seg; bm.limit_method = "ANGLE"
    for p in o.data.polygons: p.use_smooth = True
    if not bevel: o.data.set_sharp_from_angle(angle=math.radians(30))
    o.data.materials.append(material)
    if parent: o.parent = parent; o.matrix_parent_inverse = parent.matrix_world.inverted()
    return o


def cube(): bpy.ops.mesh.primitive_cube_add(size=1)
def cyl(v=48): return lambda: bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=1, depth=1)
def sph(): bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=1)
def torus(R, r): return lambda: bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=64, minor_segments=24)


def rod(name, p0, p1, r, material, parent=None):
    v = p1 - p0; L = v.length
    o = obj(name, cyl(32), material, (p0 + p1) / 2, (r, r, L))
    o.rotation_mode = "QUATERNION"; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(v)
    if parent: o.parent = parent; o.matrix_parent_inverse = parent.matrix_world.inverted()
    return o


def empty(name, loc):
    e = bpy.data.objects.new(name, None); sc.collection.objects.link(e); e.location = loc; return e


def ribbed_pad(name, cx, cy, top, w, l, parent, ribs):
    """black rubber pad (w across, l along Y) with vertical ribs and a matte lip rolled over its front edge"""
    parts = [obj(name, cube, M_RUBBER, (cx, cy + 0.001, top + 0.0012), (w, l, 0.0026), bevel=0.0012, seg=3, parent=parent)]
    for r_ in range(ribs):
        rx = cx - (w - 0.006) / 2 + (w - 0.006) * r_ / (ribs - 1)
        parts.append(obj(name + "Rib", cyl(16), M_RIB, (rx, cy + 0.0026, top + 0.0025), (0.00085, 0.00085, l - 0.0172),
                         rot=(math.pi / 2, 0, 0), parent=parent))
    parts.append(obj(name + "Lip", cyl(48), M_LIP, (cx, cy - l / 2 + 0.0018, top + 0.0017), (0.0038, 0.0038, w - 0.001),
                     rot=(0, math.pi / 2, 0), parent=parent))
    return parts


# panel: shadow catcher (the deck's leather is the background plate)
fl = obj("Panel", lambda: bpy.ops.mesh.primitive_plane_add(size=3), M_PANEL, (0.4, 0.1, 0))
fl.is_shadow_catcher = True

layers = {}
# foot switches: chrome housing hinged at its back bottom edge, ribbed pad on top; pressed = front tips into the panel
PW, PL, PH = 0.072, 0.112, 0.034
PRESS_DEG = 14.0
PED_X = {"A": 0.0, "B": (2230.5 - X0) / S, "C": (2405.0 - X0) / S}
for k, px in PED_X.items():
    piv = empty("Hinge_" + k, (px, PL, 0))
    parts = [obj("Housing_" + k, cube, M_METAL, (px, PL / 2, PH / 2), (PW, PL, PH), bevel=0.004, seg=8, parent=piv)]
    parts += ribbed_pad("Pad_" + k, px, PL / 2, PH, PW - 0.013, PL - 0.012, piv, 15)   # a 6 mm flat chrome rim shows
    layers["pedal_" + k] = {"objects": parts, "anim": ("pedal", piv)}

# knee-lever toggles: chrome ring mount on the panel, a chrome rod out to a small ribbed paddle; LKR mirrors LKL about
# the middle of the pair; engaged = the lever swings outward about the ring
RING = {"L": (2733.0 - X0) / S}
MID = (2801.5 - X0) / S
RING["R"] = 2 * MID - RING["L"]
RING_Y, RING_Z = 0.1397, 0.006
PAD_DX, PAD_Y, PAD_W, PAD_L, PAD_H = -0.042, 0.040, 0.066, 0.050, 0.018
SWING_DEG = 14.0
for k, rx in RING.items():
    sgn = 1 if k == "L" else -1                       # LKL paddle to the lower left of its ring, LKR lower right
    piv = empty("Ring_" + k, (rx, RING_Y, 0))
    parts = [obj("Mount_" + k, torus(0.0125, 0.0034), M_METAL, (rx, RING_Y, RING_Z))]           # fixed
    parts.append(obj("Hole_" + k, cyl(48), M_HOLE, (rx, RING_Y, RING_Z - 0.002), (0.0095, 0.0095, 0.004)))
    # engaged lamp: a warm jewel halo inside the ring mount that lights as the lever swings out (the A/B/C lamps'
    # colour), so the toggles show their state as plainly as the pedals do
    halo = obj("Halo_" + k, torus(0.0086, 0.0016), M_HALO[k], (rx, RING_Y, RING_Z + 0.0005))
    parts.append(halo)
    swing = [obj("Ball_" + k, sph, M_METAL, (rx, RING_Y, RING_Z + 0.002), (0.0058, 0.0058, 0.0058), parent=piv)]
    pcx, pcy = rx + sgn * PAD_DX, PAD_Y
    p_top = Vector((pcx, pcy + PAD_L / 2 - 0.006, PAD_H * 0.8))        # into the paddle's back, centred
    swing.append(rod("Rod_" + k, Vector((rx, RING_Y, RING_Z + 0.002)), p_top, 0.0055, M_METAL, parent=piv))
    swing.append(obj("Boss_" + k, cyl(32), M_METAL, (pcx, pcy + PAD_L / 2 - 0.004, PAD_H * 0.8), (0.0075, 0.0075, 0.012),
                     rot=(math.pi / 2, 0, 0), bevel=0.0015, seg=3, parent=piv))
    swing.append(obj("Paddle_" + k, cube, M_METAL, (pcx, pcy, PAD_H / 2), (PAD_W, PAD_L, PAD_H), bevel=0.0045, seg=6, parent=piv))
    swing += ribbed_pad("PPad_" + k, pcx, pcy, PAD_H, PAD_W - 0.009, PAD_L - 0.008, piv, 15)
    layers["lever_" + k] = {"objects": parts + swing, "anim": ("lever", piv, sgn)}


def area(name, loc, size, power, col, target=(0.25, 0.08, 0.0)):
    ld = bpy.data.lights.new(name, "AREA"); ld.energy = power; ld.shape = "RECTANGLE"; ld.size, ld.size_y = size; ld.color = col
    o = bpy.data.objects.new(name, ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
KEYP = float(os.environ.get("DECK_KEY", "1.0"))
area("Key", (-0.6, -0.12, 0.6), (0.5, 0.4), 14 * KEYP, (1.0, 0.80, 0.60))      # warm soft lamp, left: the vertical ribs catch it
area("Top", (0.25, 0.25, 1.2), (1.4, 0.8), 6 * KEYP, (1.0, 0.88, 0.74))       # broad fill
area("Back", (0.25, 0.9, 0.5), (1.2, 0.3), 1.5 * KEYP, (1.0, 0.85, 0.68))       # rim light on the back edges

# the room the chrome reflects (glossy rays only): bright warm above, dark toward the horizon
bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=6, location=(0.25, 0, 0))
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
v = mul(mul(mr(sep.outputs["Z"], 0.6, 5.5, 0.0, 1.0), mr(sep.outputs["X"], 5.0, -5.0, 0.4, 1.0)), mr(sep.outputs["Y"], 5.0, -5.0, 0.6, 1.0))
pw = nt.nodes.new("ShaderNodeMath"); pw.operation = "POWER"; nt.links.new(v, pw.inputs[0]); pw.inputs[1].default_value = 1.6
ramp = nt.nodes.new("ShaderNodeValToRGB"); nt.links.new(pw.outputs[0], ramp.inputs[0])
ramp.color_ramp.elements[0].color = (0.02, 0.014, 0.01, 1); ramp.color_ramp.elements[1].color = (1.0, 0.82, 0.62, 1)
nt.links.new(ramp.outputs[0], em.inputs["Color"]); em.inputs["Strength"].default_value = float(os.environ.get("DECK_DOME", "4.0"))
nt.links.new(em.outputs[0], o_.inputs[0]); dome.data.materials.append(dm)
dome.visible_camera = False; dome.visible_diffuse = False; dome.visible_shadow = False
dome.visible_transmission = False; dome.visible_volume_scatter = False; dome.visible_glossy = True

allobjs = [o for L in layers.values() for o in L["objects"]]
meta = {"region": REG, "frames": FRAMES, "press_deg": PRESS_DEG, "swing_deg": SWING_DEG, "layers": {}}
ONLY = [x for x in os.environ.get("DECK_LAYERS", "").split(",") if x]
for name, L in layers.items():
    if ONLY and name not in ONLY: continue
    for o in allobjs: o.hide_render = o not in L["objects"]
    for f in range(FRAMES):
        t = f / (FRAMES - 1)
        t = t * t * (3 - 2 * t)                    # ease in/out: the frames are spaced like the motion
        if L["anim"][0] == "pedal":
            L["anim"][1].rotation_euler = (-math.radians(PRESS_DEG) * t, 0, 0)
        else:
            L["anim"][1].rotation_euler = (0, 0, -L["anim"][2] * math.radians(SWING_DEG) * t)
            HALO_EM[name[-1]].inputs["Strength"].default_value = 1.1 * t
        bpy.context.view_layer.update()
        sc.render.filepath = str(out / ("%s_%02d.png" % (name, f)))
        bpy.ops.render.render(write_still=True)
    L["anim"][1].rotation_euler = (0, 0, 0)
    for e_ in HALO_EM.values(): e_.inputs["Strength"].default_value = 0.0
    meta["layers"][name] = FRAMES
for o in allobjs: o.hide_render = False
bpy.ops.wm.save_as_mainfile(filepath=str(out / "deck_3d.blend"))
old = json.loads((out / "render.json").read_text()) if (ONLY and (out / "render.json").exists()) else {"layers": {}}
meta["layers"] = {**old["layers"], **meta["layers"]}
(out / "render.json").write_text(json.dumps(meta, indent=1))
