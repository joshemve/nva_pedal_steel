"""Procedural E9 single-neck 10-string pedal steel guitar for Blender 5.0 (metres).
Player's view: keyhead and nut at -X (left), changer and end plate at +X (right), player at -Y.
Real proportions: 24 in scale, ~0.86 m body, ~0.2 m deck, 8.5 mm string spacing.
"""
import bpy, math, os
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
TEX = os.path.join(HERE, "..", "textures")

SCALE = 0.6096
X_BRIDGE = 0.392          # changer contact point
X_NUT = X_BRIDGE - SCALE  # -0.2176
BODY_X = (-0.44, 0.44)
BODY_W = 0.20
APRON = 0.095
STR_N, STR_SP = 10, 0.0085
Z_STR_BRIDGE, Z_STR_NUT = 0.0165, 0.0135

def fret_x(n):
    return X_NUT + SCALE * (1 - 2 ** (-n / 12))

# ---------- materials ----------
def _img(name, colorspace="sRGB"):
    p = os.path.join(TEX, name)
    if not os.path.exists(p):
        return None
    im = bpy.data.images.load(p, check_existing=True)
    im.colorspace_settings.name = colorspace
    return im

def mat(name, base=(0.8, 0.8, 0.8), metal=0.0, rough=0.5, tex=None, tex_scale=(1, 1), aniso=0.0,
        coat=0.0, coat_rough=0.05, tint=None, rough_tex=None, bump=0.0, sat=1.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs[0], out.inputs[0])
    bsdf.inputs["Base Color"].default_value = (*base, 1)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if aniso:
        bsdf.inputs["Anisotropic"].default_value = aniso
    if coat:
        bsdf.inputs["Coat Weight"].default_value = coat
        bsdf.inputs["Coat Roughness"].default_value = coat_rough
    if tex:
        im = _img(tex)
        if im:
            tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
            mp.inputs["Scale"].default_value = (tex_scale[0], tex_scale[1], 1)
            ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = im
            nt.links.new(tc.outputs["UV"], mp.inputs[0]); nt.links.new(mp.outputs[0], ti.inputs[0])
            col = ti.outputs["Color"]
            if sat != 1.0 or tint:
                hsv = nt.nodes.new("ShaderNodeHueSaturation"); hsv.inputs["Saturation"].default_value = sat
                nt.links.new(col, hsv.inputs["Color"]); col = hsv.outputs[0]
                if tint:
                    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MULTIPLY"
                    mix.inputs[0].default_value = 1.0; mix.inputs[7].default_value = (*tint, 1)
                    nt.links.new(col, mix.inputs[6]); col = mix.outputs[2]
            nt.links.new(col, bsdf.inputs["Base Color"])
            if bump:
                bw = nt.nodes.new("ShaderNodeRGBToBW"); bn = nt.nodes.new("ShaderNodeBump")
                bn.inputs["Strength"].default_value = bump; bn.inputs["Distance"].default_value = 0.0005
                nt.links.new(ti.outputs["Color"], bw.inputs[0]); nt.links.new(bw.outputs[0], bn.inputs["Height"])
                nt.links.new(bn.outputs[0], bsdf.inputs["Normal"])
            if rough_tex:
                ri = nt.nodes.new("ShaderNodeTexImage"); ri.image = _img(rough_tex, "Non-Color")
                nt.links.new(mp.outputs[0], ri.inputs[0])
                mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[3].default_value = 0.05; mr.inputs[4].default_value = 0.75
                nt.links.new(ri.outputs["Color"], mr.inputs[0]); nt.links.new(mr.outputs[0], bsdf.inputs["Roughness"])
    return m

def materials():
    return {
        "walnut": mat("Walnut", tex="walnut_1.png", tex_scale=(1, 1), rough=0.32, coat=0.6, coat_rough=0.12, bump=0.15),
        "chrome": mat("Chrome", base=(0.92, 0.92, 0.93), metal=1.0, rough=0.06),
        "alu": mat("BrushedAlu", tex="brushed_alu_1.png", metal=1.0, rough=0.28, aniso=0.6),
        "nickel": mat("StringNickel", base=(0.78, 0.76, 0.72), metal=1.0, rough=0.22),
        "black": mat("BlackPlastic", base=(0.018, 0.018, 0.02), rough=0.38),
        "rubber": mat("Rubber", base=(0.02, 0.02, 0.02), rough=0.85),
        "fret": mat("Fretboard", tex="fretboard_albedo.png", rough_tex="fretboard_rough.png", coat=0.3, coat_rough=0.2),
        "pole": mat("PolePiece", base=(0.75, 0.75, 0.76), metal=1.0, rough=0.3),
    }

# ---------- geometry helpers ----------
def _link(ob, coll):
    coll.objects.link(ob)
    return ob

def box(name, size, loc, m, coll, bevel=0.0, segs=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    ob = bpy.context.active_object; ob.name = name
    ob.scale = size; bpy.ops.object.transform_apply(scale=True)
    for c in ob.users_collection: c.objects.unlink(ob)
    coll.objects.link(ob)
    ob.data.materials.append(m)
    if bevel:
        b = ob.modifiers.new("bevel", "BEVEL"); b.width = bevel; b.segments = segs; b.limit_method = "ANGLE"
    for p in ob.data.polygons: p.use_smooth = True
    return ob

def cyl(name, r, depth, loc, rot, m, coll, verts=48, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=loc, rotation=rot)
    ob = bpy.context.active_object; ob.name = name
    for c in ob.users_collection: c.objects.unlink(ob)
    coll.objects.link(ob); ob.data.materials.append(m)
    for p in ob.data.polygons: p.use_smooth = True
    if bevel:
        b = ob.modifiers.new("bevel", "BEVEL"); b.width = bevel; b.segments = 3; b.limit_method = "ANGLE"
    return ob

def uv_box_project(ob, scale=1.0):
    """World-size UVs along X for wood grain running the length."""
    me = ob.data
    if not me.uv_layers: me.uv_layers.new()
    uv = me.uv_layers.active.data
    for poly in me.polygons:
        n = poly.normal
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            if abs(n.z) > max(abs(n.x), abs(n.y)): u, w = v.x, v.y
            elif abs(n.y) > abs(n.x): u, w = v.x, v.z
            else: u, w = v.y, v.z
            uv[li].uv = (u * scale, w * scale)

# ---------- the instrument ----------
def build(coll, M, bar_fret=7.0, with_bar=True):
    x0, x1 = BODY_X
    L = x1 - x0
    # body: apron box (walnut) with a deck slightly overhanging
    body = box("Body", (L, BODY_W, APRON), ((x0 + x1) / 2, 0, -APRON / 2), M["walnut"], coll, bevel=0.004)
    uv_box_project(body, 1.6)
    deck = box("Deck", (L - 0.01, BODY_W - 0.004, 0.006), ((x0 + x1) / 2, 0, 0.003), M["walnut"], coll, bevel=0.002)
    uv_box_project(deck, 1.6)
    # chrome end caps wrapping both ends (the rounded rails on vintage steels)
    for sx in (x0 - 0.006, x1 + 0.006):
        box("EndCap", (0.014, BODY_W + 0.006, APRON + 0.012), (sx, 0, -APRON / 2 + 0.003), M["chrome"], coll, bevel=0.006, segs=6)
    # top chrome rails along front and back edges
    for sy in (-BODY_W / 2 + 0.003, BODY_W / 2 - 0.003):
        cyl("Rail", 0.0035, L, (0, sy, 0.006), (0, math.pi / 2, 0), M["chrome"], coll)
    # fretboard (black printed) between nut and pickup
    fb_len = 0.4858
    fb = box("Fretboard", (fb_len, 0.092, 0.002), (X_NUT - 0.006 + fb_len / 2, 0, 0.007), M["fret"], coll)
    me = fb.data; me.uv_layers.new(); uv = me.uv_layers.active.data
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uv[li].uv = ((v.x + fb_len / 2) / fb_len, (v.y + 0.046) / 0.092)
    # nut roller and keyhead
    cyl("Nut", 0.004, 0.10, (X_NUT, 0, 0.0105), (math.pi / 2, 0, 0), M["chrome"], coll)
    # keyhead: low chrome housing at the left end; 10 rollers on top on a diagonal (one per string);
    # each roller's worm-gear key sticks out of the end of the guitar, staggered in two heights
    kh_x0, kh_x1 = x0 - 0.002, X_NUT - 0.016
    box("KeyheadHousing", (kh_x1 - kh_x0, 0.115, 0.016), ((kh_x0 + kh_x1) / 2, 0, 0.008), M["chrome"], coll, bevel=0.005, segs=5)
    box("KeyheadTopPlate", (kh_x1 - kh_x0 - 0.012, 0.10, 0.002), ((kh_x0 + kh_x1) / 2, 0, 0.0165), M["alu"], coll, bevel=0.0008)
    ROLL = []
    for i in range(STR_N):
        y = -(i - (STR_N - 1) / 2) * STR_SP          # string i world y
        px = kh_x1 - 0.010 - i * ((kh_x1 - kh_x0 - 0.026) / (STR_N - 1))
        cyl("Roller", 0.0042, 0.007, (px, y, 0.0205), (0, 0, 0), M["chrome"], coll, verts=32, bevel=0.0012)
        cyl("RollerCap", 0.0028, 0.002, (px, y, 0.0245), (0, 0, 0), M["chrome"], coll, verts=24, bevel=0.0006)
        ROLL.append((px, y))
        zrow = 0.005 if i % 2 == 0 else 0.0125
        ky = y * 1.15
        cyl("KeyShaft", 0.0022, 0.016, (kh_x0 - 0.007, ky, zrow), (0, math.pi / 2, 0), M["chrome"], coll, verts=16)
        box("KeyButton", (0.005, 0.010, 0.0085), (kh_x0 - 0.0175, ky, zrow), M["chrome"], coll, bevel=0.0028, segs=4)
    # pickup
    pu = box("Pickup", (0.024, 0.098, 0.009), (0.335, 0, 0.0095), M["black"], coll, bevel=0.002)
    for i in range(STR_N):
        y = (i - (STR_N - 1) / 2) * STR_SP
        cyl("Pole", 0.0022, 0.002, (0.335, y, 0.0145), (0, 0, 0), M["pole"], coll, verts=24)
    # changer: chrome housing and 10 fingers
    box("ChangerHousing", (0.05, 0.13, 0.012), (0.417, 0, 0.009), M["chrome"], coll, bevel=0.004)
    for i in range(STR_N):
        y = (i - (STR_N - 1) / 2) * STR_SP
        cyl("Finger", 0.0085, 0.0045, (X_BRIDGE + 0.004, y, 0.0085), (math.pi / 2, 0, 0), M["chrome"], coll, bevel=0.0008)
    # end plate (brushed alu) on the changer end
    box("EndPlate", (0.004, BODY_W - 0.01, APRON - 0.01), (x1 + 0.014, 0, -APRON / 2), M["alu"], coll, bevel=0.001)
    # strings: wound gauges get thicker toward the player
    gauges = [0.011, 0.014, 0.017, 0.022, 0.024, 0.026, 0.030, 0.036, 0.044, 0.054]  # E9 set, high (far) to low (near)
    for i in range(STR_N):
        y = (i - (STR_N - 1) / 2) * STR_SP
        r = gauges[STR_N - 1 - i] * 0.0254 / 2 if False else gauges[i] * 0.0254 / 2
        a = Vector((X_NUT, -y, Z_STR_NUT)); b = Vector((X_BRIDGE, -y, Z_STR_BRIDGE))
        mid = (a + b) / 2; d = b - a
        s = cyl("String", r, d.length, mid, (0, math.pi / 2 - math.atan2(d.z, d.x), 0), M["nickel"], coll, verts=12)
        # nut to its roller on the keyhead
        rx, ry = ROLL[i]
        a2 = Vector((rx, ry - r - 0.004 * (1 if ry >= 0 else -1) * 0, 0.0215)); b2 = a
        d2 = b2 - a2; m2 = (a2 + b2) / 2
        ob = cyl("StringHead", r, d2.length, m2, (0, 0, 0), M["nickel"], coll, verts=12)
        ob.rotation_mode = "QUATERNION"; ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d2.normalized())
    # steel bar resting on the strings
    if with_bar:
        bx = fret_x(bar_fret)
        t = (bx - X_NUT) / SCALE
        zs = Z_STR_NUT + t * (Z_STR_BRIDGE - Z_STR_NUT)
        r = 0.011
        cyl("SteelBar", r, 0.083, (bx, -0.004, zs + 0.0012 + r), (math.pi / 2, 0, 0), M["chrome"], coll, verts=64, bevel=0.003)
    # rubber feet
    for fx in (x0 + 0.04, x1 - 0.04):
        for fy in (-0.07, 0.07):
            cyl("Foot", 0.012, 0.012, (fx, fy, -APRON - 0.006), (0, 0, 0), M["rubber"], coll, bevel=0.003)
    return body
