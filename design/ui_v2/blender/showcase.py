"""Hero showcase of the pedal steel at true proportions, three-quarter view, on legs with the pedal rack.
blender -b -P showcase.py -- out.png [yaw_deg] [elev_deg] [samples]"""
import bpy, sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import steel_lib as S
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
OUT = a[0]; YAW = float(a[1]) if len(a) > 1 else 28; ELEV = float(a[2]) if len(a) > 2 else 30
SAMPLES = int(a[3]) if len(a) > 3 else 96
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
coll = bpy.data.collections.new("Steel"); sc.collection.children.link(coll)
M = S.materials()
M["walnut"] = S.mat("Walnut", tex="walnut_1.png", rough=0.3, coat=0.7, coat_rough=0.1, bump=0.1, tint=(0.48, 0.36, 0.28))
S.BODY_W = 0.21; S.APRON = 0.095       # real single-neck S10 proportions
S.build(coll, M, bar_fret=8.6)
H = 0.62                                # deck height above the floor (playing height ~ 0.7 m incl. legs)
root = bpy.data.objects.new("Root", None); coll.objects.link(root)
for o in list(coll.objects):
    if o.parent is None and o is not root: o.parent = root
root.location.z = H
# legs: four chrome tubes, slightly splayed, with rubber tips
for sx in (S.BODY_X[0] + 0.05, S.BODY_X[1] - 0.05):
    for sy in (-0.065, 0.065):
        top = Vector((sx, sy, H - S.APRON)); bot = Vector((sx * 1.04, sy * 1.5, 0.02)); d = bot - top
        leg = S.cyl("Leg", 0.0115, d.length, (top + bot) / 2, (0, 0, 0), M["chrome"], coll, verts=32)
        leg.rotation_mode = "QUATERNION"; leg.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
        S.cyl("LegTip", 0.014, 0.025, bot + Vector((0, 0, -0.008)), (0, 0, 0), M["rubber"], coll, bevel=0.004)
# pedal rack: bar between the front legs at floor level, three pedals A B C with pull rods to the underside
rx0, rx1 = -0.12, 0.08
S.cyl("PedalBar", 0.009, 0.36, (-0.02, -0.11, 0.06), (0, math.pi / 2, 0), M["chrome"], coll)
for i in range(3):
    px = -0.10 + i * 0.075
    ped = S.box("Pedal", (0.05, 0.11, 0.016), (px, -0.16, 0.075), M["chrome"], coll, bevel=0.006, segs=5)
    ped.rotation_euler = (math.radians(-12), 0, 0)
    pad = S.box("PedalPad", (0.042, 0.09, 0.006), (px, -0.163, 0.086), M["rubber"], coll, bevel=0.002)
    pad.rotation_euler = (math.radians(-12), 0, 0)
    top = Vector((px, -0.02, H - S.APRON)); bot = Vector((px, -0.13, 0.085)); d = bot - top
    rod = S.cyl("PullRod", 0.003, d.length, (top + bot) / 2, (0, 0, 0), M["chrome"], coll, verts=12)
    rod.rotation_mode = "QUATERNION"; rod.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
# knee levers under the apron
for kx, side in ((-0.17, -1), (0.06, 1)):
    S.box("KneeLever", (0.012, 0.05, 0.10), (kx, -0.085, H - S.APRON - 0.05), M["chrome"], coll, bevel=0.004)
    S.box("KneePad", (0.03, 0.02, 0.04), (kx + 0.012 * side, -0.11, H - S.APRON - 0.09), M["rubber"], coll, bevel=0.006)
# stage: cream seamless sweep
bpy.ops.mesh.primitive_plane_add(size=12, location=(0, 0, 0)); fl = bpy.context.active_object
fl.data.materials.append(S.mat("Stage", base=(0.62, 0.55, 0.46), rough=0.75))
# camera: player's three-quarter view
cam_d = bpy.data.cameras.new("Cam"); cam = bpy.data.objects.new("Cam", cam_d); coll.objects.link(cam); sc.camera = cam
cam_d.lens = float(a[4]) if len(a) > 4 else 45
target = Vector((0, 0, H - 0.2)); dist = float(a[5]) if len(a) > 5 else 1.9
y, e = math.radians(YAW), math.radians(ELEV)
cam.location = target + Vector((math.sin(y) * math.cos(e) * dist, -math.cos(y) * math.cos(e) * dist, math.sin(e) * dist))
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
# lights
def area(loc, energy, size):
    l = bpy.data.lights.new("A", "AREA"); l.energy = energy; l.size = size
    o = bpy.data.objects.new("A", l); coll.objects.link(o); o.location = loc
    o.rotation_euler = (target - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
area((-1.6, 0.4, 1.8), 220, 1.6); area((1.6, -0.9, 1.0), 90, 2.0); area((0.3, 1.5, 1.6), 160, 1.0)
hdr = r"C:/Users/Joshua/Documents/farwilder/experiments/character_lab/downloads/hdri/studio_small_09_4k.hdr"
w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True; nt = w.node_tree
env = nt.nodes.new("ShaderNodeTexEnvironment"); env.image = bpy.data.images.load(hdr)
nt.links.new(env.outputs[0], nt.nodes["Background"].inputs[0]); nt.nodes["Background"].inputs[1].default_value = 0.4
sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES
p = bpy.context.preferences.addons["cycles"].preferences; p.compute_device_type = "OPTIX"; p.get_devices()
for dv in p.devices: dv.use = dv.type == "OPTIX"
sc.render.resolution_x, sc.render.resolution_y = 1600, 1000
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.4
sc.render.filepath = OUT
bpy.ops.render.render(write_still=True)
