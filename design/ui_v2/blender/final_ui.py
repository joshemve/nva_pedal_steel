"""Pedal Steel UI v2 final scene (layout of round17/x6_1, top bar without SAVE).
blender -b -P final_ui.py -- <out.png> [samples] [res_scale] [elev_deg]
Steel: true proportions, horizontal, front-on, camera above looking down. Room: oak floor, Persian rug,
background plate. Frame (top bar + control deck) is camera-space UI geometry in pixel units (1600x1000)."""
import bpy, bmesh, sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import steel_lib as S, ui_lib as U, knob_lib as KL
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
OUT = a[0]; SAMPLES = int(a[1]) if len(a) > 1 else 64; RES = float(a[2]) if len(a) > 2 else 1.0
ELEV = float(a[3]) if len(a) > 3 else 58
SKIN = a[4] if len(a) > 4 else "walnut"
SKINS = {
 "walnut": dict(led="01 SWEET GEORGIA E9", key=(1, 0.86, 0.7), rug=(0.42, 0.33, 0.3), hw=None),
 "maple": dict(led="04 MONTANA LUSH SWELL", key=(0.92, 0.95, 1.0), rug=(0.6, 0.58, 0.55), hw=None),
 "redsparkle": dict(led="06 BAKERSFIELD SLAPBACK", key=(1, 0.8, 0.58), rug=(0.38, 0.28, 0.24), hw=None),
 "koa": dict(led="18 SUNSET SLACK-KEY", key=(1, 0.78, 0.5), rug=(0.55, 0.47, 0.38), hw=(1.0, 0.82, 0.55)),
 "pearloid": dict(led="13 4-TRACK CASSETTE", key=(1, 0.88, 0.74), rug=(0.55, 0.47, 0.4), hw=None)}
SK = SKINS[SKIN]
CREAM_EMIT = tuple(float(v) for v in os.environ.get("CREAM", "2.9,1.55,0.66").split(",")) + (1.0,)
RUG = {"walnut": ("final/carpet_1.png", (2.4, 1.8)), "maple": ("textures/rooms/woolrug_1.png", (2.0, 1.5)), "redsparkle": ("textures/rooms/honkytonk_1.png", (2.4, 1.8)), "koa": ("textures/rooms/lauhala_1.png", (3.2, 2.4)), "pearloid": ("textures/rooms/shag_1.png", (2.4, 1.8))}[SKIN]
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, "..")
W_PX, H_PX = 1600, 1000
X = lambda x: x * 1600 / 1568      # x6_1 was 1568 wide

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
coll = bpy.data.collections.new("Scene"); sc.collection.children.link(coll)
ui = bpy.data.collections.new("UI"); sc.collection.children.link(ui)

# ---------------- materials ----------------
M = S.materials()
M["walnut"] = S.mat("Walnut", tex="walnut_1.png", rough=0.4, coat=0.35, coat_rough=0.22, bump=0.08, tint=(0.5, 0.36, 0.27))
M["ui_walnut"] = M["walnut"]
if SKIN == "maple": M["walnut"] = S.mat("Maple", tex="skins/maple_1.png", rough=0.35, coat=0.5, coat_rough=0.15, bump=0.05)
if SKIN == "koa": M["walnut"] = S.mat("Koa", tex="skins/koa_1.png", rough=0.3, coat=0.7, coat_rough=0.08, bump=0.05)
if SKIN == "redsparkle": M["walnut"] = S.mat("RedSparkle", tex="skins/redsparkle_1.png", metal=0.55, rough=0.32, coat=1.0, coat_rough=0.04, bump=0.3)
if SKIN == "pearloid": M["walnut"] = S.mat("Pearloid", tex="skins/pearloid_1.png", rough=0.25, coat=1.0, coat_rough=0.05, bump=0.05)
if SK["hw"]: M["chrome"] = S.mat("Brass", base=SK["hw"], metal=1.0, rough=0.12)
KL.materials(S, M)
M["panel"] = S.mat("BlackPanel", tex="black_panel_1.png", rough=0.6, metal=0.2)
M["cream_face"] = S.mat("CreamFace", tex="../brand/cream_powdercoat_1.png", rough=0.65, sat=1.0, tint=(1.0, 0.86, 0.66))
_cf = M["cream_face"].node_tree; _b = [n for n in _cf.nodes if n.type == "BSDF_PRINCIPLED"][0]
_b.inputs["Emission Color"].default_value = CREAM_EMIT; _b.inputs["Emission Strength"].default_value = 1.0
_b.inputs["Base Color"].default_value = (0, 0, 0, 1)
for _l in list(_cf.links):
    if _l.to_socket == _b.inputs["Base Color"]: _cf.links.remove(_l)
M["print_cream"] = S.mat("PrintCream", base=(0.86, 0.80, 0.67), rough=0.6)
M["print_dark"] = S.mat("PrintDark", base=(0.012, 0.009, 0.007), rough=0.8)
M["rule_dark"] = S.mat("RuleDark", base=(0.32, 0.25, 0.2), rough=0.7)
M["keycap"] = S.mat("Keycap", base=(0.025, 0.024, 0.024), rough=0.35, coat=0.3)
M["rubber_rib"] = S.mat("RubberRib", tex="../brand/rubber_pad_1.png", rough=0.8, bump=0.9)
M["led_on"] = U.emission_mat("LedOn", (1.0, 0.42, 0.04), 5)
M["led_off"] = S.mat("LedOff", base=(0.12, 0.11, 0.1), rough=0.2, coat=1.0)
M["glow"] = U.emission_mat("PedalGlow", (1.0, 0.45, 0.06), 4)
M["meter_on"] = U.emission_mat("MeterOn", (1.0, 0.55, 0.1), 7)
M["meter_off"] = S.mat("MeterOff", base=(0.06, 0.04, 0.03), rough=0.4)
_ledp = os.path.join(ROOT, "textures", "led_%s.png" % SKIN)   # pre-built by make_led.py (Blender has no PIL)
M["screen"] = U.image_emission_mat("LEDScreen", _ledp, 3.0)
M["track"] = S.mat("Track", base=(0.75, 0.70, 0.6), rough=0.5)
M["capmetal"] = S.mat("CapMetal", base=(0.92, 0.9, 0.86), metal=1.0, rough=0.38)

# ---------------- steel on legs (front-on) ----------------
inst = bpy.data.collections.new("Steel"); coll.children.link(inst)
S.BODY_W = 0.21; S.APRON = 0.095
S.build(inst, M, bar_fret=8.6)
H = 0.40
sroot = bpy.data.objects.new("SteelRoot", None); inst.objects.link(sroot)
for o in list(inst.objects):
    if o.parent is None and o is not sroot: o.parent = sroot
sroot.location.z = H
def rod(a_, b_, r, m, name):
    d = b_ - a_
    o = S.cyl(name, r, d.length, (a_ + b_) / 2, (0, 0, 0), m, inst, verts=24)
    o.rotation_mode = "QUATERNION"; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    return o
for sx in (S.BODY_X[0] + 0.06, S.BODY_X[1] - 0.06):
    for sy in (-0.07, 0.07):
        top = Vector((sx, sy, H - S.APRON)); bot = Vector((sx * 1.03, sy * 1.25, 0.03))
        rod(top, bot, 0.012, M["chrome"], "Leg")
        S.cyl("LegTip", 0.015, 0.03, bot + Vector((0, 0, -0.012)), (0, 0, 0), M["rubber"], inst, bevel=0.004)
# pedal rack centred, three pedals with pull rods
S.cyl("PedalBar", 0.008, 0.30, (0, -0.06, 0.05), (0, math.pi / 2, 0), M["chrome"], inst)
for i in range(3):
    px = (i - 1) * 0.062
    p = S.box("PedalFrame", (0.05, 0.10, 0.018), (px, -0.10, 0.07), M["chrome"], inst, bevel=0.006, segs=5)
    p.rotation_euler = (math.radians(-14), 0, 0)
    q = S.box("PedalPad", (0.04, 0.085, 0.006), (px, -0.103, 0.081), M["rubber_rib"], inst, bevel=0.002)
    q.rotation_euler = (math.radians(-14), 0, 0); S.uv_box_project(q, 9.0)
    rod(Vector((px, -0.01, H - S.APRON)), Vector((px, -0.06, 0.085)), 0.0028, M["chrome"], "PullRod")
for kx in (-0.17, 0.10):     # knee levers hanging under the apron
    rod(Vector((kx, -0.07, H - S.APRON)), Vector((kx, -0.08, H - S.APRON - 0.09)), 0.005, M["chrome"], "KneeArm")
    S.box("KneePad", (0.03, 0.02, 0.045), (kx, -0.085, H - S.APRON - 0.11), M["rubber"], inst, bevel=0.007)

# ---------------- room ----------------
def tex_plane(name, size, loc, rot, img, tile, rough=0.8, emit=0.0, tint=None):
    bpy.ops.mesh.primitive_plane_add(size=1, location=loc, rotation=rot); o = bpy.context.active_object; o.name = name
    for c in o.users_collection: c.objects.unlink(o)
    coll.objects.link(o); o.scale = (size[0], size[1], 1)
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); b = nt.nodes.new("ShaderNodeBsdfPrincipled"); b.inputs["Roughness"].default_value = rough
    ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(os.path.join(ROOT, img))
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (tile[0], tile[1], 1)
    nt.links.new(tc.outputs["UV"], mp.inputs[0]); nt.links.new(mp.outputs[0], ti.inputs[0])
    col = ti.outputs[0]
    if tint:
        mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs[0].default_value = 1.0
        mx.inputs[7].default_value = (*tint, 1); nt.links.new(col, mx.inputs[6]); col = mx.outputs[2]
    nt.links.new(col, b.inputs["Base Color"])
    if emit:
        nt.links.new(ti.outputs[0], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = emit
    nt.links.new(b.outputs[0], out.inputs[0]); o.data.materials.append(m); return o
tex_plane("Floor", (12, 12), (0, 0, -0.002), (0, 0, 0), "final/floor_1.png", (8, 8), 0.45, tint=(0.45, 0.38, 0.32))
tex_plane("Rug", (3.2, 2.4), (0, 0.15, 0.0), (0, 0, 0), RUG[0], RUG[1], 0.95, tint=SK["rug"])
tex_plane("RoomPlate", (9.0, 5.6), (0, 4.5, 1.6), (math.radians(80), 0, 0), "final/room_1.png", (1, 1), 1.0, emit=0.6)

# ---------------- camera + auto-fit ----------------
cam_d = bpy.data.cameras.new("Cam"); cam = bpy.data.objects.new("Cam", cam_d); coll.objects.link(cam); sc.camera = cam
cam_d.lens = 80; cam_d.sensor_fit = "HORIZONTAL"; cam_d.sensor_width = 36; cam_d.clip_start = 0.05
sc.render.resolution_x, sc.render.resolution_y = int(W_PX * RES), int(H_PX * RES)
target = Vector((0, -0.02, H - 0.12)); e = math.radians(ELEV); DIST = 1.8
def place():
    cam.location = target + Vector((0, -math.cos(e) * DIST, math.sin(e) * DIST))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
from bpy_extras.object_utils import world_to_camera_view
def bbox(objs):
    bpy.context.view_layer.update(); dg = bpy.context.evaluated_depsgraph_get(); xs, ys = [], []
    for o in objs:
        if o.type not in ("MESH", "CURVE"): continue
        ev = o.evaluated_get(dg); me = ev.to_mesh(); vs = list(me.vertices); st = max(1, len(vs) // 150)
        for v in vs[::st]:
            p = world_to_camera_view(sc, cam, o.matrix_world @ v.co); xs.append(p.x); ys.append(p.y)
        ev.to_mesh_clear()
    return min(xs), max(xs), min(ys), max(ys)
body_objs = [o for o in inst.all_objects if o.name.split(".")[0] in ("Body", "Deck", "EndCap", "EndPlate", "KeyButton")]
want_w = (X(1440) - X(130)) / W_PX
for _ in range(6):
    place(); x0, x1, y0, y1 = bbox(body_objs); DIST *= (x1 - x0) / want_w
place(); x0, x1, y0, y1 = bbox(body_objs)
cam_d.shift_y = (y1 - (1 - 128 / H_PX)) * H_PX / W_PX
cam_d.shift_x = ((x0 + x1) / 2 - 0.5)
allb = bbox([o for o in inst.all_objects])
print("FIT", round(DIST, 3), "steel ndc", [round(v, 3) for v in allb])

# ---------------- camera-space UI helpers (px) ----------------
OD = 0.3
def cpx(px, py, dz=0.0):
    wd = OD * cam_d.sensor_width / cam_d.lens; hd = wd * H_PX / W_PX
    return Vector(((px / W_PX - 0.5 + cam_d.shift_x) * wd, (0.5 - py / H_PX + cam_d.shift_y * W_PX / H_PX) * hd, -OD + dz)), wd / W_PX
K = cpx(0, 0)[1]
def link(o):
    for c in o.users_collection: c.objects.unlink(o)
    ui.objects.link(o); o.parent = cam; return o
def box(px0, py0, px1, py1, m, depth=4, bevel=2, z=0, name="B"):
    c, k = cpx((px0 + px1) / 2, (py0 + py1) / 2, z * k_)
    o = S.box(name, ((px1 - px0) * k, (py1 - py0) * k, depth * k), (0, 0, 0), m, ui, bevel=bevel * k, segs=4)
    link(o); o.location = c + Vector((0, 0, depth * k / 2)); return o
k_ = K
def text(body, px, py, size, m, track=0.25, align="CENTER", z=16, font=U.FONT_SANS):
    c, k = cpx(px, py, z * K)
    t = U.text(ui, body, c, size * 1.15 * k, m, track=track, align=align, rot=(0, 0, 0)); t.data.font = U.font(font); link(t); t.location = c
    return t
def hline(px0, px1, py, m, w=1.4, z=4): return box(px0, py - w / 2, px1, py + w / 2, m, depth=0.6, bevel=0, z=z)
def vline(px, py0, py1, m, w=1.4, z=4): return box(px - w / 2, py0, px + w / 2, py1, m, depth=0.6, bevel=0, z=z)
def pill(cx, cy, w, h, m, line=1.6, z=4):
    for (x0, y0, x1, y1) in ((cx - w / 2, cy - h / 2, cx + w / 2, cy - h / 2 + line), (cx - w / 2, cy + h / 2 - line, cx + w / 2, cy + h / 2),
                             (cx - w / 2, cy - h / 2, cx - w / 2 + line, cy + h / 2), (cx + w / 2 - line, cy - h / 2, cx + w / 2, cy + h / 2)):
        box(x0, y0, x1, y1, m, depth=0.6, bevel=0, z=z)
def plane(px0, py0, px1, py1, m, z=8):
    c, k = cpx((px0 + px1) / 2, (py0 + py1) / 2, z * K)
    bpy.ops.mesh.primitive_plane_add(size=1); o = bpy.context.active_object; link(o)
    o.scale = ((px1 - px0) * k, (py1 - py0) * k, 1); o.location = c; o.data.materials.append(m); return o
def tri(cx, cy, s, m, left, z=18):
    c, k = cpx(cx, cy, z * K); s *= k
    me = bpy.data.meshes.new("tri")
    pts = [(-s * .5, 0, 0), (s * .5, s * .6, 0), (s * .5, -s * .6, 0)] if left else [(s * .5, 0, 0), (-s * .5, s * .6, 0), (-s * .5, -s * .6, 0)]
    me.from_pydata(pts, [], [(0, 1, 2)]); o = bpy.data.objects.new("Tri", me); ui.objects.link(o); me.materials.append(m)
    o.parent = cam; o.location = c; return o
def screw(px, py):
    c, k = cpx(px, py, 14 * K)
    o = S.cyl("Screw", 5 * k, 2 * k, c, (0, 0, 0), M["chrome"], ui, verts=24, bevel=1.2 * k); link(o); o.location = c
    s = S.box("Slot", (7 * k, 1.2 * k, 1 * k), c + Vector((0, 0, 1.2 * k)), M["print_dark"], ui); link(s); s.location = c + Vector((0, 0, 1.2 * k)); s.rotation_euler = (0, 0, 0.6)

PANEL_Z = 12.4   # px: front face of the deck panel
def shadow_mat(name):
    m = bpy.data.materials.get(name)
    if m: return m
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    o = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bs.inputs["Base Color"].default_value = (0, 0, 0, 1); bs.inputs["Roughness"].default_value = 1.0
    ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(os.path.join(ROOT, "textures", name + ".png"))
    mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"; mul.inputs[1].default_value = 0.85
    nt.links.new(ti.outputs["Alpha"], mul.inputs[0]); nt.links.new(mul.outputs[0], bs.inputs["Alpha"])
    nt.links.new(bs.outputs[0], o.inputs[0]); return m
def drop(px, py, wpx, hpx, kind="round", dx=6, dy=10, z=PANEL_Z + 0.1):
    # texture has a 20% margin on each side, so scale the plane by 1/0.6
    o = plane(px + dx - wpx / 0.6 / 2, py + dy - hpx / 0.6 / 2, px + dx + wpx / 0.6 / 2, py + dy + hpx / 0.6 / 2, shadow_mat("shadow_" + kind), z=z)
    return o

# tick ring radius per style, measured on APPROVED_main.png (circle fit through the printed ticks)
TICK_SCALE = {"dome": 1.073, "scallop": 0.921, "space": 0.905}
# knob centres = centres of the reference's tick rings (UI px), so strips land exactly where the approved screen has them
REF_CENTRE = {"TONE": (132.8, 775.8), "PICK": (271.3, 775.8), "GLIDE": (441.4, 776.8), "SWELL": (582.9, 776.7),
              "ECHO": (746.4, 777.0), "REVERB": (876.3, 777.1)}
KNOBS = []    # (style, px, py, rpx, root, drop) for STRIP / NO_KNOBS modes
def knob(px, py, rpx, ang, style):
    """ang = degrees clockwise from 12 o'clock. style: dome | scallop | space (knob_lib) or 'trim'."""
    rpx0 = rpx   # frame size for strips stays fixed whatever the fitted scale
    rpx *= float(os.environ.get("KNOB_RSCALE_" + style.upper(), str(KL.RSCALE.get(style, 1.0))))   # fitting hook
    c, k = cpx(px, py, 0)
    R = rpx * k
    z = 6.0 if style == "trim" else PANEL_Z      # trim sits on the cream top bar (front face at 6 px)
    d = drop(px, py, rpx * 2.5, rpx * 2.5, "round", dx=rpx * 0.18, dy=rpx * 0.32, z=z + 0.1)
    root = KL.build(ui, M, R, "dome" if style == "trim" else style, parent=cam); KL.spin(root, ang)   # TRIM: small cream dome on a knurled skirt, as in the reference
    KNOBS.append((style, px, py, rpx0, root, d))
    tick_root = bpy.data.objects.new("TickRoot", None); ui.objects.link(tick_root); tick_root.parent = cam
    dpt = OD - z * K   # printed on the flat panel (never tilted with the knob), on the knob's sight line so it stays concentric
    tick_root.location = Vector((c.x * dpt / OD, c.y * dpt / OD, -dpt))
    ts = TICK_SCALE.get(style, 1.0)
    if style != "trim":
        for i in range(11):   # printed tick scale on the panel face
            tt = math.radians(-135 + 27 * i); r0 = (R * 1.1 + 5 * k) * ts; r1 = r0 + (9 if i == 5 else 7) * k
            tk = S.box("Tick", (2.2 * k, r1 - r0, 0.4 * k), (0, 0, 0), M["print_cream"], ui); tk.parent = tick_root
            tk.location = (-math.sin(tt) * (r0 + r1) / 2, math.cos(tt) * (r0 + r1) / 2, 0.3 * k); tk.rotation_euler = (0, 0, tt)
    # head-on: put the knob on its own sight line and aim its axis at the camera, so no knob shows a side wall
    # from perspective parallax and each projects centred on its layout position (and on its printed ticks)
    dpt = OD - z * K
    root.location = Vector((c.x * dpt / OD, c.y * dpt / OD, -dpt))
    root.rotation_mode = "QUATERNION"; root.rotation_quaternion = Vector((0, 0, 1)).rotation_difference((-root.location).normalized())
    return root

def pedal_block(cx, py0, py1, w, on):
    drop(cx, (py0 + py1) / 2, w, py1 - py0, "rect", dx=5, dy=11)
    box(cx - w / 2, py0, cx + w / 2, py1, M["chrome"], depth=10, bevel=6, z=PANEL_Z)
    box(cx - w / 2 + 7, py0 + 12, cx + w / 2 - 7, py1 - 8, M["rubber_rib"], depth=12, bevel=3, z=PANEL_Z)
    if on: box(cx - w / 2 + 8, py0 + 4, cx + w / 2 - 8, py0 + 11, M["glow"], depth=11, bevel=2, z=PANEL_Z)

def lever(cx, py):
    """Knee lever like the reference: chrome mounting bracket and pivot, a bent chrome arm angling down-left,
    and a chrome-framed ribbed rubber paddle, all standing off the panel with a soft cast shadow."""
    Z = PANEL_Z * K
    pivx, pivy = cx + 14, py + 6
    padx0, pady0, padx1, pady1 = cx - 52, py + 62, cx + 10, py + 112
    drop((padx0 + padx1) / 2, (pady0 + pady1) / 2, padx1 - padx0, pady1 - pady0, "rect", dx=7, dy=12)
    drop(pivx - 12, pivy + 30, 26, 70, "rect", dx=7, dy=10)
    c, k = cpx(pivx, pivy, 0)
    br = S.box("Bracket", (30 * k, 24 * k, 6 * k), (0, 0, 0), M["chrome"], ui, bevel=4 * k); link(br); br.location = c + Vector((0, 0, Z + 3 * k))
    for sx in (-10, 10):
        sc_ = S.cyl("BracketScrew", 2.6 * k, 1.5 * k, (0, 0, 0), (0, 0, 0), M["print_dark"], ui, verts=16); link(sc_); sc_.location = c + Vector((sx * k, 0, Z + 6.6 * k))
    ring = S.cyl("Pivot", 9 * k, 10 * k, (0, 0, 0), (0, 0, 0), M["chrome"], ui, verts=48, bevel=2.5 * k); link(ring); ring.location = c + Vector((0, 0, Z + 9 * k))
    cu = bpy.data.curves.new("arm", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 4.6 * k; cu.bevel_resolution = 6
    sp = cu.splines.new("BEZIER")
    a0 = cpx(pivx, pivy, Z + 12 * k)[0]; a1 = cpx(cx - 4, py + 34, Z + 18 * k)[0]; a2 = cpx(cx - 18, py + 66, Z + 14 * k)[0]
    sp.bezier_points.add(2)
    for bp, pt in zip(sp.bezier_points, (a0, a1, a2)):
        bp.co = pt; bp.handle_left_type = bp.handle_right_type = "AUTO"
    arm = bpy.data.objects.new("Arm", cu); ui.objects.link(arm); cu.materials.append(M["chrome"]); arm.parent = cam
    box(padx0, pady0, padx1, pady1, M["chrome"], depth=12, bevel=6, z=PANEL_Z + 4)
    box(padx0 + 6, pady0 + 7, padx1 - 6, pady1 - 6, M["rubber_rib"], depth=14, bevel=3, z=PANEL_Z + 4)

def led(px, py, on):
    c, k = cpx(px, py, 0)
    b = S.cyl("LEDBezel", 8 * k, 3 * k, (0, 0, 0), (0, 0, 0), M["chrome"], ui, bevel=1.2 * k); link(b); b.location = c + Vector((0, 0, (PANEL_Z + 1.5) * k))
    l = S.cyl("LED", 5.5 * k, 4 * k, (0, 0, 0), (0, 0, 0), M["led_on"] if on else M["led_off"], ui, verts=32, bevel=2 * k); link(l); l.location = c + Vector((0, 0, (PANEL_Z + 3.5) * k))

# ---------------- top bar (no SAVE) ----------------
TOPBAR = box(0, 0, 1600, 97, M["cream_face"], depth=6, bevel=0)
hline(0, 1600, 97.5, M["rule_dark"], w=1.6, z=7)
text("NORTHERN VALLEY AUDIO", X(785), 19, 16, M["print_dark"], track=0.75)
text("Pedal Steel", X(40), 47, 58, M["print_dark"], track=-0.02, align="LEFT", font=r"C:\Windows\Fonts\georgia.ttf")
hline(X(335), X(505), 51, M["rule_dark"], z=7); hline(X(1062), X(1140), 51, M["rule_dark"], z=7)
box(X(533), 40, X(583), 82, M["keycap"], depth=10, bevel=7, z=2); tri(X(558), 61, 18, M["print_cream"], True)
box(X(985), 40, X(1035), 82, M["keycap"], depth=10, bevel=7, z=2); tri(X(1010), 61, 18, M["print_cream"], False)
box(X(596), 38, X(972), 84, M["keycap"], depth=10, bevel=8, z=2)
plane(X(606), 46, X(962), 76, M["screen"], z=13)
text("OUTPUT", X(1190), 53, 16, M["print_dark"], track=0.3)
for i in range(14):
    x0 = X(1238) + i * 10.4; box(x0, 45, x0 + 7.6, 61, M["meter_on"] if i < 10 else M["meter_off"], depth=2, bevel=1, z=7)
vline(X(1410), 26, 80, M["rule_dark"], z=7)
text("TRIM", X(1457), 18, 15, M["print_dark"], track=0.3)
knob(X(1457), 60, 18, 0, "trim")
text("0.0 dB", X(1523), 53, 18, M["print_dark"], track=0.05)

# ---------------- control deck ----------------
box(X(10), 672, X(1558), 985, M["ui_walnut"], depth=10, bevel=16, z=0)
PANEL = box(X(22), 684, X(1546), 973, M["panel"], depth=12, bevel=10, z=0)
for sx, sy in ((X(40), 698), (X(1528), 698), (X(40), 946), (X(1528), 946)): screw(sx, sy)
for name, cx, rl in (("VOICE", X(198), 120), ("FEEL", X(502), 125), ("SPACE", X(795), 117)):
    text(name, cx, 705, 27, M["print_cream"], track=0.22)
    hline(cx - rl, cx - 52, 705, M["print_cream"], w=1.2, z=14); hline(cx + 52, cx + rl, 705, M["print_cream"], w=1.2, z=14)
for x in (X(350), X(655), X(945), X(1235)): vline(x, 700, 876, M["print_cream"], w=1.2, z=14)
for name, cx, val, r, ang, st in (("TONE", X(130), "65 %", 29.0, 35, "dome"), ("PICK", X(265), "40 %", 29.0, -50, "dome"),
                                  ("GLIDE", X(433), "145 ms", 44.4, 30, "scallop"), ("SWELL", X(572), "35 %", 44.4, 25, "scallop"),
                                  ("ECHO", X(733), "18 %", 38, -100, "space"), ("REVERB", X(860), "28 %", 38, -68, "space")):
    knob(*REF_CENTRE[name], r, ang, st)
    text(name, cx, 835, 25, M["print_cream"], track=0.1, z=14)
    pill(cx, 864, 84 if "ms" in val else 72, 29, M["print_cream"], line=1.4, z=14)
    text(val, cx, 864, 22, M["print_cream"], track=0.02, z=14)
for lab, cx, on in (("A", X(1005), True), ("B", X(1092), True), ("C", X(1178), False)):
    text(lab, cx, 705, 27, M["print_cream"], track=0, z=14); led(cx, 735, on)
    pedal_block(cx, 757, 866, 64, on)
for lab, cx in (("LKL", X(1328), ), ("LKR", X(1453), )) if False else (("LKL", X(1328)), ("LKR", X(1453))):
    text(lab, cx, 705, 27, M["print_cream"], track=0.05, z=14); lever(cx, 742)
hline(X(55), X(1515), 893, M["print_cream"], w=1.0, z=14)
text("MODE", X(127), 930, 18, M["print_cream"], track=0.3, z=14)
box(X(186), 913, X(460), 948, M["keycap"], depth=4, bevel=5, z=13); pill(X(323), 930.5, X(460) - X(186), 35, M["print_cream"], line=1.2, z=16)
text("Smart Auto-Pedal E9", X(205), 931, 19, M["print_cream"], track=0.0, align="LEFT", z=18)
tri(X(435), 931, 13, M["print_cream"], False, z=18).rotation_euler = (0, 0, -math.pi / 2)
vline(X(519), 913, 948, M["print_cream"], w=1.2, z=14)
text("VOLUME PEDAL", X(655), 930, 18, M["print_cream"], track=0.3, z=14)
box(X(762), 925, X(1160), 935, M["track"], depth=3, bevel=4, z=14)
for i in range(13): vline(X(775) + i * 30.5 * 1.02, 942, 950, M["print_cream"], w=1.0, z=14)
box(X(1027), 914, X(1058), 946, M["cream_knob"], depth=12, bevel=4, z=15)
pill(X(1216), 930, X(1252) - X(1180), 30, M["print_cream"], line=1.2, z=14); text("72 %", X(1216), 930, 18, M["print_cream"], track=0.02, z=14)
vline(X(1290), 913, 948, M["print_cream"], w=1.2, z=14)
pill(X(1407), 930, X(1482) - X(1332), 34, M["print_cream"], line=1.3, z=14); text("MIDI LEARN", X(1407), 930, 17, M["print_cream"], track=0.2, z=14)

# ---------------- lights ----------------
def area(loc, energy, size, aim, color=(1, 0.86, 0.7), parent=None):
    l = bpy.data.lights.new("A", "AREA"); l.energy = energy; l.size = size; l.color = color
    o = bpy.data.objects.new("A", l); coll.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    if parent: o.parent = parent
    return o
area((-1.6, -0.6, 2.4), 520, 1.6, (0, 0, 0.5), SK["key"])      # key upper left, per skin
area((1.8, -1.2, 1.6), 140, 2.0, (0, 0, 0.5), (1, 0.93, 0.85))   # fill
area((0.0, 1.6, 2.0), 260, 1.4, (0, 0, 0.6), (1, 0.8, 0.6))    # rim on chrome
lu = bpy.data.lights.new("UI", "AREA"); lu.energy = 0.32; lu.size = 0.7; lu.color = (1, 0.9, 0.78)
lo = bpy.data.objects.new("UILight", lu); coll.objects.link(lo); lo.parent = cam; lo.location = (0.0, 0.1, -0.1)
lo.rotation_euler = (Vector((0, 0, -OD)) - Vector(lo.location)).to_track_quat("-Z", "Y").to_euler()
# Deck knobs: lit from above like the approved screen (bright face, darker lower rim) by a knob-only key light
# (Cycles light linking); the flat UI light is excluded from them so they don't read flat.
KB = bpy.data.collections.new("KnobBodiesIn"); KBX = bpy.data.collections.new("KnobBodiesEx")
for st_, px_, py_, rpx_, root_, d_ in KNOBS:
    if st_ == "trim": continue
    for o in root_.children_recursive:
        if o.type == "MESH" and not o.name.startswith("Tick"): KB.objects.link(o); KBX.objects.link(o)
for co in KBX.collection_objects: co.light_linking.link_state = "EXCLUDE"
for o in list(coll.objects):      # room lights + flat UI light: not on the deck knobs
    if o.type == "LIGHT": o.light_linking.receiver_collection = KBX
KEY_E = float(os.environ.get("KNOB_KEY", "0.05")); KEY_EL = float(os.environ.get("KNOB_KEY_EL", "18"))
FILL_E = float(os.environ.get("KNOB_FILL", "0.003"))
def knob_light(name, energy, el, d_px, sx, sy, col, ax=500, ay=777, recv=None):
    l = bpy.data.lights.new(name, "AREA"); l.shape = "RECTANGLE"; l.size = sx * K; l.size_y = sy * K; l.energy = energy; l.color = col
    o = bpy.data.objects.new(name, l); coll.objects.link(o); o.parent = cam
    aim = cpx(ax, ay, PANEL_Z * K)[0]; e_ = math.radians(el); d = d_px * K
    o.location = aim + Vector((0, math.cos(e_) * d, math.sin(e_) * d))
    o.rotation_euler = (aim - o.location).to_track_quat("-Z", "X").to_euler()
    o.light_linking.receiver_collection = recv or KB; return o
for st_, px_, py_, rpx_, root_, d_ in KNOBS:     # identical soft top strip centred on each deck knob, linked to it alone
    if st_ == "trim": continue
    own = bpy.data.collections.new("KnobOwn")
    for o in root_.children_recursive:
        if o.type == "MESH" and not o.name.startswith("Tick"): own.objects.link(o)
    el_ = float(os.environ.get("KNOB_KEY_EL_" + st_.upper(), {"dome": 30}.get(st_, KEY_EL)))
    e_ = float(os.environ.get("KNOB_KEY_" + st_.upper(), {"dome": 0.038, "space": 0.018}.get(st_, KEY_E)))
    knob_light("KnobKey", e_, el_, 260, 1000, 90, (1, 0.93, 0.84), px_, py_, own)
knob_light("KnobFill", FILL_E, 88, 400, 1200, 400, (1, 0.95, 0.9))         # weak frontal fill
w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True; nt = w.node_tree
env = nt.nodes.new("ShaderNodeTexEnvironment"); env.image = bpy.data.images.load(r"C:/Users/Joshua/Documents/farwilder/experiments/character_lab/downloads/hdri/studio_small_09_4k.hdr")
nt.links.new(env.outputs[0], nt.nodes["Background"].inputs[0]); nt.nodes["Background"].inputs[1].default_value = 0.25

sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
p = bpy.context.preferences.addons["cycles"].preferences; p.compute_device_type = "OPTIX"; p.get_devices()
for dv in p.devices: dv.use = dv.type == "OPTIX"
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.7
sc.render.filepath = OUT
def spin_objs(root): return [o for o in root.children_recursive if o.type == "MESH" and not o.name.startswith("Tick")]
if os.environ.get("NO_KNOBS"):   # plugin background plate: ticks and knob shadows stay, knob bodies come from strips
    for st, px, py, rpx, root, d in KNOBS:
        for o in spin_objs(root): o.visible_camera = False   # still casts its shadow and lights the panel
STRIP = os.environ.get("STRIP")   # "style:frames": filmstrip of the first knob of that style, knob only (shadow is in the plate)
if STRIP:
    st, nfr = STRIP.split(":")[0], int(STRIP.split(":")[1])
    st_, px, py, rpx, root, d = [kk for kk in KNOBS if kk[0] == st][0]
    keep = set(spin_objs(root))
    for o in sc.objects:
        if o.type in ("MESH", "CURVE", "FONT") and o not in keep: o.visible_camera = False   # still seen in reflections
    sc.render.film_transparent = True; sc.render.image_settings.color_mode = "RGBA"
    # integer-pixel border (at render scale) so every frame is the same even size, centred on the knob
    hw = math.ceil(rpx * 1.32 * RES); cx_, cy_ = round(px * RES), round(py * RES)
    os.environ["BORDER"] = "%f,%f,%f,%f" % ((cx_ - hw) / RES, (cy_ - hw) / RES, (cx_ + hw) / RES, (cy_ + hw) / RES)
    import json
    json.dump(dict(style=st, frames=nfr, frame_px=2 * hw, scale=RES, origin_px=[(cx_ - hw) / RES, (cy_ - hw) / RES],
                   centre_px=[cx_ / RES, cy_ / RES]), open(OUT + ".json", "w"))
    PASS = os.environ.get("KNOB_PASS")   # data passes for the reference-fitted shading: normal | id | ao
    if PASS:
        R_ = rpx * cpx(px, py, 0)[1]
        def emit_mat(name, build_fn):
            m = bpy.data.materials.new(name); m.use_nodes = True; nt_ = m.node_tree; nt_.nodes.clear()
            out_ = nt_.nodes.new("ShaderNodeOutputMaterial"); em = nt_.nodes.new("ShaderNodeEmission")
            nt_.links.new(build_fn(nt_), em.inputs["Color"]); nt_.links.new(em.outputs[0], out_.inputs[0]); return m
        def n_fn(nt_):
            g = nt_.nodes.new("ShaderNodeNewGeometry"); vt = nt_.nodes.new("ShaderNodeVectorTransform")
            vt.vector_type = "NORMAL"; vt.convert_from = "WORLD"; vt.convert_to = "CAMERA"
            nt_.links.new(g.outputs["Normal"], vt.inputs[0])
            ma = nt_.nodes.new("ShaderNodeVectorMath"); ma.operation = "MULTIPLY_ADD"
            ma.inputs[1].default_value = (0.5, 0.5, 0.5); ma.inputs[2].default_value = (0.5, 0.5, 0.5)
            nt_.links.new(vt.outputs[0], ma.inputs[0]); return ma.outputs[0]
        def ao_fn(nt_):
            a = nt_.nodes.new("ShaderNodeAmbientOcclusion"); a.samples = 32
            a.inputs["Distance"].default_value = R_ * float(os.environ.get("KNOB_PASS_AO", "0.35")); return a.outputs["AO"]
        def id_fn(col):
            def f(nt_):
                r = nt_.nodes.new("ShaderNodeRGB"); r.outputs[0].default_value = col; return r.outputs[0]
            return f
        for o in keep:
            if PASS == "normal": mat = emit_mat("PassN", n_fn)
            elif PASS == "ao": mat = emit_mat("PassAO", ao_fn)
            else: mat = emit_mat("PassID", id_fn((1, 0, 0, 1) if o.name.startswith("Pointer") else
                                                  (0, 0, 1, 1) if o.name.startswith("Skirt") else (0, 1, 0, 1)))
            o.data.materials.clear(); o.data.materials.append(mat)
        sc.view_settings.view_transform = "Raw"; sc.view_settings.look = "None"; sc.view_settings.exposure = 0
        sc.cycles.use_denoising = False; sc.cycles.samples = int(os.environ.get("KNOB_PASS_SAMPLES", "64"))
        sc.render.image_settings.color_depth = "16"
    spin = [c for c in root.children_recursive if c.name.startswith("Spin")][0]
    sc.frame_start, sc.frame_end = 0, nfr - 1
    for f in range(nfr):      # key every frame so the angle is exactly linear whatever the interpolation
        a_ = float(os.environ["STRIP_ANGLE"]) if os.environ.get("STRIP_ANGLE") else -135 + 270 * f / max(1, nfr - 1)
        KL.spin(root, a_); spin.keyframe_insert("rotation_euler", index=2, frame=f)
if os.environ.get("BORDER"):   # px box "x0,y0,x1,y1" in 1600x1000 space: render only that region, cropped
    x0, y0, x1, y1 = (float(v) for v in os.environ["BORDER"].split(","))
    sc.render.use_border = True; sc.render.use_crop_to_border = True
    sc.render.border_min_x, sc.render.border_max_x = x0 / W_PX, x1 / W_PX
    sc.render.border_min_y, sc.render.border_max_y = 1 - y1 / H_PX, 1 - y0 / H_PX
if STRIP:
    sc.render.filepath = OUT + "_####"
    bpy.ops.render.render(animation=True); raise SystemExit
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")
