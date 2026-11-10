"""The main pedal steel as a real 3D model (Joshua 2026-11-10: "make real blender models as close as possible"; approved
goal = final/steel3d/gen/ref_rosewood_1_1.png for the look (dark rosewood + silver aluminium) and his real photo
final/steel3d/real/real_rosewood_a.png for true sizes and spacing).

A single-neck 10-string pedal steel at real dimensions (metres): 24" scale, equal-tempered frets, 11/32" string spacing,
keyhead with ten tuners (five a side), roller nut, raised aluminium neck, pickup, ten-finger changer, endplates, four
telescoping legs, pedal bar with three pedals and pull rods, two knee levers. X right (keyhead at -X), Y away from the
player, Z up; Z = 0 is the cabinet's top surface.

blender --factory-startup -b -P steel_3d.py -- OUT_DIR [samples]
Writes OUT_DIR/steel.png (RGBA, the whole 3200x2000 canvas, shadow-catcher floor) and OUT_DIR/steel.json (canvas
positions of the frets, strings, bridge etc. for the plugin layout).
"""
import bpy, json, math, os, sys
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

args = sys.argv[sys.argv.index("--") + 1:]
out = Path(args[0]).resolve(); out.mkdir(parents=True, exist_ok=True)
SAMPLES = int(args[1]) if len(args) > 1 else 96
CW, CH = 3200, 2000
E = lambda k, d: float(os.environ.get(k, d))

# ---- dimensions
SCALE = 0.6096
BODY_L, BODY_D, APRON = 0.875, 0.205, 0.085
XL, XR = -BODY_L / 2, BODY_L / 2
X_NUT = XL + 0.205; X_BR = X_NUT + SCALE
SP = 0.00873                                   # string spacing
SY = [SP * (4.5 - i) for i in range(10)]       # string 1 (far side) .. 10 (player side)
NECK_W, NECK_H = 0.100, 0.020
Z_STR = NECK_H + 0.0105
LEG = 0.40;     # shortened from a real ~0.60 so the floor and pedals fit above the deck
Z_FLOOR = -(APRON + LEG)
fret_x = lambda n: X_NUT + SCALE * (1 - 2 ** (-n / 12))

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
sc.render.resolution_x = CW; sc.render.resolution_y = CH
sc.render.resolution_percentage = int(E("STEEL_PCT", 100))
sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
sc.view_settings.view_transform = "Standard"

world = bpy.data.worlds.new("Room"); sc.world = world; world.use_nodes = True
bgn = world.node_tree.nodes["Background"]; bgn.inputs[0].default_value = (0.05, 0.035, 0.025, 1); bgn.inputs[1].default_value = 1.0


# ---- materials
def mat(name, base, metal=0.0, rough=0.5, coat=0.0, spec=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.12
    if spec is not None: b.inputs["Specular IOR Level"].default_value = spec
    return m


def wood_mat():
    """rosewood: dark brown with long reddish and near-black streaks along the cabinet, satin lacquer"""
    m = mat("Rosewood", (0.1, 0.04, 0.02), 0.0, 0.32, coat=0.35); nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (2.0, 70.0, 70.0); nt.links.new(tc.outputs["Object"], mp.inputs[0])
    n1 = nt.nodes.new("ShaderNodeTexNoise"); n1.inputs["Scale"].default_value = 3.0; n1.inputs["Detail"].default_value = 6.0
    n1.inputs["Roughness"].default_value = 0.6; nt.links.new(mp.outputs[0], n1.inputs["Vector"])
    mp2 = nt.nodes.new("ShaderNodeMapping"); mp2.inputs["Scale"].default_value = (6.0, 260.0, 260.0)
    nt.links.new(tc.outputs["Object"], mp2.inputs[0])
    n2 = nt.nodes.new("ShaderNodeTexNoise"); n2.inputs["Scale"].default_value = 4.0; n2.inputs["Detail"].default_value = 3.0
    nt.links.new(mp2.outputs[0], n2.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMath"); mix.operation = "MULTIPLY_ADD"; mix.inputs[1].default_value = 0.7
    nt.links.new(n1.outputs["Fac"], mix.inputs[0])
    sc2 = nt.nodes.new("ShaderNodeMath"); sc2.operation = "MULTIPLY"; sc2.inputs[1].default_value = 0.3
    nt.links.new(n2.outputs["Fac"], sc2.inputs[0]); nt.links.new(sc2.outputs[0], mix.inputs[2])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); cr = ramp.color_ramp
    cr.elements[0].position = 0.30; cr.elements[0].color = (0.012, 0.006, 0.004, 1)
    cr.elements[1].position = 0.70; cr.elements[1].color = (0.36, 0.13, 0.05, 1)
    e = cr.elements.new(0.50); e.color = (0.105, 0.038, 0.017, 1)
    nt.links.new(mix.outputs[0], ramp.inputs[0]); nt.links.new(ramp.outputs[0], b.inputs["Base Color"])
    return m


M_WOOD = wood_mat()
M_ALU = mat("Aluminium", (0.72, 0.72, 0.73), 1.0, 0.22)          # polished cast aluminium (satin)
M_ALU_B = mat("AluBrushed", (0.60, 0.60, 0.61), 1.0, 0.42)        # neck, pedal bar
M_CHROME = mat("Chrome", (0.72, 0.71, 0.70), 1.0, 0.07)
M_STEEL = mat("StringSteel", (0.78, 0.77, 0.74), 1.0, 0.22)
M_BLACK = mat("Black", (0.006, 0.006, 0.006), 0.0, 0.5, spec=0.3)
M_FB = mat("Fretboard", (0.010, 0.010, 0.011), 0.0, 0.38, spec=0.4)
M_FRET = mat("FretLine", (0.72, 0.70, 0.66), 0.0, 0.5)
M_RUBBER = mat("Rubber", (0.008, 0.008, 0.008), 0.0, 0.75, spec=0.1)
M_FLOOR = mat("Floor", (0.16, 0.06, 0.035), 0.0, 0.8)
COL = {"red": (0.75, 0.05, 0.03), "cream": (0.86, 0.72, 0.52), "blue": (0.05, 0.25, 0.80), "yellow": (0.95, 0.65, 0.04)}
M_MARK = {k: mat("Mark_" + k, v, 0.0, 0.45) for k, v in COL.items()}


# ---- helpers
def finish(o, material, bevel=0.0, seg=3, smooth=True):
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        bm = o.modifiers.new("bevel", "BEVEL"); bm.width = bevel; bm.segments = seg; bm.limit_method = "ANGLE"
    if smooth:
        for p in o.data.polygons: p.use_smooth = True
        if not bevel: o.data.set_sharp_from_angle(angle=math.radians(35))
    o.data.materials.append(material); return o


def box(name, c, size, material, bevel=0.0, seg=3, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c); o = bpy.context.object; o.name = name
    o.scale = size; o.rotation_euler = rot; return finish(o, material, bevel, seg)


def cyl(name, c, r, depth, material, axis="Z", verts=40, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=1, depth=1, location=c); o = bpy.context.object; o.name = name
    o.scale = (r, r, depth)
    o.rotation_euler = {"Z": (0, 0, 0), "X": (0, math.pi / 2, 0), "Y": (math.pi / 2, 0, 0)}[axis]
    return finish(o, material, bevel, 2)


def rod(name, p0, p1, r, material, verts=24):
    p0, p1 = Vector(p0), Vector(p1); v = p1 - p0
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=1, depth=1, location=(p0 + p1) / 2); o = bpy.context.object
    o.name = name; o.scale = (r, r, v.length); o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(v); return finish(o, material)


def text(name, s, loc, size, material):
    cu = bpy.data.curves.new(name, "FONT"); cu.body = s; cu.size = size; cu.align_x = "CENTER"; cu.align_y = "CENTER"
    o = bpy.data.objects.new(name, cu); sc.collection.objects.link(o); o.location = loc; o.data.materials.append(material)
    return o


# ---- cabinet
box("Cabinet", (0, 0, -APRON / 2), (BODY_L, BODY_D, APRON), M_WOOD, bevel=0.0025, seg=3)
for y in (-BODY_D / 2 - 0.0004, BODY_D / 2 + 0.0004):                       # thin aluminium inlay lines along the aprons
    for z in (-0.016, -APRON + 0.016):
        box("Inlay", (0, y, z), (BODY_L - 0.03, 0.0012, 0.0022), M_ALU)
for sx in (-1, 1):                                                           # endplates: polished aluminium, a little proud
    box("Endplate", (sx * (BODY_L / 2 + 0.009), 0, -APRON / 2 - 0.004), (0.018, BODY_D + 0.014, APRON + 0.014), M_ALU, bevel=0.005, seg=4)
    for sy in (-1, 1):
        for z in (-0.02, -APRON + 0.012):
            cyl("Screw", (sx * (BODY_L / 2 + 0.0182), sy * (BODY_D / 2 - 0.02), z), 0.0032, 0.0012, M_CHROME, "X", 16)

# ---- neck, fretboard
x0n, x1n = X_NUT - 0.006, X_BR - 0.020
box("Neck", ((x0n + x1n) / 2, 0, NECK_H / 2), (x1n - x0n, NECK_W, NECK_H), M_ALU_B, bevel=0.002, seg=3)
fb0, fb1 = X_NUT + 0.004, fret_x(24) + 0.013
box("Fretboard", ((fb0 + fb1) / 2, 0, NECK_H + 0.0008), (fb1 - fb0, NECK_W - 0.008, 0.0016), M_FB)
ZF_ = NECK_H + 0.00165
for n in range(1, 25):
    box("Fret", (fret_x(n), 0, ZF_), (0.0011, NECK_W - 0.008, 0.0003), M_FRET, smooth=False) if False else \
        box("Fret", (fret_x(n), 0, ZF_), (0.0011, NECK_W - 0.008, 0.0003), M_FRET)
for y in SY:                                                                 # the fretboard's printed string guide lines
    box("Guide", ((fb0 + fb1) / 2, y, ZF_ - 0.0001), (fb1 - fb0, 0.0004, 0.0002), mat("GuideLine", (0.22, 0.21, 0.20), 0.0, 0.5))
MARKS = {3: ["red"], 5: ["cream"], 7: ["blue"], 9: ["cream"], 12: ["yellow", "yellow"], 15: ["red"], 17: ["cream"],
         19: ["blue"], 21: ["blue"], 24: ["yellow", "yellow"]}
for n, cols in MARKS.items():
    xm = (fret_x(n - 1) + fret_x(n)) / 2
    for j, c in enumerate(cols):
        ym = 0.004 if len(cols) == 1 else (0.022, -0.016)[j]
        box("Marker", (xm, ym, ZF_ + 0.0001), (0.0062, 0.0062, 0.0003), M_MARK[c], rot=(0, 0, math.pi / 4))
    t = text("Num%d" % n, str(n), (xm, -NECK_W / 2 + 0.0105, ZF_ + 0.0002), 0.0088, mat("Num%d" % n, (0.9, 0.88, 0.82), 0.0, 0.5))

# ---- nut (rollers), strings, pickup, changer
box("NutBlock", (X_NUT, 0, NECK_H / 2 + 0.004), (0.011, NECK_W + 0.006, NECK_H + 0.008), M_ALU, bevel=0.0015)
GA = [0.00028, 0.00030, 0.00028, 0.00036, 0.00043, 0.00050, 0.00066, 0.00076, 0.00086, 0.00094]   # E9 gauges (radius)
KH_X0 = XL + 0.022                                                           # keyhead: left end
for i, y in enumerate(SY):
    cyl("Roller", (X_NUT, y, Z_STR - 0.0035), 0.0035, 0.0052, M_CHROME, "Y", 24)
    far = i < 5; k = i if far else 9 - i                                     # outer strings to the tuners nearest the nut
    xs = X_NUT - 0.040 - k * 0.030                                           # this string's tuner shaft
    rod("String", (xs, y, Z_STR - 0.004), (X_NUT, y, Z_STR), GA[i] * 1.5, M_STEEL, 12)
    rod("String", (X_NUT, y, Z_STR), (X_BR, y, Z_STR), GA[i] * 1.5, M_STEEL, 12)
    yr = (1 if far else -1) * (NECK_W / 2 + 0.004)                           # the rail it mounts in
    rod("Shaft", (xs, -NECK_W / 2 - 0.004, Z_STR - 0.0075), (xs, NECK_W / 2 + 0.004, Z_STR - 0.0075), 0.0028, M_CHROME, 20)
    yo = yr + (1 if far else -1) * 0.016                                     # tuner button outside the rail
    rod("KeyStem", (xs, yr, Z_STR - 0.0075), (xs, yo, Z_STR - 0.0075), 0.0022, M_CHROME, 16)
    cyl("KeyButton", (xs, yo + (1 if far else -1) * 0.004, Z_STR - 0.0035), 0.0068, 0.0130, M_ALU, "Z", 32, bevel=0.002)
    # changer finger: a thin plate with a round top the string bends over
    cyl("Finger", (X_BR + 0.004, y, Z_STR - 0.0085), 0.0085, 0.0058, M_ALU, "Y", 32)
    box("FingerBody", (X_BR + 0.012, y, Z_STR - 0.017), (0.022, 0.0058, 0.017), M_ALU, bevel=0.001)
    rod("StringEnd", (X_BR + 0.004, y, Z_STR), (X_BR + 0.019, y, Z_STR - 0.012), GA[i] * 1.5, M_STEEL, 12)
    cyl("Pole", (X_BR - 0.062, y, NECK_H + 0.0062), 0.0022, 0.002, M_CHROME, "Z", 16)
for s in (1, -1):                                                            # keyhead: two cast rails joined at the end
    box("KeyRail", ((KH_X0 + X_NUT - 0.006) / 2, s * (NECK_W / 2 + 0.004), (Z_STR + 0.002) / 2), (X_NUT - 0.006 - KH_X0, 0.0085, Z_STR + 0.002), M_ALU, bevel=0.0025, seg=4)
box("KeyEnd", (KH_X0 + 0.004, 0, (Z_STR + 0.002) / 2), (0.0085, NECK_W + 0.0165, Z_STR + 0.002), M_ALU, bevel=0.0025, seg=4)
box("Pickup", (X_BR - 0.062, 0, NECK_H + 0.003), (0.020, NECK_W - 0.006, 0.0065), M_BLACK, bevel=0.0015)
box("PickupWell", (X_BR - 0.062, 0, NECK_H + 0.0003), (0.026, NECK_W - 0.002, 0.001), M_BLACK)
box("ChangerBase", (X_BR + 0.014, 0, 0.006), (0.050, NECK_W + 0.014, 0.012), M_ALU, bevel=0.002)
for s in (1, -1):
    box("ChangerCheek", (X_BR + 0.012, s * (NECK_W / 2 + 0.0045), Z_STR / 2), (0.040, 0.007, Z_STR + 0.003), M_ALU, bevel=0.003, seg=4)
rod("ChangerAxle", (X_BR + 0.004, -NECK_W / 2 - 0.009, Z_STR - 0.0085), (X_BR + 0.004, NECK_W / 2 + 0.009, Z_STR - 0.0085), 0.0032, M_CHROME)

# ---- legs: chrome upper tube, thinner lower tube with a clutch collar, rubber foot; splayed a little
FEET = {}
for sx in (-1, 1):
    for sy in (-1, 1):
        top = Vector((sx * (BODY_L / 2 - 0.022), sy * (BODY_D / 2 - 0.028), -APRON - 0.004))
        foot = Vector((sx * (BODY_L / 2 + 0.012), sy * (BODY_D / 2 + (0.070 if sy < 0 else 0.055)), Z_FLOOR + 0.012))
        mid = top.lerp(foot, 0.56)
        rod("LegUpper", top, mid, 0.0115, M_CHROME, 32); rod("LegLower", mid, foot, 0.0080, M_CHROME, 32)
        rod("LegCollar", mid - (foot - top).normalized() * 0.012, mid + (foot - top).normalized() * 0.010, 0.0140, M_CHROME, 32)
        rod("LegFoot", foot, foot + (foot - top).normalized() * 0.014, 0.0105, M_RUBBER, 24)
        FEET[(sx, sy)] = foot

# ---- pedal bar on the front legs, three pedals, pull rods; knee levers
fl, fr = FEET[(-1, -1)], FEET[(1, -1)]
BAR_Z = Z_FLOOR + 0.030; BAR_Y = fl.y + (BAR_Z - fl.z) * 0.0 - 0.004
box("PedalBar", (0, BAR_Y, BAR_Z), (fr.x - fl.x + 0.03, 0.028, 0.014), M_ALU_B, bevel=0.003, seg=3)
for f in (fl, fr):
    box("BarClamp", (f.x, BAR_Y, BAR_Z), (0.034, 0.046, 0.030), M_ALU, bevel=0.004, seg=3)
PEDAL_X = [-0.115, -0.045, 0.025]
for k, px in zip("ABC", PEDAL_X):
    box("PedalHinge_" + k, (px, BAR_Y - 0.008, BAR_Z + 0.014), (0.036, 0.018, 0.012), M_ALU, bevel=0.002)
    box("Pedal_" + k, (px, BAR_Y - 0.060, BAR_Z + 0.026), (0.027, 0.092, 0.008), M_ALU, bevel=0.0035, seg=4, rot=(math.radians(-12), 0, 0))
    rod("PullRod_" + k, (px, BAR_Y - 0.030, BAR_Z + 0.026), (px, -BODY_D / 2 + 0.050, -APRON), 0.0024, M_CHROME, 16)
for k, kx in (("L", -0.205), ("R", 0.115)):
    box("KneeBracket_" + k, (kx, -BODY_D / 2 + 0.040, -APRON - 0.008), (0.040, 0.024, 0.016), M_ALU, bevel=0.002)
    rod("KneeArm_" + k, (kx, -BODY_D / 2 + 0.040, -APRON - 0.012), (kx, -BODY_D / 2 + 0.034, -APRON - 0.120), 0.0050, M_ALU, 20)
    box("KneePad_" + k, (kx, -BODY_D / 2 + 0.030, -APRON - 0.105), (0.016, 0.024, 0.085), M_BLACK, bevel=0.005, seg=4)

# ---- tone bar: polished steel bullet bar lying across the strings (a separate live layer later)
BAR_FRET = E("STEEL_BARFRET", 9.6)
if BAR_FRET >= 0:
    bx_ = fret_x(BAR_FRET); br = 0.0105
    cyl("ToneBar", (bx_, -0.004, Z_STR + br), br, 0.083, M_CHROME, "Y", 48, bevel=0.003)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=br, location=(bx_, 0.0375, Z_STR + br))
    finish(bpy.context.object, M_CHROME)

# ---- floor (shadow catcher) and what the metal reflects
bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0.5, Z_FLOOR)); fl_ = bpy.context.object; fl_.name = "Floor"
fl_.data.materials.append(M_FLOOR); fl_.is_shadow_catcher = True

# ---- camera: perspective, front-on from the player's side, looking down
TH = math.radians(E("STEEL_PITCH", 64.0)); DIST = E("STEEL_DIST", 1.5); PXM = E("STEEL_PXM", 1265.0) * 2   # canvas px / m at the neck
AIM = Vector((0, 0, NECK_H)); AIM_Y = E("STEEL_AIMY", 206.0) * 2
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.location = AIM + Vector((0, -math.cos(TH), math.sin(TH))) * DIST
cam.rotation_euler = (math.pi / 2 - TH, 0, 0)
cam.data.sensor_fit = "HORIZONTAL"; cam.data.sensor_width = 36.0; cam.data.lens = PXM * DIST * 36.0 / CW
cam.data.clip_start = 0.05; cam.data.clip_end = 30
bpy.context.view_layer.update()


def proj(p):
    v = world_to_camera_view(sc, cam, Vector(p)); return v.x * CW, (1 - v.y) * CH


y_a = proj(AIM)[1]; cam.data.shift_y = 0.1; bpy.context.view_layer.update(); y_b = proj(AIM)[1]
cam.data.shift_y = 0.1 * (AIM_Y - y_a) / (y_b - y_a); bpy.context.view_layer.update()


# ---- lights: the room's warm lamp upper left, a broad soft top light, a low front fill
def area(name, loc, size, power, col, target=(0, 0, -0.1)):
    ld = bpy.data.lights.new(name, "AREA"); ld.energy = power; ld.shape = "RECTANGLE"; ld.size, ld.size_y = size; ld.color = col
    o = bpy.data.objects.new(name, ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
KP = E("STEEL_LIGHT", 1.0)
area("Key", (-1.1, -0.5, 1.5), (0.9, 0.7), 26 * KP, (1.0, 0.84, 0.66))
area("Top", (0.2, 0.3, 1.9), (2.2, 1.2), 14 * KP, (1.0, 0.90, 0.78))
area("Fill", (0.6, -1.6, 0.5), (1.6, 0.8), 4 * KP, (1.0, 0.86, 0.72))

bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=9, location=(0, 0, 0))
dome = bpy.context.object; dome.name = "RoomDome"
for p in dome.data.polygons: p.use_smooth = True
dm = bpy.data.materials.new("RoomDome"); dm.use_nodes = True; nt = dm.node_tree; nt.nodes.clear()
o_ = nt.nodes.new("ShaderNodeOutputMaterial"); em = nt.nodes.new("ShaderNodeEmission")
geo = nt.nodes.new("ShaderNodeNewGeometry"); sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(geo.outputs["Position"], sep.inputs[0])
ramp = nt.nodes.new("ShaderNodeValToRGB")
mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[1].default_value = -3.0; mr.inputs[2].default_value = 8.0
nt.links.new(sep.outputs["Z"], mr.inputs[0]); nt.links.new(mr.outputs[0], ramp.inputs[0])
ramp.color_ramp.elements[0].color = (0.10, 0.045, 0.025, 1)                  # rug / dark room toward the horizon
ramp.color_ramp.elements[1].color = (1.0, 0.85, 0.68, 1)                     # warm ceiling
nt.links.new(ramp.outputs[0], em.inputs["Color"]); em.inputs["Strength"].default_value = E("STEEL_DOME", 0.9)
nt.links.new(em.outputs[0], o_.inputs[0]); dome.data.materials.append(dm)
dome.visible_camera = False; dome.visible_diffuse = False; dome.visible_shadow = False; dome.visible_glossy = True

# ---- layout numbers for the plugin (canvas px)
meta = {"frets": [[n, round(proj((fret_x(n) if n else X_NUT, 0, Z_STR))[0], 2)] for n in range(0, 25)],
        "stringY": [round(proj((0, y, Z_STR))[1], 2) for y in SY],
        "bridgeX": round(proj((X_BR, 0, Z_STR))[0], 2),
        "body": [round(v, 1) for v in (proj((XL, 0, 0))[0], proj((0, BODY_D / 2, 0))[1], proj((XR, 0, 0))[0], proj((0, -BODY_D / 2, -APRON))[1])],
        "floorFrontY": round(proj((0, fl.y, Z_FLOOR))[1], 1),
        "pedals": {k: [round(v, 1) for v in proj((px, BAR_Y - 0.06, BAR_Z + 0.03))] for k, px in zip("ABC", PEDAL_X)},
        "camera": {"pitch_deg": math.degrees(TH), "dist": DIST, "px_per_m": PXM}}
(out / "steel.json").write_text(json.dumps(meta, indent=1))
print("LAYOUT", json.dumps({k: meta[k] for k in ("stringY", "bridgeX", "body", "floorFrontY")}))
sc.render.filepath = str(out / "steel.png")
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out / "steel_3d.blend"))
