"""Pedal Steel UI v2 — one physical camera, correct perspective by construction.
blender -b -P build_ui.py -- <out.png> [samples] [scale]
World: instrument at origin (X length, keyhead left), player/camera at -Y, elevated.
Header and bottom bar are flat plugin chrome parented to the camera (pixel-exact)."""
import bpy, sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib, steel_lib as S, ui_lib as U
importlib.reload(S); importlib.reload(U)
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
OUT = argv[0]; SAMPLES = int(argv[1]) if len(argv) > 1 else 64; RES = float(argv[2]) if len(argv) > 2 else 1.0
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, "..")
W_PX, H_PX = 1600, 1000

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
coll = bpy.data.collections.new("UI"); sc.collection.children.link(coll)

# ---------------- materials ----------------
M = S.materials()
M["walnut"] = S.mat("Walnut", tex="walnut_1.png", tex_scale=(1, 1), rough=0.3, coat=0.7, coat_rough=0.1, bump=0.1, sat=0.95, tint=(0.48, 0.36, 0.28))
M["cream_knob"] = S.mat("CreamKnob", tex="cream_bakelite_1.png", rough=0.32, coat=0.4, coat_rough=0.2, sat=0.55)
M["black_knob"] = S.mat("BlackKnob", base=(0.02, 0.02, 0.022), rough=0.28)
M["pointer"] = S.mat("Pointer", base=(0.02, 0.015, 0.012), rough=0.5)
M["panel"] = S.mat("BlackPanel", tex="black_panel_1.png", rough=0.55, metal=0.3)
M["print_cream"] = S.mat("PrintCream", base=(0.86, 0.80, 0.68), rough=0.6)
M["print_dark"] = S.mat("PrintDark", base=(0.012, 0.008, 0.006), rough=0.7)
M["orange"] = S.mat("Orange", base=(0.85, 0.28, 0.05), rough=0.45)
M["faceplate"] = S.mat("Faceplate", tex="../brand/cream_powdercoat_1.png", rough=0.55, sat=0.8)
M["keycap"] = S.mat("Keycap", base=(0.03, 0.03, 0.032), rough=0.35, coat=0.3)
M["rubber_rib"] = S.mat("RubberRib", tex="../brand/rubber_pad_1.png", rough=0.8, bump=0.8)
M["led_on"] = U.emission_mat("LedOn", (1.0, 0.45, 0.06), 25)
M["led_off"] = S.mat("LedOff", base=(0.08, 0.03, 0.01), rough=0.2, coat=1.0)
M["meter_on"] = U.emission_mat("MeterOn", (1.0, 0.5, 0.08), 6)
M["meter_off"] = S.mat("MeterOff", base=(0.07, 0.04, 0.02), rough=0.4)
M["screen"] = U.image_emission_mat("LEDScreen", os.path.join(ROOT, "textures", "led_preset.png"), 3.5)
M["glass"] = S.mat("Glass", base=(0.02, 0.02, 0.02), rough=0.02, coat=1.0)
M["stage"] = S.mat("Stage", base=(0.30, 0.24, 0.19), rough=0.8)

def decal_mat(name, path):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    o = nt.nodes.new("ShaderNodeOutputMaterial"); b = nt.nodes.new("ShaderNodeBsdfPrincipled"); b.inputs["Roughness"].default_value = 0.5
    ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(path); ti.interpolation = "Cubic"
    nt.links.new(ti.outputs["Color"], b.inputs["Base Color"]); nt.links.new(ti.outputs["Alpha"], b.inputs["Alpha"])
    nt.links.new(b.outputs[0], o.inputs[0]); return m
M["wordmark"] = decal_mat("Wordmark", os.path.join(ROOT, "brand", "wordmark_decal.png"))

# ---------------- instrument ----------------
S.BODY_W = 0.22; S.APRON = 0.175
inst = bpy.data.collections.new("Instrument"); coll.children.link(inst)
S.build(inst, M, bar_fret=8.6)
AY = -S.BODY_W / 2 - 0.0025          # front face of the control panel
pan = S.box("ControlPanel", (0.80, 0.004, S.APRON - 0.026), (0, AY + 0.0015, -S.APRON / 2), M["panel"], inst, bevel=0.0015)
for sx in (-0.39, 0.39):
    for sz in (-0.02, -S.APRON + 0.02):
        S.cyl("Screw", 0.0035, 0.002, (sx, AY - 0.0005, sz), (math.pi / 2, 0, 0), M["chrome"], inst, verts=24, bevel=0.001)
FR = (math.pi / 2, 0, 0)
def ftext(body, x, z, size, m=None, track=0.25):
    return U.text(inst, body, (x, AY - 0.0012, z), size, m or M["print_cream"], track=track, rot=FR)
groups = [("VOICE", -0.255, [("TONE", -0.32, 0.020, "65 %", -60), ("PICK", -0.19, 0.020, "40 %", -25)]),
          ("FEEL", 0.005, [("GLIDE", -0.06, 0.026, "145 ms", 40), ("SWELL", 0.07, 0.026, "35 %", -30)]),
          ("SPACE", 0.265, [("ECHO", 0.20, 0.020, "18 %", -105), ("REVERB", 0.33, 0.020, "28 %", -70)])]
for gname, gx, knobs in groups:
    ftext(gname, gx, -0.026, 0.020, track=0.3)
    for side in (-1, 1):
        S.box("GroupRule", (0.05, 0.0004, 0.0007), (gx + side * 0.075, AY - 0.0012, -0.0255), M["print_cream"], inst)
    for name, kx, r, val, ang in knobs:
        U.knob(inst, M, Vector((kx, AY - 0.0006, -0.074)), r, ang, rot=FR)
        U.ticks(inst, M["print_cream"], Vector((kx, AY - 0.0012, -0.074)), r * 1.32, r * 1.6, rot=FR, width=0.0024)
        ftext(name, kx, -0.121, 0.019, track=0.12)
        S.box("ValWin", (0.07 if "ms" in val else 0.06, 0.002, 0.022), (kx, AY - 0.001, -0.147), M["keycap"], inst, bevel=0.002)
        U.text(inst, val, (kx, AY - 0.0024, -0.147), 0.017, M["meter_on"], track=0.02, rot=FR)
for dx in (-0.125, 0.135):
    S.box("Divider", (0.0012, 0.0004, 0.13), (dx, AY - 0.0012, -0.09), M["print_cream"], inst)

# ---------------- pedal & knee-lever console (sloped, same camera) ----------------
con = bpy.data.collections.new("Console"); coll.children.link(con)
SLOPE = math.radians(38)
croot = bpy.data.objects.new("ConsoleRoot", None); con.objects.link(croot)
croot.location = (0, -0.27, -0.215); croot.rotation_euler = (SLOPE, 0, 0)
cplate = S.box("ConsolePlate", (0.92, 0.155, 0.02), (0, 0, -0.01), M["panel"], con, bevel=0.006); cplate.parent = croot
S.uv_box_project(cplate, 1.2)
rim = S.box("ConsoleRim", (0.93, 0.165, 0.014), (0, 0, -0.016), M["walnut"], con, bevel=0.006); rim.parent = croot; S.uv_box_project(rim, 1.6)
for sx in (-0.445, 0.445):
    for sy in (-0.065, 0.065):
        s = S.cyl("Screw", 0.004, 0.002, (sx, sy, 0.001), (0, 0, 0), M["chrome"], con, verts=24, bevel=0.001); s.parent = croot
def ctext(body, x, y, size, m=None, track=0.3, align="CENTER"):
    return U.text(con, body, (x, y, 0.0005), size, m or M["print_cream"], track=track, parent=croot, rot=(0, 0, 0), align=align)
ctext("PEDALS", -0.41, 0.014, 0.022, track=0.3, align="LEFT"); ctext("& KNEE", -0.41, -0.016, 0.022, track=0.3, align="LEFT"); ctext("LEVERS", -0.41, -0.046, 0.022, track=0.3, align="LEFT")
for i, (lab, on) in enumerate((("A", True), ("B", True), ("C", False))):
    px = -0.17 + i * 0.11
    ctext(lab, px, 0.062, 0.024, track=0)
    U.flat_led(con, M, (px, 0.043), on, croot)
    p = U.foot_pedal(con, M, Vector((px, -0.022, 0.012)), w=0.085, l=0.105, tilt=-10); p.parent = croot
S.box("Sep", (0.0012, 0.12, 0.0004), (0.13, 0, 0.0004), M["print_cream"], con).parent = croot
for i, (lab, mir) in enumerate((("LKL", -1), ("LKR", 1))):
    px = 0.22 + i * 0.15
    ctext(lab, px, 0.062, 0.022, track=0.05)
    U.flat_led(con, M, (px, 0.043), False, croot)
    k = U.knee_lever(con, M, Vector((px - 0.03 * mir, 0.02, 0.004)), mirror=mir); k.parent = croot

# stage the instrument sits on (dark walnut shelf), so shadows land somewhere real
stage = S.box("Stage", (4.0, 3.0, 0.02), (0, 0.6, -0.75), M["faceplate"], coll); S.uv_box_project(stage, 1.0)
wall = S.box("Wall", (4.0, 0.02, 3.0), (0, 0.35, 0.8), M["faceplate"], coll); S.uv_box_project(wall, 1.0)

# ---------------- camera ----------------
cam_d = bpy.data.cameras.new("Cam"); cam = bpy.data.objects.new("Cam", cam_d); coll.objects.link(cam); sc.camera = cam
cam_d.lens = 62; cam_d.sensor_fit = "HORIZONTAL"; cam_d.sensor_width = 36
ELEV = math.radians(36); DIST = 2.0
target = Vector((0, -0.10, -0.13))
cam.location = target + Vector((0, -math.cos(ELEV) * DIST, math.sin(ELEV) * DIST))
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
cam_d.shift_y = -0.05
cam_d.clip_start = 0.05
sc.render.resolution_x, sc.render.resolution_y = int(W_PX * RES), int(H_PX * RES)

# ---- auto-fit: solve distance + shift so the instrument fills px x 40..1560, top at y=150 ----
from bpy_extras.object_utils import world_to_camera_view
def ndc_bbox(collection):
    bpy.context.view_layer.update(); dg = bpy.context.evaluated_depsgraph_get()
    xs, ys = [], []
    for o in collection.all_objects:
        if o.type not in ("MESH", "CURVE", "FONT"): continue
        ev = o.evaluated_get(dg)
        try: me = ev.to_mesh()
        except Exception: continue
        mw = o.matrix_world
        vs = list(me.vertices); step = max(1, len(vs) // 200)
        for v in vs[::step]:
            p = world_to_camera_view(sc, cam, mw @ v.co); xs.append(p.x); ys.append(p.y)
        ev.to_mesh_clear()
    return min(xs), max(xs), min(ys), max(ys)
def place_cam():
    cam.location = target + Vector((0, -math.cos(ELEV) * DIST, math.sin(ELEV) * DIST))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
cam_d.shift_y = 0.0
for _ in range(6):
    place_cam(); x0, x1, y0, y1 = ndc_bbox(inst)
    DIST *= (x1 - x0) / ((1560 - 40) / W_PX)
place_cam(); x0, x1, y0, y1 = ndc_bbox(inst)
cam_d.shift_y += (y1 - (1 - 150 / H_PX)) * H_PX / W_PX
x0, x1, y0, y1 = ndc_bbox(inst); INST_BOTTOM = y0
print("FIT inst", round(DIST, 3), [round(v, 3) for v in (x0, x1, y0, y1)])
# console: slide it along world Z (and Y) so its top sits 20 px under the instrument
for _ in range(10):
    cx0, cx1, cy0, cy1 = ndc_bbox(con)
    err = cy1 - (INST_BOTTOM - 20 / H_PX)
    croot.location.z -= err * 0.9 * DIST * 0.62; croot.location.y -= err * 0.3
cx0, cx1, cy0, cy1 = ndc_bbox(con); print("FIT console", [round(v, 3) for v in (cx0, cx1, cy0, cy1)])

# camera-space overlay helper: pixel position at distance d in front of the camera
def cam_px(px, py, d):
    wd = d * cam_d.sensor_width / cam_d.lens; hd = wd * H_PX / W_PX
    return Vector(((px / W_PX - 0.5 + cam_d.shift_x) * wd, (0.5 - py / H_PX + cam_d.shift_y * W_PX / H_PX) * hd, -d)), wd / W_PX
OD = 0.30   # overlay distance (in front of everything)
def ov_box(name, px0, py0, px1, py1, m, depth_px=4, bevel_px=2, z_px=0):
    c, k = cam_px((px0 + px1) / 2, (py0 + py1) / 2, OD - z_px * 0.0004)
    o = S.box(name, ((px1 - px0) * k, (py1 - py0) * k, depth_px * k), (0, 0, 0), m, coll, bevel=bevel_px * k)
    o.parent = cam; o.location = c; return o
def ov_text(body, px, py, size_px, m, track=0.3, align="CENTER", z_px=3):
    c, k = cam_px(px, py, OD - z_px * 0.0004)
    t = U.text(coll, body, c, size_px * k, m, track=track, align=align, rot=(0, 0, 0)); t.parent = cam; return t
def ov_plane(name, px0, py0, px1, py1, m, z_px=2):
    c, k = cam_px((px0 + px1) / 2, (py0 + py1) / 2, OD - z_px * 0.0004)
    bpy.ops.mesh.primitive_plane_add(size=1); o = bpy.context.active_object; o.name = name
    for cc in o.users_collection: cc.objects.unlink(o)
    coll.objects.link(o); o.scale = ((px1 - px0) * k, (py1 - py0) * k, 1); o.data.materials.append(m); o.parent = cam; o.location = c
    return o
def ov_knob(px, py, r_px, ang, style="cream"):
    c, k = cam_px(px, py, OD - 0.006)
    kn = U.knob(coll, M, c, r_px * k, ang, style=style, rot=(0, 0, 0)); kn.parent = cam; return kn
def ov_tri(px, py, s_px, m, left=True):
    c, k = cam_px(px, py, OD - 0.004)
    me = bpy.data.meshes.new("tri"); s = s_px * k
    pts = [(-s * 0.5, 0, 0), (s * 0.5, s * 0.6, 0), (s * 0.5, -s * 0.6, 0)] if left else [(s * 0.5, 0, 0), (-s * 0.5, s * 0.6, 0), (-s * 0.5, -s * 0.6, 0)]
    me.from_pydata(pts, [], [(0, 1, 2)]); o = bpy.data.objects.new("Tri", me); coll.objects.link(o); me.materials.append(m)
    o.parent = cam; o.location = c; return o

# header band
ov_box("Header", 0, 0, 1600, 138, M["faceplate"], depth_px=6, bevel_px=0)
ov_box("HeaderLip", 0, 136, 1600, 141, M["chrome"], depth_px=8, bevel_px=2, z_px=1)
ov_text("NORTHERN VALLEY AUDIO", 205, 22, 22, M["print_dark"], track=0.4)
for x0, x1 in ((22, 50), (360, 388)):
    ov_box("Rule", x0, 23, x1, 25, M["print_dark"], depth_px=1, bevel_px=0, z_px=3)
ov_plane("Wordmark", 42, 40, 398, 96, M["wordmark"], z_px=3)
ov_box("LEDBezel", 536, 12, 876, 82, M["keycap"], depth_px=10, bevel_px=6, z_px=2)
ov_plane("LEDScreen", 552, 22, 860, 72, M["screen"], z_px=8)
for x0, left in ((470, True), (890, False)):
    ov_box("ArrowKey", x0, 24, x0 + 52, 70, M["keycap"], depth_px=10, bevel_px=7, z_px=3)
    ov_tri(x0 + 26, 47, 16, M["print_cream"], left=left)
for x0, x1, lab in ((955, 1060, "BROWSE"), (1070, 1160, "SAVE")):
    ov_box("Key", x0, 28, x1, 66, M["keycap"], depth_px=10, bevel_px=7, z_px=3)
    ov_text(lab, (x0 + x1) / 2, 47, 22, M["print_cream"], track=0.1, z_px=9)
ov_box("Div", 1180, 25, 1182, 70, M["print_dark"], depth_px=1, bevel_px=0, z_px=3)
ov_text("OUTPUT", 1222, 47, 21, M["print_dark"], track=0.3)
for i in range(14):
    ov_box("Seg", 1272 + i * 10.5, 40, 1272 + i * 10.5 + 8, 55, M["meter_on"] if i < 10 else M["meter_off"], depth_px=2, bevel_px=1, z_px=3)
ov_box("Div", 1444, 25, 1446, 70, M["print_dark"], depth_px=1, bevel_px=0, z_px=3)
ov_text("TRIM", 1492, 15, 19, M["print_dark"], track=0.3)
ov_knob(1492, 52, 17, 0)
ov_text("0.0 dB", 1552, 52, 23, M["print_dark"], track=0.05)
for x, lab, on in ((560, "PLAY", True), (790, "MOTION", False), (1030, "EFFECTS", False)):
    ov_text(lab, x, 112, 27, M["print_dark"], track=0.3)
ov_box("TabLine", 500, 127, 620, 133, M["orange"], depth_px=1, bevel_px=0, z_px=3)
for x in (675, 910):
    ov_box("TabSep", x, 104, x + 2, 120, M["print_dark"], depth_px=1, bevel_px=0, z_px=3)

# bottom bar
ov_box("Footer", 0, 922, 1600, 1000, M["faceplate"], depth_px=6, bevel_px=0)
ov_box("FooterLip", 0, 919, 1600, 924, M["chrome"], depth_px=8, bevel_px=2, z_px=1)
ov_text("MODE", 62, 961, 24, M["print_dark"], track=0.35)
ov_box("ModeBox", 115, 942, 420, 980, M["keycap"], depth_px=6, bevel_px=6, z_px=2)
ov_text("Smart Auto-Pedal E9", 135, 961, 24, M["print_cream"], track=0.02, align="LEFT", z_px=8)
ov_tri(398, 961, 12, M["print_cream"], left=False).rotation_euler = (0, 0, -math.pi / 2)
ov_text("VOLUME PEDAL", 545, 961, 24, M["print_dark"], track=0.35)
ov_box("Track", 660, 955, 1150, 967, M["keycap"], depth_px=2, bevel_px=5, z_px=2)
ov_box("TrackFill", 663, 958, 1012, 964, M["orange"], depth_px=2, bevel_px=2, z_px=3)
ov_box("Thumb", 996, 943, 1030, 979, M["cream_knob"], depth_px=14, bevel_px=6, z_px=4)
ov_box("ValBox", 1168, 946, 1228, 976, M["keycap"], depth_px=4, bevel_px=4, z_px=2)
ov_text("72%", 1198, 961, 23, M["print_cream"], track=0.05, z_px=8)
ov_box("Midi", 1400, 944, 1560, 978, M["faceplate"], depth_px=8, bevel_px=6, z_px=3)
ov_text("MIDI LEARN", 1480, 961, 23, M["print_dark"], track=0.3, z_px=12)

# ---------------- light & world ----------------
hdr = r"C:/Users/Joshua/Documents/farwilder/experiments/character_lab/downloads/hdri/studio_small_09_4k.hdr"
w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True; nt = w.node_tree
env = nt.nodes.new("ShaderNodeTexEnvironment"); env.image = bpy.data.images.load(hdr)
nt.links.new(env.outputs[0], nt.nodes["Background"].inputs[0]); nt.nodes["Background"].inputs[1].default_value = 0.35
def area(loc, energy, size, aim=Vector((0, -0.1, -0.05))):
    l = bpy.data.lights.new("A", "AREA"); l.energy = energy; l.size = size
    o = bpy.data.objects.new("A", l); coll.objects.link(o); o.location = loc
    o.rotation_euler = (aim - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
area((-0.9, -1.1, 1.3), 150, 1.0)     # key, upper left
area((1.2, -1.4, 0.5), 60, 1.5)       # fill, right
area((0.0, 0.8, 0.9), 80, 1.2)        # rim from behind for chrome edges

sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = os.environ.get("NODENOISE") is None
p = bpy.context.preferences.addons["cycles"].preferences; p.compute_device_type = "OPTIX"; p.get_devices()
for dv in p.devices: dv.use = dv.type == "OPTIX"
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.5
sc.render.filepath = OUT
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")
