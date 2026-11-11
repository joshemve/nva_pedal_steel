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
BODY_L, BODY_D, APRON = 0.917, 0.193, 0.075      # measured off the approved reference
XL, XR = -BODY_L / 2, BODY_L / 2
X_NUT = XL + 0.230; X_BR = X_NUT + SCALE
SP = 0.00873                                   # string spacing
SY = [SP * (4.5 - i) for i in range(10)]       # string 1 (far side) .. 10 (player side)
NECK_W, NECK_H = 0.141, 0.014
Z_STR = NECK_H + 0.0105
LEG = 0.225;     # shortened from a real ~0.60 so the floor and pedals fit above the deck
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
sc.view_settings.view_transform = "Standard"; sc.cycles.filter_width = 1.0

world = bpy.data.worlds.new("Room"); sc.world = world; world.use_nodes = True
bgn = world.node_tree.nodes["Background"]; bgn.inputs[0].default_value = (0.035, 0.025, 0.018, 1); bgn.inputs[1].default_value = 1.0


# ---- materials
def mat(name, base, metal=0.0, rough=0.5, coat=0.0, spec=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.04
    if spec is not None: b.inputs["Specular IOR Level"].default_value = spec
    return m


FINISH = os.environ.get("STEEL_FINISH", "rosewood")
# (darkest, middle, lightest) of the grain, lacquer roughness
WOODS = {"rosewood": ((0.008, 0.003, 0.002), (0.055, 0.011, 0.004), (0.30, 0.042, 0.010)),
         "maple": ((0.36, 0.20, 0.07), (0.62, 0.40, 0.17), (0.82, 0.60, 0.30)),
         "koa": ((0.06, 0.018, 0.006), (0.28, 0.10, 0.025), (0.62, 0.28, 0.07))}


def wood_mat():
    """rosewood: dark brown with long reddish and near-black streaks along the cabinet, satin lacquer"""
    m = mat("Rosewood", (0.1, 0.04, 0.02), 0.0, 0.22, coat=0.8); nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.6, 48.0, 48.0); nt.links.new(tc.outputs["Object"], mp.inputs[0])
    n1 = nt.nodes.new("ShaderNodeTexNoise"); n1.inputs["Scale"].default_value = 3.0; n1.inputs["Detail"].default_value = 6.0
    n1.inputs["Roughness"].default_value = 0.75; nt.links.new(mp.outputs[0], n1.inputs["Vector"])
    mp2 = nt.nodes.new("ShaderNodeMapping"); mp2.inputs["Scale"].default_value = (6.0, 260.0, 260.0)
    nt.links.new(tc.outputs["Object"], mp2.inputs[0])
    n2 = nt.nodes.new("ShaderNodeTexNoise"); n2.inputs["Scale"].default_value = 4.0; n2.inputs["Detail"].default_value = 3.0
    nt.links.new(mp2.outputs[0], n2.inputs["Vector"])
    # broad, cloudy figure under the streaks (the reference's rosewood is blotchy, not evenly striped)
    mp3 = nt.nodes.new("ShaderNodeMapping"); mp3.inputs["Scale"].default_value = (1.0, 5.0, 5.0)
    nt.links.new(tc.outputs["Object"], mp3.inputs[0])
    n3 = nt.nodes.new("ShaderNodeTexNoise"); n3.inputs["Scale"].default_value = 2.2; n3.inputs["Detail"].default_value = 3.0
    n3.inputs["Distortion"].default_value = 0.8; nt.links.new(mp3.outputs[0], n3.inputs["Vector"])
    n1.inputs["Distortion"].default_value = 0.5
    def lin(sock, k, add=None):
        n = nt.nodes.new("ShaderNodeMath"); n.operation = "MULTIPLY_ADD"; n.inputs[1].default_value = k
        nt.links.new(sock, n.inputs[0])
        if add is None: n.inputs[2].default_value = 0.0
        else: nt.links.new(add, n.inputs[2])
        return n.outputs[0]
    mix = nt.nodes.new("ShaderNodeMath"); mix.operation = "ADD"; mix.inputs[1].default_value = 0.0
    nt.links.new(lin(n3.outputs["Fac"], 0.40, lin(n1.outputs["Fac"], 0.40, lin(n2.outputs["Fac"], 0.20))), mix.inputs[0])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); cr = ramp.color_ramp
    wd = WOODS.get(FINISH, WOODS["rosewood"])
    cr.elements[0].position = 0.42; cr.elements[0].color = (*wd[0], 1)
    cr.elements[1].position = 0.68; cr.elements[1].color = (*wd[2], 1)
    e = cr.elements.new(0.54); e.color = (*wd[1], 1)
    nt.links.new(mix.outputs[0], ramp.inputs[0]); nt.links.new(ramp.outputs[0], b.inputs["Base Color"])
    # the lacquer is not factory-new: fine swirls and dull patches in the gloss
    mp4 = nt.nodes.new("ShaderNodeMapping"); mp4.inputs["Scale"].default_value = (14.0, 14.0, 14.0); nt.links.new(tc.outputs["Object"], mp4.inputs[0])
    n4 = nt.nodes.new("ShaderNodeTexNoise"); n4.inputs["Scale"].default_value = 3.0; n4.inputs["Detail"].default_value = 6.0
    nt.links.new(mp4.outputs[0], n4.inputs["Vector"])
    cr_ = nt.nodes.new("ShaderNodeMapRange"); cr_.inputs[1].default_value = 0.35; cr_.inputs[2].default_value = 0.75
    cr_.inputs[3].default_value = 0.03; cr_.inputs[4].default_value = 0.16
    nt.links.new(n4.outputs["Fac"], cr_.inputs[0]); nt.links.new(cr_.outputs[0], b.inputs["Coat Roughness"])
    return m


def flake_mat(name, base, cell, lo, hi, metal):
    """sparkle / pearloid top coats: small cells of varying brightness under a clear coat"""
    m = mat(name, base, metal, 0.25, coat=1.0); nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = cell
    nt.links.new(tc.outputs["Object"], v.inputs["Vector"])
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[3].default_value = lo; mr.inputs[4].default_value = hi
    nt.links.new(v.outputs["Color"], mr.inputs[0])
    mx_ = nt.nodes.new("ShaderNodeMix"); mx_.data_type = "RGBA"; mx_.blend_type = "MULTIPLY"; mx_.inputs[0].default_value = 1.0
    mx_.inputs[6].default_value = (*base, 1); nt.links.new(mr.outputs[0], mx_.inputs[7]); nt.links.new(mx_.outputs[2], b.inputs["Base Color"])
    return m


M_WOOD = (flake_mat("RedSparkle", (0.55, 0.012, 0.012), 900.0, 0.35, 1.6, 0.7) if FINISH == "redsparkle" else
          flake_mat("Pearloid", (0.86, 0.82, 0.74), 55.0, 0.72, 1.05, 0.0) if FINISH == "pearloid" else wood_mat())
M_ALU = mat("Aluminium", (0.74, 0.73, 0.72), 1.0, 0.30)   # satin cast aluminium (keyhead, changer, endplates)   # polished: crisp reflections (Joshua: ours felt hazed)          # polished cast aluminium (satin)
M_ALU_B = mat("AluBrushed", (0.60, 0.60, 0.61), 1.0, 0.20)        # neck, pedal bar
M_CHROME = mat("Chrome", (0.74, 0.73, 0.72), 1.0, 0.04)
M_SPRING = mat("Spring", (0.02, 0.02, 0.02), 1.0, 0.5)
M_COIL = mat("Coil", (0.32, 0.31, 0.30), 1.0, 0.30)
M_POL = mat("PolishedAlu", (0.80, 0.79, 0.78), 1.0, 0.12)
M_BRASS = mat("Brass", (0.75, 0.60, 0.32), 1.0, 0.25)
M_STEEL = mat("StringSteel", (0.62, 0.61, 0.58), 1.0, 0.28)
M_BLACK = mat("Black", (0.014, 0.014, 0.014), 0.0, 0.42, spec=0.4)
M_FB = mat("Fretboard", (0.004, 0.004, 0.005), 0.0, 0.10, spec=0.5)
M_FRET = mat("FretLine", (0.78, 0.76, 0.72), 0.0, 0.5)
M_RUBBER = mat("Rubber", (0.022, 0.022, 0.022), 0.0, 0.55, spec=0.35)
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


def ecyl(name, c, rx, rz, depth, material, verts=48):
    """an elliptical cylinder lying along Y (rx along X, rz up)"""
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=1, depth=1, location=c); o = bpy.context.object; o.name = name
    o.scale = (rx, rz, depth); o.rotation_euler = (math.pi / 2, 0, 0); return finish(o, material, 0.0008, 2)


def rod(name, p0, p1, r, material, verts=24):
    p0, p1 = Vector(p0), Vector(p1); v = p1 - p0
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=1, depth=1, location=(p0 + p1) / 2); o = bpy.context.object
    o.name = name; o.scale = (r, r, v.length); o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(v); return finish(o, material)


def prism(name, pts, z0, z1, material):
    """a flat polygon (x, y points, counter-clockwise) extruded from z0 to z1"""
    n = len(pts); vs = [(x, y, z0) for x, y in pts] + [(x, y, z1) for x, y in pts]
    fs = [tuple(range(n))[::-1], tuple(range(n, 2 * n))] + [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    me = bpy.data.meshes.new(name); me.from_pydata(vs, [], fs); me.update()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); me.materials.append(material); return o


def wear(m, amount=0.07, scale=38.0):
    """used metal: roughness and tone vary a little across the surface (fingerprints, polish marks)"""
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]; base = b.inputs["Roughness"].default_value
    tc = nt.nodes.new("ShaderNodeTexCoord"); n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale; n.inputs["Detail"].default_value = 5.0; n.inputs["Roughness"].default_value = 0.65
    nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[1].default_value = 0.30; mr.inputs[2].default_value = 0.75
    mr.inputs[3].default_value = max(base - amount * 0.5, 0.02); mr.inputs[4].default_value = base + amount
    nt.links.new(n.outputs["Fac"], mr.inputs[0]); nt.links.new(mr.outputs[0], b.inputs["Roughness"])
    # handling marks: the tone dips a little where the surface is rougher
    tn = nt.nodes.new("ShaderNodeMapRange"); tn.inputs[1].default_value = 0.35; tn.inputs[2].default_value = 0.80
    tn.inputs[3].default_value = 1.0; tn.inputs[4].default_value = 1.0 - amount * 1.6
    nt.links.new(n.outputs["Fac"], tn.inputs[0])
    mx_ = nt.nodes.new("ShaderNodeMix"); mx_.data_type = "RGBA"; mx_.blend_type = "MULTIPLY"; mx_.inputs[0].default_value = 1.0
    mx_.inputs[6].default_value = b.inputs["Base Color"].default_value
    nt.links.new(tn.outputs[0], mx_.inputs[7]); nt.links.new(mx_.outputs[2], b.inputs["Base Color"])
    return m


def text(name, s, loc, size, material):
    cu = bpy.data.curves.new(name, "FONT"); cu.body = s; cu.size = size; cu.align_x = "CENTER"; cu.align_y = "CENTER"
    o = bpy.data.objects.new(name, cu); sc.collection.objects.link(o); o.location = loc; o.data.materials.append(material)
    return o


wear(M_ALU, 0.11, 30.0); wear(M_ALU_B, 0.06, 60.0); wear(M_CHROME, 0.025, 45.0)

# ---- cabinet
box("Cabinet", (0, 0, -APRON / 2), (BODY_L, BODY_D, APRON), M_WOOD, bevel=0.003, seg=3)
box("TopTrim", (0, -BODY_D / 2 + 0.0012, 0.0004), (BODY_L - 0.004, 0.0020, 0.0014), M_ALU, bevel=0.0005)       # front top edge strip
EP_T = 0.025
for sx in (-1, 1):                                                           # endplates: polished cast aluminium, proud of the cabinet
    xe = sx * (BODY_L / 2 + EP_T / 2 - 0.004)
    box("Endplate", (xe, 0, -APRON / 2 - 0.004), (EP_T, BODY_D + 0.014, APRON + 0.022), M_ALU, bevel=0.006, seg=5)
    box("EndLip", (sx * (BODY_L / 2 - 0.004), 0, 0.0025), (0.026, BODY_D + 0.014, 0.005), M_ALU, bevel=0.002, seg=3)
    for z in (-0.016, -APRON + 0.006):                                       # screws on the front face
        cyl("Screw", (xe, -BODY_D / 2 - 0.0072, z), 0.0048, 0.0018, M_CHROME, "Y", 20, bevel=0.0006)
        box("ScrewSlot", (xe, -BODY_D / 2 - 0.0082, z), (0.0070, 0.0006, 0.0010), M_BLACK, rot=(0, math.radians(35), 0))
    for sy in (-1, 1):
        cyl("LipScrew", (sx * (BODY_L / 2 - 0.006), sy * (BODY_D / 2 - 0.016), 0.0052), 0.0030, 0.0012, M_CHROME, "Z", 16)

# ---- neck plate (runs from the keyhead to the changer), fretboard
FB_W = 0.122
X_GUIDE = X_NUT - 0.098                                                      # the roller bar where the keyhead ends
x0n, x1n = X_NUT - 0.009, X_BR + 0.004
for sy in (-1, 1):                                                           # between keyhead and nut: an open frame over the wood
    box("NeckRail", ((X_GUIDE + X_NUT) / 2, sy * (NECK_W / 2 - 0.0045), NECK_H / 2), (X_NUT - X_GUIDE + 0.006, 0.009, NECK_H), M_ALU_B, bevel=0.002)
box("Neck", ((x0n + x1n) / 2, 0, NECK_H / 2), (x1n - x0n, NECK_W, NECK_H), M_ALU_B, bevel=0.0025, seg=3)
for xs_ in (X_NUT + 0.02, fret_x(6.5), fret_x(14.5), X_BR - 0.085):
    for sy in (-1, 1):
        cyl("NeckScrew", (xs_, sy * (NECK_W / 2 - 0.0045), NECK_H + 0.0004), 0.0028, 0.0012, M_CHROME, "Z", 16)
fb0, fb1 = X_NUT + 0.004, X_NUT + 0.495
box("Fretboard", ((fb0 + fb1) / 2, 0, NECK_H + 0.0008), (fb1 - fb0, FB_W, 0.0016), M_FB, bevel=0.0006)
ZF_ = NECK_H + 0.00165
M_GUIDE = mat("GuideLine", (0.42, 0.41, 0.39), 0.0, 0.5)
box("FbBorder", ((fb0 + fb1) / 2, 0, ZF_ - 0.00012), (fb1 - fb0 - 0.004, FB_W - 0.004, 0.0002), M_FRET)
box("FbInner", ((fb0 + fb1) / 2, 0, ZF_ - 0.00006), (fb1 - fb0 - 0.0052, FB_W - 0.0052, 0.0002), M_FB)
FRET_Y0, FRET_Y1 = -FB_W / 2 + 0.019, FB_W / 2 - 0.004                       # fret lines stop above the number strip
for n in range(1, 25):
    box("Fret", (fret_x(n), (FRET_Y0 + FRET_Y1) / 2, ZF_), (0.0007, FRET_Y1 - FRET_Y0, 0.0003), M_FRET)
    box("FretTick", (fret_x(n), -FB_W / 2 + 0.011, ZF_), (0.0007, 0.016, 0.0003), M_FRET)
for y in SY:                                                                 # the fretboard's printed string guide lines
    box("Guide", ((fb0 + fb1) / 2, y, ZF_ - 0.00002), (fb1 - fb0 - 0.006, 0.0004, 0.0002), M_GUIDE)
MARKS = {3: ["red"], 5: ["cream"], 7: ["blue"], 9: ["cream"], 12: ["yellow", "yellow"], 15: ["red"], 17: ["cream"],
         19: ["blue"], 21: ["blue"], 24: ["yellow", "yellow"]}
M_NUM = mat("FretNumber", (0.92, 0.90, 0.84), 0.0, 0.5)
for n, cols in MARKS.items():
    xm = (fret_x(n - 1) + fret_x(n)) / 2
    for j, c in enumerate(cols):
        ym = 0.004 if len(cols) == 1 else (0.024, -0.014)[j]
        box("Marker", (xm, ym, ZF_ + 0.0001), (0.0105, 0.0105, 0.0003), M_MARK[c], rot=(0, 0, math.pi / 4))
    text("Num%d" % n, str(n), (xm, -FB_W / 2 + 0.0105, ZF_ + 0.0002), 0.0120, M_NUM)

# ---- nut roller bar, keyhead roller bar, strings, keyhead with upright tuners, pickup, changer
GA = [0.00028, 0.00030, 0.00028, 0.00036, 0.00043, 0.00050, 0.00066, 0.00076, 0.00086, 0.00094]   # E9 gauges (radius)
SR = 0.95                                                                    # drawn a little heavier so they read on screen
for xb, r_ in ((X_NUT, 0.0058), (X_GUIDE, 0.0062)):
    rod("RollerBar", (xb, -NECK_W / 2 + 0.002, Z_STR - r_), (xb, NECK_W / 2 - 0.002, Z_STR - r_), r_, M_CHROME, 40)
    for sy in (-1, 1):
        cyl("RollerEnd", (xb, sy * (NECK_W / 2 - 0.001), Z_STR - r_), r_ + 0.0016, 0.005, M_CHROME, "Y", 32, bevel=0.001)
KH_X0 = XL + 0.012; KH_Z = 0.015                                             # keyhead: a cast frame, slightly wider at the left
RAIL0 = lambda s: Vector((X_GUIDE - 0.002, s * (NECK_W / 2 - 0.006), KH_Z / 2))
RAIL1 = lambda s: Vector((KH_X0 + 0.010, s * 0.036, KH_Z / 2))       # narrow at the left end, widening toward the nut (reference)
# one casting: a tapered plate with a rounded left end and the window cut out of it
WR = 0.021                                                                   # rail width
yL, yR = 0.036 + WR / 2, NECK_W / 2 - 0.006 + WR / 2
xl, xr = KH_X0 - 0.002, X_GUIDE + 0.004
outer = [(xl + 0.012, -yL), (xr, -yR), (xr, yR), (xl + 0.012, yL), (xl + 0.003, yL - 0.010), (xl, yL - 0.024), (xl, -yL + 0.024), (xl + 0.003, -yL + 0.010)]
kh = prism("Keyhead", outer, 0.0, KH_Z, M_ALU)
inner = [(xl + WR + 0.006, -yL + WR + 0.004), (xr - 0.012, -yR + WR), (xr - 0.012, yR - WR), (xl + WR + 0.006, yL - WR - 0.004), (xl + WR, yL - WR - 0.012), (xl + WR, -yL + WR + 0.012)]
cut = prism("KeyheadWindow", inner, -0.01, KH_Z + 0.01, M_ALU); cut.hide_render = True; cut.display_type = "WIRE"
bo = kh.modifiers.new("window", "BOOLEAN"); bo.operation = "DIFFERENCE"; bo.object = cut; bo.solver = "EXACT"
bv = kh.modifiers.new("bevel", "BEVEL"); bv.width = 0.0045; bv.segments = 6; bv.limit_method = "ANGLE"; bv.angle_limit = math.radians(40)
for pl in kh.data.polygons: pl.use_smooth = True
for i, y in enumerate(SY):
    far = i < 5; k = i if far else 9 - i; s = 1 if far else -1                # outer strings' tuners nearest the nut
    t = 0.16 + k * 0.165
    post = RAIL0(s).lerp(RAIL1(s), t)
    # strings run across the open frame to its left end, drawing together a little
    rod("String", (KH_X0 + 0.016, y * 0.80, KH_Z * 0.70), (X_GUIDE, y, Z_STR), GA[i] * SR, M_STEEL, 12)
    # tuner: stacked collars on the rail; the far row's keys stand up, the near row's reach out toward the player
    cyl("TunerBase", (post.x, post.y, KH_Z + 0.0020), 0.0080, 0.0040, M_CHROME, "Z", 28, bevel=0.0010)
    cyl("TunerCollar", (post.x, post.y, KH_Z + 0.0062), 0.0062, 0.0045, M_ALU, "Z", 24, bevel=0.0010)
    cyl("TunerCap", (post.x, post.y, KH_Z + 0.0100), 0.0046, 0.0035, M_CHROME, "Z", 20, bevel=0.0008)
    if far:
        rod("TunerStem", (post.x, post.y, KH_Z + 0.010), (post.x, post.y + 0.003, KH_Z + 0.022), 0.0026, M_CHROME, 16)
        cyl("KeyButton", (post.x, post.y + 0.003, KH_Z + 0.0245), 0.0090, 0.0056, M_ALU, "Z", 36, bevel=0.0024)
    else:
        rod("TunerStem", (post.x, post.y - 0.006, KH_Z * 0.6), (post.x - 0.004, post.y - 0.026, KH_Z * 0.35), 0.0026, M_CHROME, 16)
        cyl("KeyButton", (post.x - 0.004, post.y - 0.029, KH_Z * 0.35), 0.0090, 0.0056, M_ALU, "Z", 36, bevel=0.0024)
    rod("String", (X_GUIDE, y, Z_STR), (X_BR + 0.006, y, Z_STR), GA[i] * SR, M_STEEL, 12)
    # changer finger: a long humped lever the string hooks into, then its square nut and coil spring in the tray
    ecyl("Finger", (X_BR + 0.010, y, Z_STR - 0.0130), 0.0150, 0.0130, 0.0068, M_POL)
    box("FingerArm", (X_BR + 0.030, y, Z_STR - 0.0120), (0.030, 0.0062, 0.0075), M_POL, bevel=0.002, rot=(0, math.radians(14), 0))
    cyl("FingerEye", (X_BR - 0.0085, y, Z_STR - 0.0125), 0.0060, 0.0070, M_POL, "Y", 24, bevel=0.0008)
    cyl("FingerEyeHole", (X_BR - 0.0085, y, Z_STR - 0.0125), 0.0026, 0.0074, M_BLACK, "Y", 12)
    box("TuneNut", (X_BR + 0.0470, y, Z_STR - 0.0165), (0.0080, 0.0080, 0.0090), M_POL, bevel=0.0012)
    cyl("Spring", (X_BR + 0.0605, y, Z_STR - 0.0170), 0.0030, 0.0200, M_SPRING, "X", 14)
    for q in range(8):
        cyl("Coil", (X_BR + 0.0525 + q * 0.0025, y, Z_STR - 0.0170), 0.0037, 0.0011, M_COIL, "X", 14)
    cyl("Pole", (X_BR - 0.038, y, NECK_H + 0.0100), 0.0027, 0.002, M_BRASS, "Z", 16)
box("Pickup", (X_BR - 0.038, 0, NECK_H + 0.0048), (0.026, FB_W - 0.004, 0.0100), M_BLACK, bevel=0.0035, seg=5)
box("ChangerTray", (X_BR + 0.038, 0, 0.004), (0.074, NECK_W - 0.006, 0.008), M_BLACK)
box("TrayWall", (X_BR + 0.0735, 0, Z_STR / 2 - 0.004), (0.005, NECK_W - 0.002, Z_STR - 0.006), M_ALU, bevel=0.0015)
for s in (1, -1):
    box("TraySide", (X_BR + 0.040, s * (NECK_W / 2 - 0.0025), Z_STR / 2 - 0.004), (0.072, 0.005, Z_STR - 0.006), M_ALU, bevel=0.0015)
    cyl("ChangerCheek", (X_BR + 0.006, s * (NECK_W / 2 - 0.0005), Z_STR - 0.0150), 0.0165, 0.0100, M_ALU, "Y", 36, bevel=0.0025)
    box("CheekFoot", (X_BR + 0.006, s * (NECK_W / 2 - 0.0005), (Z_STR - 0.015) / 2), (0.033, 0.0100, Z_STR - 0.015), M_ALU, bevel=0.002)
    cyl("CheekScrew", (X_BR + 0.006, s * (NECK_W / 2 + 0.0048), Z_STR - 0.0150), 0.0040, 0.0016, M_CHROME, "Y", 16)

# ---- legs: chrome upper tube, thinner lower tube with a clutch collar, rubber foot; as the reference draws them, the
# feet stand a little inboard of the endplates and the back pair further in
FEET = {}
for sx in (-1, 1):
    for sy in (-1, 1):
        top = Vector((sx * (BODY_L / 2 + 0.006), sy * (BODY_D / 2 - 0.026), -APRON - 0.010))
        foot = Vector((sx * (BODY_L / 2 - (0.036 if sy < 0 else 0.066)), -BODY_D / 2 - 0.004 if sy < 0 else BODY_D / 2 - 0.034, Z_FLOOR + 0.012))
        ax = (foot - top).normalized(); mid = top.lerp(foot, 0.70)
        rod("LegSocket", top + ax * -0.006, top + ax * 0.020, 0.0150, M_ALU, 32)
        rod("LegUpper", top, mid, 0.0112, M_CHROME, 40); rod("LegLower", mid, foot, 0.0080, M_CHROME, 40)
        rod("LegCollar", mid - ax * 0.012, mid + ax * 0.008, 0.0142, M_CHROME, 32)
        rod("LegFoot", foot - ax * 0.004, foot + ax * 0.014, 0.0125, M_RUBBER, 24)
        FEET[(sx, sy)] = foot

# ---- pedal rod clamped to the front legs, three ribbed pedals, pull rods; knee levers at the front apron
fl, fr = FEET[(-1, -1)], FEET[(1, -1)]
BAR_Z = Z_FLOOR + 0.036; BAR_Y = fl.y - 0.002
rod("PedalBar", (fl.x, BAR_Y, BAR_Z), (fr.x, BAR_Y, BAR_Z), 0.0062, M_CHROME, 32)
for f in (fl, fr):
    box("BarClamp", (f.x, BAR_Y + 0.004, BAR_Z), (0.032, 0.034, 0.030), M_POL, bevel=0.008, seg=6)
    cyl("ClampBolt", (f.x - 0.008, BAR_Y - 0.0145, BAR_Z), 0.0036, 0.003, M_CHROME, "Y", 6)
    cyl("ClampBolt", (f.x + 0.008, BAR_Y - 0.0145, BAR_Z), 0.0036, 0.003, M_CHROME, "Y", 6)
PEDAL_X = [-0.0935, -0.0250, 0.0410]
for xs_ in (PEDAL_X[0] - 0.030, PEDAL_X[2] + 0.030):                         # stop collars either side of the pedal group
    cyl("BarCollar", (xs_, BAR_Y, BAR_Z), 0.0095, 0.010, M_CHROME, "X", 24, bevel=0.001)
PT = math.radians(-16)
cT, sT = math.cos(PT), math.sin(PT)
for k, px in zip("ABC", PEDAL_X):
    box("PedalHinge_" + k, (px, BAR_Y, BAR_Z), (0.030, 0.020, 0.020), M_ALU, bevel=0.004, seg=3)
    pc = Vector((px, BAR_Y - 0.030, BAR_Z + 0.004))                         # pedal plate centre; local Y runs down the tread
    box("Pedal_" + k, pc, (0.037, 0.048, 0.009), M_ALU, bevel=0.003, seg=4, rot=(PT, 0, 0))
    for r_ in range(12):                                                      # ribs across the tread
        ly = 0.0200 - r_ * 0.0036
        box("PedalRib_" + k, pc + Vector((0, ly * cT - 0.0055 * sT, ly * sT + 0.0055 * cT)), (0.033, 0.0017, 0.0016), M_ALU, bevel=0.0005, rot=(PT, 0, 0))
    box("RodClevis_" + k, (px, BAR_Y - 0.004, BAR_Z + 0.030), (0.009, 0.009, 0.016), M_ALU, bevel=0.0025)
    for sgn in (-1, 1):
        box("Yoke_" + k, (px + sgn * 0.011, BAR_Y - 0.006, BAR_Z + 0.018), (0.005, 0.012, 0.032), M_ALU, bevel=0.002)
    cyl("ClevisPin_" + k, (px, BAR_Y - 0.004, BAR_Z + 0.032), 0.0030, 0.016, M_CHROME, "X", 12)
    rod("PullRod_" + k, (px, BAR_Y - 0.004, BAR_Z + 0.036), (px, -BODY_D / 2 + 0.012, -APRON), 0.0020, M_CHROME, 16)
for k, kx in (("L", -0.1905), ("R", 0.1366)):
    box("KneeBracket_" + k, (kx, -BODY_D / 2 + 0.008, -APRON - 0.005), (0.046, 0.020, 0.010), M_ALU, bevel=0.002)
    for bx in (-0.015, 0.015):
        cyl("KneeBolt_" + k, (kx + bx, -BODY_D / 2 - 0.0025, -APRON - 0.005), 0.0028, 0.002, M_CHROME, "Y", 6)
    box("KneeArm_" + k, (kx, -BODY_D / 2 + 0.002, -APRON - 0.026), (0.010, 0.006, 0.040), M_ALU, bevel=0.0015)
    cyl("KneePivot_" + k, (kx, -BODY_D / 2 - 0.002, -APRON - 0.040), 0.0042, 0.004, M_CHROME, "Y", 16)
    box("KneePad_" + k, (kx, -BODY_D / 2 - 0.002, -APRON - 0.086), (0.021, 0.013, 0.082), M_RUBBER, bevel=0.0055, seg=4)
    for q in range(9):
        box("KneeRib_" + k, (kx, -BODY_D / 2 - 0.0088, -APRON - 0.056 - q * 0.0075), (0.017, 0.0016, 0.0026), M_RUBBER, bevel=0.0006)

# ---- tone bar: polished steel bullet bar lying across the strings (a separate live layer later)
BAR_FRET = E("STEEL_BARFRET", 8.7)
if BAR_FRET >= 0:
    bx_ = fret_x(BAR_FRET); br = 0.0148
    cyl("ToneBar", (bx_, 0.004, Z_STR + br), br, 0.112, M_CHROME, "Y", 48, bevel=0.003)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=br, location=(bx_, 0.060, Z_STR + br))
    finish(bpy.context.object, M_CHROME).name = "ToneBarNose"

# ---- floor (shadow catcher) and what the metal reflects
bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0.5, Z_FLOOR)); fl_ = bpy.context.object; fl_.name = "Floor"
fl_.data.materials.append(M_FLOOR); fl_.is_shadow_catcher = True

# ---- camera: perspective, front-on from the player's side, looking down
TH = math.radians(E("STEEL_PITCH", 39.0)); DIST = E("STEEL_DIST", 3.4); PXM = E("STEEL_PXM", 1391.0) * 2   # canvas px / m at the neck
AIM = Vector((0, 0, NECK_H)); AIM_Y = E("STEEL_AIMY", 232.0) * 2
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
area("Key", (-1.2, -0.7, 1.5), (0.35, 0.30), 60 * KP, (1.0, 0.84, 0.66))
area("Top", (0.2, 0.3, 1.9), (1.6, 0.5), 9 * KP, (1.0, 0.90, 0.78))
area("Fill", (0.6, -1.6, 0.5), (1.6, 0.8), 11 * KP, (1.0, 0.86, 0.72))

bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=9, location=(0, 0, 0))
dome = bpy.context.object; dome.name = "RoomDome"
for p in dome.data.polygons: p.use_smooth = True
dm = bpy.data.materials.new("RoomDome"); dm.use_nodes = True; nt = dm.node_tree; nt.nodes.clear()
o_ = nt.nodes.new("ShaderNodeOutputMaterial"); em = nt.nodes.new("ShaderNodeEmission")
geo = nt.nodes.new("ShaderNodeNewGeometry"); sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(geo.outputs["Position"], sep.inputs[0])
ramp = nt.nodes.new("ShaderNodeValToRGB")
mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[1].default_value = 6.2; mr.inputs[2].default_value = 8.2   # a dark room with a bright ceiling: hard-edged reflections
nt.links.new(sep.outputs["Z"], mr.inputs[0])
# plus soft bright walls far to the left and right: round bars and legs pick them up as bands, flat tops don't
ab = nt.nodes.new("ShaderNodeMath"); ab.operation = "ABSOLUTE"; nt.links.new(sep.outputs["X"], ab.inputs[0])
ms = nt.nodes.new("ShaderNodeMapRange"); ms.inputs[1].default_value = 2.2; ms.inputs[2].default_value = 3.6; ms.inputs[4].default_value = 0.78
nt.links.new(ab.outputs[0], ms.inputs[0])
mz = nt.nodes.new("ShaderNodeMapRange"); mz.inputs[1].default_value = -2.0; mz.inputs[2].default_value = 0.0
nt.links.new(sep.outputs["Z"], mz.inputs[0])
sm = nt.nodes.new("ShaderNodeMath"); sm.operation = "MULTIPLY"; nt.links.new(ms.outputs[0], sm.inputs[0]); nt.links.new(mz.outputs[0], sm.inputs[1])
mx = nt.nodes.new("ShaderNodeMath"); mx.operation = "MAXIMUM"; nt.links.new(mr.outputs[0], mx.inputs[0]); nt.links.new(sm.outputs[0], mx.inputs[1])
nt.links.new(mx.outputs[0], ramp.inputs[0])
ramp.color_ramp.elements[0].color = (0.035, 0.016, 0.009, 1)                  # rug / dark room toward the horizon
ramp.color_ramp.elements[1].color = (1.0, 0.85, 0.68, 1)                     # warm ceiling
nt.links.new(ramp.outputs[0], em.inputs["Color"]); em.inputs["Strength"].default_value = E("STEEL_DOME", 1.6)
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

# ---- what moves in the plugin: floor pedals (with clevis, yoke, pull rod), knee levers, the tone bar
MODE = os.environ.get("STEEL_MODE", "still")
FRAMES = 7
def named(*prefixes): return [o for o in bpy.data.objects if o.name.split(".")[0] in prefixes]
LAYERS = {}
for k, px in zip("ABC", PEDAL_X):
    LAYERS["pedal_" + k] = {"turn": named("Pedal_" + k, "PedalRib_" + k, "Yoke_" + k, "RodClevis_" + k, "ClevisPin_" + k),
                            "slide": named("PullRod_" + k), "pivot": Vector((px, BAR_Y, BAR_Z)), "axis": "X", "angle": math.radians(15.0)}
for k, kx, sgn in (("L", -0.1905, 1), ("R", 0.1366, -1)):
    LAYERS["knee_" + k] = {"turn": named("KneeArm_" + k, "KneePad_" + k, "KneeRib_" + k, "KneePivot_" + k), "slide": [],
                           "pivot": Vector((kx, -BODY_D / 2, -APRON - 0.012)), "axis": "Y", "angle": sgn * math.radians(15.0)}
BAR_OBJS = named("ToneBar", "ToneBarNose")
MOVING = [o for L in LAYERS.values() for o in L["turn"] + L["slide"]] + BAR_OBJS
REST = {o.name: o.matrix_world.copy() for o in MOVING}
from mathutils import Matrix


def pose(L, t):
    R = Matrix.Translation(L["pivot"]) @ Matrix.Rotation(L["angle"] * t, 4, L["axis"]) @ Matrix.Translation(-L["pivot"])
    for o in L["turn"]: o.matrix_world = R @ REST[o.name]
    for o in L["slide"]: o.matrix_world = Matrix.Translation((0, 0, -0.005 * t)) @ REST[o.name]


def region_of(objs, margin=70):
    """canvas box around these objects' bounds (all poses are applied by the caller), with room for their shadows"""
    xs, ys = [], []
    for o in objs:
        for c in o.bound_box:
            x, y = proj(o.matrix_world @ Vector(c)); xs.append(x); ys.append(y)
    return [max(0, int(min(xs)) - margin), max(0, int(min(ys)) - margin), min(CW, int(max(xs)) + margin), min(CH, int(max(ys)) + margin)]


def render(path, region=None):
    sc.render.use_border = region is not None; sc.render.use_crop_to_border = region is not None
    if region:
        sc.render.border_min_x = region[0] / CW; sc.render.border_max_x = region[2] / CW
        sc.render.border_min_y = 1 - region[3] / CH; sc.render.border_max_y = 1 - region[1] / CH
    sc.render.filepath = str(path); bpy.ops.render.render(write_still=True)


if MODE == "still":
    render(out / "steel.png")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "steel_3d.blend"))
elif MODE == "static":                         # the instrument without its moving parts (one finish's background plate)
    for o in MOVING: o.hide_render = True
    render(out / "static.png")
else:                                          # "layers": static plate, each moving layer's poses, the tone bar alone
    info = {"frames": FRAMES, "layers": {}, "bar": {}}
    for o in MOVING: o.hide_render = True
    render(out / "static.png")
    ONLY = [x for x in os.environ.get("STEEL_LAYERS", "").split(",") if x]
    for name, L in LAYERS.items():
        objs = L["turn"] + L["slide"]
        pose(L, 1.0); bpy.context.view_layer.update(); r1 = region_of(objs)
        pose(L, 0.0); bpy.context.view_layer.update(); r0 = region_of(objs)
        reg = [min(r0[0], r1[0]), min(r0[1], r1[1]), max(r0[2], r1[2]), max(r0[3], r1[3])]
        info["layers"][name] = {"region": reg}
        if ONLY and name not in ONLY: continue
        for o in objs: o.hide_render = False
        for f in range(FRAMES):
            t = f / (FRAMES - 1); t = t * t * (3 - 2 * t)
            pose(L, t); bpy.context.view_layer.update()
            render(out / ("%s_%02d.png" % (name, f)), reg)
        pose(L, 0.0)
        for o in objs: o.hide_render = True
    # the tone bar: everything else holds out, no floor, so only the bar's own pixels (and what hides it) remain
    for o in bpy.data.objects:
        if o.type in ("MESH", "FONT") and o not in BAR_OBJS and o.name not in ("RoomDome",):
            o.is_holdout = True
    for o in BAR_OBJS: o.hide_render = False
    bpy.data.objects["Floor"].hide_render = True
    bpy.context.view_layer.update()
    reg = region_of(BAR_OBJS, 24)
    render(out / "bar.png", reg)
    info["bar"] = {"region": reg, "cx": round(proj((fret_x(BAR_FRET), 0, Z_STR))[0], 2), "fret": BAR_FRET}
    old = json.loads((out / "layers.json").read_text()) if (out / "layers.json").exists() else {}
    (out / "layers.json").write_text(json.dumps(info, indent=1))
