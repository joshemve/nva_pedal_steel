"""UI building blocks: printed text, knobs, keycaps, LED display, meters, pedals, knee levers.
UI space: faceplate in the XZ plane at y=0, camera at -Y. 1 px = 1 mm: x = px/1000, z = (1000-py)/1000."""
import bpy, math, os
from mathutils import Vector, Matrix
import steel_lib as S

FONT_SANS = r"C:\Windows\Fonts\segoeuib.ttf"

def P(px, py, y=0.0):
    return Vector((px / 1000, y, (1000 - py) / 1000))

_fonts = {}
def font(path=FONT_SANS):
    if path not in _fonts:
        _fonts[path] = bpy.data.fonts.load(path, check_existing=True)
    return _fonts[path]

def text(coll, body, loc, size, m, align="CENTER", track=0.0, parent=None, rot=(math.pi / 2, 0, 0), bold=False, valign="CENTER"):
    cu = bpy.data.curves.new("txt", "FONT"); cu.body = body; cu.font = font()
    if bold:
        cu.font_bold = font(); cu.offset = 0.0
    cu.size = size; cu.align_x = align; cu.align_y = valign; cu.space_character = 1.0 + track
    cu.resolution_u = 6
    ob = bpy.data.objects.new("T_" + body[:12], cu); coll.objects.link(ob)
    ob.location = loc; ob.rotation_euler = rot
    cu.materials.append(m)
    if parent: ob.parent = parent
    return ob

def rbox(coll, name, w, h, d, loc, m, bevel=0.002, parent=None, segs=4):
    """Box sized in UI space: w along X, h along Z, d along Y (towards camera = -Y)."""
    ob = S.box(name, (w, d, h), loc, m, coll, bevel=bevel, segs=segs)
    if parent: ob.parent = parent
    return ob

def knob(coll, M, loc, r, angle_deg, style="cream", parent=None, rot=(math.pi / 2, 0, 0)):
    """Skirted knurled knob: skirt, knurled body, domed cap, pointer line. Axis = local +Z (rotated to face -Y)."""
    root = bpy.data.objects.new("Knob", None); coll.objects.link(root)
    root.location = loc; root.rotation_euler = rot
    if parent: root.parent = parent
    body_m = M["cream_knob"] if style == "cream" else M["black_knob"]
    parts = []
    parts.append(S.cyl("KnobSkirt", r * 1.18, r * 0.18, (0, 0, r * 0.09), (0, 0, 0), M["black_knob"], coll, verts=64, bevel=r * 0.05))
    b = S.cyl("KnobBody", r, r * 0.62, (0, 0, r * 0.18 + r * 0.31), (0, 0, 0), body_m, coll, verts=96, bevel=r * 0.12)
    parts.append(b)
    # knurl: 32 shallow flutes cut by a displace-like boolean is heavy; use radial grooves as thin dark boxes
    for i in range(36):
        a = 2 * math.pi * i / 36
        g = S.box("Knurl", (r * 0.05, r * 0.03, r * 0.44), (math.cos(a) * r * 0.995, math.sin(a) * r * 0.995, r * 0.18 + r * 0.29), body_m, coll)
        g.rotation_euler = (0, 0, a); parts.append(g)
    cap = S.cyl("KnobCap", r * 0.86, r * 0.06, (0, 0, r * 0.82), (0, 0, 0), body_m, coll, verts=96, bevel=r * 0.03)
    parts.append(cap)
    pa = math.radians(angle_deg)
    ptr = S.box("Pointer", (r * 0.16, r * 0.66, r * 0.02), (0, 0, r * 0.855), M["pointer"], coll)
    ptr.location = (-math.sin(pa) * r * 0.42, math.cos(pa) * r * 0.42, r * 0.856); ptr.rotation_euler = (0, 0, pa)
    parts.append(ptr)
    for p in parts: p.parent = root
    return root

def ticks(coll, m, center, r_in, r_out, n=11, a0=-135, a1=135, parent=None, width=0.0016, rot=(math.pi / 2, 0, 0)):
    root = bpy.data.objects.new("Ticks", None); coll.objects.link(root); root.location = center; root.rotation_euler = rot
    if parent: root.parent = parent
    for i in range(n):
        a = math.radians(a0 + (a1 - a0) * i / (n - 1))
        rm = (r_in + r_out) / 2
        t = S.box("Tick", (width, r_out - r_in, 0.0004), (-math.sin(a) * rm, math.cos(a) * rm, 0), m, coll)
        t.rotation_euler = (0, 0, a); t.parent = root
    return root

def outline_rect(coll, m, cx, cz, w, h, line=0.0012, y=-0.0003, parent=None):
    """Thin rounded outline (value boxes) as four slim boxes in the XZ plane."""
    obs = []
    for (dx, dz, sw, sh) in ((0, h / 2, w, line), (0, -h / 2, w, line), (-w / 2, 0, line, h), (w / 2, 0, line, h)):
        o = S.box("Outline", (sw, 0.0004, sh), (cx + dx, y, cz + dz), m, coll, bevel=0)
        if parent: o.parent = parent
        obs.append(o)
    return obs

def emission_mat(name, color, strength):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear(); o = nt.nodes.new("ShaderNodeOutputMaterial"); b = nt.nodes.new("ShaderNodeBsdfPrincipled")
    b.inputs["Base Color"].default_value = (*color, 1); b.inputs["Emission Color"].default_value = (*color, 1)
    b.inputs["Emission Strength"].default_value = strength; b.inputs["Roughness"].default_value = 0.3
    nt.links.new(b.outputs[0], o.inputs[0]); return m

def image_emission_mat(name, path, strength=6.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear(); o = nt.nodes.new("ShaderNodeOutputMaterial"); b = nt.nodes.new("ShaderNodeBsdfPrincipled")
    ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(path, check_existing=True); ti.interpolation = "Cubic"
    nt.links.new(ti.outputs[0], b.inputs["Base Color"]); nt.links.new(ti.outputs[0], b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = strength; b.inputs["Roughness"].default_value = 0.5
    nt.links.new(b.outputs[0], o.inputs[0]); return m

def plane_uv(coll, name, w, h, loc, m, parent=None):
    bpy.ops.mesh.primitive_plane_add(size=1, location=(0, 0, 0))
    ob = bpy.context.active_object; ob.name = name
    for c in ob.users_collection: c.objects.unlink(ob)
    coll.objects.link(ob); ob.scale = (w, h, 1); ob.rotation_euler = (math.pi / 2, 0, 0); ob.location = loc
    ob.data.materials.append(m)
    if parent: ob.parent = parent
    return ob

def foot_pedal(coll, M, loc, w=0.085, l=0.12, tilt=55):
    """Chrome-framed foot pedal with ribbed rubber pad, hinged at the back, tilted toward the camera."""
    root = bpy.data.objects.new("Pedal", None); coll.objects.link(root); root.location = loc
    root.rotation_euler = (math.radians(tilt), 0, 0)
    fr = S.box("PedalFrame", (w, l, 0.02), (0, 0, 0), M["chrome"], coll, bevel=0.008, segs=6)
    pad = S.box("PedalPad", (w * 0.84, l * 0.84, 0.008), (0, -0.002, 0.012), M["rubber_rib"], coll, bevel=0.003)
    S.uv_box_project(pad, 9.0)
    hinge = S.cyl("Hinge", 0.008, w * 0.7, (0, l / 2 + 0.004, -0.004), (0, math.pi / 2, 0), M["chrome"], coll)
    for o in (fr, pad, hinge): o.parent = root
    return root

def knee_lever(coll, M, loc, mirror=1):
    """Knee lever lying on a console plate (local XY = plate surface, +Z = up off the plate):
    pivot bracket, chrome bent arm sweeping down and outward, rubber-faced paddle."""
    root = bpy.data.objects.new("KneeLever", None); coll.objects.link(root); root.location = loc
    br = S.cyl("LeverPivot", 0.014, 0.012, (0, 0, 0.006), (0, 0, 0), M["chrome"], coll, bevel=0.003); br.parent = root
    bolt = S.cyl("LeverBolt", 0.006, 0.004, (0, 0, 0.0135), (0, 0, 0), M["black_knob"], coll, verts=6); bolt.parent = root
    cu = bpy.data.curves.new("arm", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 0.0055; cu.bevel_resolution = 6
    sp = cu.splines.new("BEZIER"); pts = [(0, 0, 0.009), (0.018 * mirror, -0.022, 0.012), (0.045 * mirror, -0.045, 0.012)]
    sp.bezier_points.add(len(pts) - 1)
    for bp, p in zip(sp.bezier_points, pts):
        bp.co = p; bp.handle_left_type = bp.handle_right_type = "AUTO"
    arm = bpy.data.objects.new("LeverArm", cu); coll.objects.link(arm); cu.materials.append(M["chrome"]); arm.parent = root
    padf = S.box("LeverPadFrame", (0.05, 0.034, 0.012), (0.055 * mirror, -0.058, 0.009), M["chrome"], coll, bevel=0.005); padf.parent = root
    pad = S.box("LeverPad", (0.042, 0.026, 0.006), (0.055 * mirror, -0.058, 0.0145), M["rubber_rib"], coll, bevel=0.002)
    S.uv_box_project(pad, 9.0); pad.parent = root
    return root

def flat_led(coll, M, xy, on, parent):
    """LED with chrome bezel on a plate (local +Z up)."""
    b = S.cyl("LEDBezel", 0.0068, 0.003, (xy[0], xy[1], 0.0015), (0, 0, 0), M["chrome"], coll, bevel=0.0012); b.parent = parent
    l = S.cyl("LED", 0.0048, 0.004, (xy[0], xy[1], 0.003), (0, 0, 0), M["led_on"] if on else M["led_off"], coll, verts=32, bevel=0.0018); l.parent = parent
    return l

def led(coll, M, loc, on):
    S.cyl("LEDBezel", 0.0075, 0.004, loc, (math.pi / 2, 0, 0), M["chrome"], coll, bevel=0.0015)
    l = loc + Vector((0, -0.003, 0))
    return S.cyl("LED", 0.0055, 0.004, l, (math.pi / 2, 0, 0), M["led_on"] if on else M["led_off"], coll, verts=32, bevel=0.002)
