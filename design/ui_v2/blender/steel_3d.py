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
# body shape (Joshua 2026-11-12, round29 s3_1 / s6_3): "box" (the approved steel), "bullnose" (60s blonde: half-round front
# apron, bell feet, light fretboard on maple), "double" (E9/C6 double neck: a second neck behind the playable one, eight
# pedals; rendered through a slightly wider lens on the same camera, so the plugin maps it with one scale + offset)
SHAPE = os.environ.get("STEEL_SHAPE", "box")
if SHAPE != "box":
    os.environ.setdefault("CAB_UV", "0"); os.environ.setdefault("HL", "0")   # the UV sheets are laid out for the box
# settings matched to the approved concepts by measuring hue/saturation/brightness (Joshua 2026-11-11); env still overrides
TUNED = {
    "koa": dict(CAB_SAT=0.95, CAB_VAL=1.02, CAB_HUE=0.524, CAB_GAMMA=1.35, CAB_COAT=0.10, FRONT_GLINT=0.6, HL=7, HL_TEXP=0.5, HL_CORE=1.2, HL_GAMMA=1.0, HL_TR=1.0, HL_TG=0.45, HL_TB=0.15),   # red sparkle's gloss (Joshua)
    "maple": dict(HL=7, HL_TEXP=0.5, HL_CORE=1.2, HL_GAMMA=1.0, HL_TR=1.0, HL_TG=0.75, HL_TB=0.45),
    "rosewood": dict(HL=7, HL_TEXP=0.5, HL_CORE=1.2, HL_GAMMA=1.0, HL_TR=1.0, HL_TG=0.35, HL_TB=0.2),
    "redsparkle": dict(RS_DARK=0.38, RS_HI=3.4, RS_SCALE=1900, FLAKE_COAT=0.0, TOP_GLINT=1.6, FRONT_GLINT=0.0, FLAKE_EDGE_TINT=1, HL=9.0, HL_CORE=2.2, HL_GAMMA=1.0, TOP_TRIM=0,
                       BOTTOM_BAR=0, TRIM_ROUGH=0.04),   # one mirror band in the middle of the front only (Joshua)
    "pearloid": dict(CAB_SAT=0.56, CAB_VAL=1.08, CAB_HUE=0.52, CAB_GAMMA=1.25, CAB_TOP_VAL=0.55, HL=1.5, TOP_TRIM=0, EDGE_TRIM=1, HL_GAMMA=1.0, HL_TEXP=6, TILE_GLINT=2.0,
                     HL_CORE=0.01, HL_TR=1, HL_TG=1, HL_TB=1),   # glossy: white flashes on the bright tiles (Joshua 2026-11-11)
}
for _k, _v in TUNED.get(FINISH, {}).items(): os.environ.setdefault(_k, str(_v))
# (darkest, middle, lightest) of the grain, lacquer roughness
WOODS = {"rosewood": ((0.008, 0.003, 0.002), (0.055, 0.011, 0.004), (0.30, 0.042, 0.010)),
         "maple": ((0.50, 0.30, 0.11), (0.74, 0.50, 0.22), (0.88, 0.66, 0.34)),
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
    nt.links.new(mix.outputs[0], ramp.inputs[0])
    fig = None
    if FINISH == "maple":                     # birdseye: small dark eyes scattered through pale, softly figured maple
        v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = 150.0; nt.links.new(tc.outputs["Object"], v.inputs["Vector"])
        fig = nt.nodes.new("ShaderNodeMapRange"); fig.inputs[1].default_value = 0.02; fig.inputs[2].default_value = 0.16
        fig.inputs[3].default_value = 0.50; fig.inputs[4].default_value = 1.0; nt.links.new(v.outputs["Distance"], fig.inputs[0])
    elif FINISH == "koa":                     # curl: soft bands across the grain that catch the light
        w = nt.nodes.new("ShaderNodeTexWave"); w.inputs["Scale"].default_value = 26.0; w.inputs["Distortion"].default_value = 5.0
        w.inputs["Detail"].default_value = 2.0; nt.links.new(tc.outputs["Object"], w.inputs["Vector"])
        fig = nt.nodes.new("ShaderNodeMapRange"); fig.inputs[3].default_value = 0.62; fig.inputs[4].default_value = 1.25
        nt.links.new(w.outputs["Fac"], fig.inputs[0])
    if fig is None: nt.links.new(ramp.outputs[0], b.inputs["Base Color"])
    else:
        fm = nt.nodes.new("ShaderNodeMix"); fm.data_type = "RGBA"; fm.blend_type = "MULTIPLY"; fm.inputs[0].default_value = 1.0
        nt.links.new(ramp.outputs[0], fm.inputs[6]); nt.links.new(fig.outputs[0], fm.inputs[7]); nt.links.new(fm.outputs[2], b.inputs["Base Color"])
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
    m = mat(name, base, metal, 0.25, coat=E("FLAKE_COAT", 1.0)); nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    if metal > 0:                              # sparkle: tiny hard flakes
        v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = cell; src = "Color"
    else:                                      # pearloid: soft swirled clouds, not cells
        v = nt.nodes.new("ShaderNodeTexNoise"); v.inputs["Scale"].default_value = cell; v.inputs["Detail"].default_value = 2.5
        v.inputs["Distortion"].default_value = 3.0; src = "Fac"; mr0 = (0.30, 0.70)
    nt.links.new(tc.outputs["Object"], v.inputs["Vector"])
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[3].default_value = lo; mr.inputs[4].default_value = hi
    nt.links.new(v.outputs[src], mr.inputs[0])
    if metal <= 0: mr.inputs[1].default_value, mr.inputs[2].default_value = mr0
    else:                                      # each flake tilted its own way under the smooth clear coat, so they glint
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        sub = nt.nodes.new("ShaderNodeVectorMath"); sub.operation = "SUBTRACT"; sub.inputs[1].default_value = (0.5, 0.5, 0.5)
        nt.links.new(v.outputs["Color"], sub.inputs[0])
        scl = nt.nodes.new("ShaderNodeVectorMath"); scl.operation = "SCALE"; scl.inputs["Scale"].default_value = E("FLAKE_TILT", 0.9)
        nt.links.new(sub.outputs[0], scl.inputs[0])
        add = nt.nodes.new("ShaderNodeVectorMath"); add.operation = "ADD"
        nt.links.new(geo.outputs["Normal"], add.inputs[0]); nt.links.new(scl.outputs[0], add.inputs[1])
        nrm = nt.nodes.new("ShaderNodeVectorMath"); nrm.operation = "NORMALIZE"; nt.links.new(add.outputs[0], nrm.inputs[0])
        nt.links.new(nrm.outputs[0], b.inputs["Normal"]); b.inputs["Roughness"].default_value = E("FLAKE_ROUGH", 0.08)
    mx_ = nt.nodes.new("ShaderNodeMix"); mx_.data_type = "RGBA"; mx_.blend_type = "MULTIPLY"; mx_.inputs[0].default_value = 1.0
    mx_.inputs[6].default_value = (*base, 1); nt.links.new(mr.outputs[0], mx_.inputs[7]); nt.links.new(mx_.outputs[2], b.inputs["Base Color"])
    if metal > 0 and E("FLAKE_EDGE_TINT", 0.0) > 0:   # metal flakes keep their red at grazing angles instead of going white
        nt.links.new(mx_.outputs[2], b.inputs["Specular Tint"])
    return m


def photo_wood(path, sat=0.92, val=0.78, hue=0.5, scale=1.0):
    """a real veneer photo (textures/wood) box-projected in object space, grain along X, under a gloss coat"""
    m = mat("Veneer", (0.1, 0.04, 0.02), 0.0, 0.30, coat=0.7); nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Coat Roughness"].default_value = 0.05
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.55 * scale, 2.3 * scale, 2.3 * scale); mp.inputs["Location"].default_value = (0.13, 0.31, 0.47)
    nt.links.new(tc.outputs["Object"], mp.inputs[0])
    tx = nt.nodes.new("ShaderNodeTexImage"); tx.image = bpy.data.images.load(path); tx.projection = "BOX"; tx.projection_blend = 0.2
    tx.extension = "MIRROR"; nt.links.new(mp.outputs[0], tx.inputs["Vector"])
    hs = nt.nodes.new("ShaderNodeHueSaturation"); hs.inputs["Saturation"].default_value = sat; hs.inputs["Value"].default_value = val; hs.inputs["Hue"].default_value = hue
    nt.links.new(tx.outputs["Color"], hs.inputs["Color"]); nt.links.new(hs.outputs["Color"], b.inputs["Base Color"])
    # pores and grain lines catch the light a little differently from the flat lacquer
    bw = nt.nodes.new("ShaderNodeRGBToBW"); nt.links.new(tx.outputs["Color"], bw.inputs[0])
    bm = nt.nodes.new("ShaderNodeBump"); bm.inputs["Strength"].default_value = 0.06; bm.inputs["Distance"].default_value = 0.0004
    nt.links.new(bw.outputs[0], bm.inputs["Height"]); nt.links.new(bm.outputs["Normal"], b.inputs["Normal"])
    return m


WOOD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "textures", "wood")
WOOD_TEX = os.path.join(WOOD_DIR, "rosewood_1_1.png")
PEARL_TEX = os.path.join(WOOD_DIR, "pearloid_2_1.png")    # real mother-of-pearl celluloid (approved ref p3d_pearloid_4)
KOA_TEX = os.path.join(WOOD_DIR, "koa_1_1.png")           # real figured koa (the procedural curl read as stripes: Joshua)
MAPLE_TEX = os.path.join(WOOD_DIR, "maple_1_1.png")       # approved ref p3d_maple_1: honey birdseye/burl maple
if SHAPE == "bullnose":   # the 60s blonde (round29 s3_1) is flamed maple, not burl
    MAPLE_TEX = os.path.join(WOOD_DIR, "flamemaple_1_1.png")
    for _k, _v in dict(MAPLE_SAT=1.18, MAPLE_VAL=0.90, MAPLE_HUE=0.5, MAPLE_SCALE=1.0).items(): os.environ.setdefault(_k, str(_v))
WALNUT_TEX = os.path.join(WOOD_DIR, "walnut_1_1.png")     # figured walnut of the double-neck concept (round29 s6_3)
M_WOOD = (photo_wood(WALNUT_TEX, E("WALNUT_SAT", 1.0), E("WALNUT_VAL", 0.80), 0.5, E("WALNUT_SCALE", 1.0)) if SHAPE == "double" and os.path.exists(WALNUT_TEX) else
          photo_wood(WOOD_TEX) if FINISH == "rosewood" and os.path.exists(WOOD_TEX) else
          photo_wood(KOA_TEX, E("KOA_SAT", 1.12), E("KOA_VAL", 0.93), E("KOA_HUE", 0.515), E("KOA_SCALE", 1.8)) if FINISH == "koa" and os.path.exists(KOA_TEX) else
          photo_wood(PEARL_TEX, E("PEARL_SAT", 0.42), E("PEARL_VAL", 1.08), E("PEARL_HUE", 0.512), E("PEARL_SCALE", 2.2)) if FINISH == "pearloid" and os.path.exists(PEARL_TEX) else
          photo_wood(MAPLE_TEX, E("MAPLE_SAT", 0.86), E("MAPLE_VAL", 1.17), E("MAPLE_HUE", 0.532), E("MAPLE_SCALE", 1.3)) if FINISH == "maple" and os.path.exists(MAPLE_TEX) else
          flake_mat("RedSparkle", tuple(c * E("RS_DARK", 1.0) for c in (0.26, 0.004, 0.006)), E("RS_SCALE", 1100.0), 0.55, E("RS_HI", 2.4), 0.90) if FINISH == "redsparkle" else
          flake_mat("Pearloid", (0.88, 0.85, 0.78), 22.0, 0.66, 1.08, 0.0) if FINISH == "pearloid" else wood_mat())
M_ALU = mat("Aluminium", (0.74, 0.73, 0.72), 1.0, 0.30)   # satin cast aluminium (keyhead, changer, endplates)   # polished: crisp reflections (Joshua: ours felt hazed)          # polished cast aluminium (satin)
M_ALU_B = mat("AluBrushed", (0.60, 0.60, 0.61), 1.0, 0.20)        # neck, pedal bar
M_CHROME = mat("Chrome", (0.74, 0.73, 0.72), 1.0, 0.04)
# pedal rod, pull rods and collars: brass on the koa steel (STEEL_METAL=brass)
M_ROD = mat("RodBrass", (0.78, 0.56, 0.24), 1.0, 0.10) if os.environ.get("STEEL_METAL") == "brass" else M_CHROME
# pull rods: bright satin steel, not mirror chrome (chrome only showed the dark room: darker than the reference, Joshua)
M_PULL = M_ROD if os.environ.get("STEEL_METAL") == "brass" else mat("PullSteel", (0.86, 0.85, 0.83), 1.0, E("PULL_ROUGH", 0.32))
M_SPRING = mat("Spring", (0.02, 0.02, 0.02), 1.0, 0.5)
M_COIL = mat("Coil", (0.32, 0.31, 0.30), 1.0, 0.30)
M_POL = mat("PolishedAlu", (0.80, 0.79, 0.78), 1.0, 0.12)
M_BRASS = mat("Brass", (0.75, 0.60, 0.32), 1.0, 0.25)
M_PICKUP = mat("PickupCover", (0.018, 0.018, 0.019), 0.0, 0.28, spec=0.5)
M_STEEL = mat("StringSteel", (0.62, 0.61, 0.58), 1.0, 0.28)
M_BLACK = mat("Black", (0.014, 0.014, 0.014), 0.0, 0.42, spec=0.4)
M_FB = mat("Fretboard", (0.004, 0.004, 0.005), 0.0, 0.10, spec=0.5)
M_FRET = mat("FretLine", (0.78, 0.76, 0.72), 0.0, 0.5)
M_RUBBER = mat("Rubber", (0.022, 0.022, 0.022), 0.0, 0.55, spec=0.35)
M_FLOOR = mat("Floor", (0.16, 0.06, 0.035), 0.0, 0.8)
COL = {"red": (0.75, 0.05, 0.03), "cream": (0.86, 0.72, 0.52), "blue": (0.05, 0.25, 0.80), "yellow": (0.95, 0.65, 0.04)}
M_MARK = {k: mat("Mark_" + k, v, 0.0, 0.45) for k, v in COL.items()}
# per-model trim (approved refs p3d_maple_1 / p3d_redsparkle_1): mirror-chrome endplates, cream keys on the maple
# hardware options per model (HW_KEYHEAD satin|polished|black, HW_KEYS alu|chrome|ivory|pearl|ebony,
# HW_PICKUP black|chrome|ring, HW_END satin|chrome|nickel); unset = the model's current hardware
HW = lambda k, d: os.environ.get(k, d)
M_NICKEL = mat("Nickel", (0.72, 0.68, 0.60), 1.0, 0.10)
M_BLACKANO = mat("BlackAnodised", (0.030, 0.030, 0.032), 1.0, 0.32)
KEYMATS = {"alu": M_ALU, "chrome": M_CHROME,
           "ivory": mat("IvoryKey", (0.80, 0.70, 0.50), 0.0, 0.25, coat=0.6),
           "pearl": mat("PearlKey", (0.86, 0.84, 0.79), 0.0, 0.18, coat=1.0),
           "ebony": mat("EbonyKey", (0.025, 0.018, 0.014), 0.0, 0.30, coat=0.7)}
# Joshua's hardware picks (2026-11-12, picker page hw_*, second answer): rosewood A, maple B, red sparkle B,
# pearloid B, koa A (A = the satin/alu/black/satin default)
HW_PICKS = {"maple": dict(KEYHEAD="polished", KEYS="ivory", PICKUP="ring", END="chrome"),
            "redsparkle": dict(KEYHEAD="polished", KEYS="chrome", PICKUP="chrome", END="chrome"),
            "pearloid": dict(KEYHEAD="polished", KEYS="pearl", PICKUP="ring", END="nickel")}
_hw = HW_PICKS.get(FINISH, {})
M_END = {"satin": M_ALU, "chrome": M_CHROME, "nickel": M_NICKEL}[HW("HW_END", _hw.get("END", "satin"))]
M_KEY = KEYMATS[HW("HW_KEYS", _hw.get("KEYS", "alu"))]
M_KH = {"satin": M_ALU, "polished": M_CHROME, "black": M_BLACKANO}[HW("HW_KEYHEAD", _hw.get("KEYHEAD", "satin"))]
PICKUP = HW("HW_PICKUP", _hw.get("PICKUP", "black"))
M_INLAY = mat("Inlay", (0.020, 0.008, 0.004), 0.0, 0.35, coat=0.8)


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


def prism_xz(name, pts, y0, y1, material):
    """a side profile (x, z points, counter-clockwise seen from -Y) extruded across Y from y0 to y1"""
    n = len(pts); vs = [(x, y0, z) for x, z in pts] + [(x, y1, z) for x, z in pts]
    fs = [tuple(range(n)), tuple(range(n, 2 * n))[::-1]] + [(i, n + i, n + (i + 1) % n, (i + 1) % n) for i in range(n)]
    me = bpy.data.meshes.new(name); me.from_pydata(vs, [], fs); me.update(); me.validate()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); me.materials.append(material); return o


def text(name, s, loc, size, material):
    cu = bpy.data.curves.new(name, "FONT"); cu.body = s; cu.size = size; cu.align_x = "CENTER"; cu.align_y = "CENTER"
    o = bpy.data.objects.new(name, cu); sc.collection.objects.link(o); o.location = loc; o.data.materials.append(material)
    return o


wear(M_ALU, 0.11, 30.0); wear(M_ALU_B, 0.06, 60.0); wear(M_CHROME, 0.025, 45.0)

# ---- cabinet
# a finish painted by ChatGPT straight onto the cabinet's UV layout from the approved reference (blender/cabinet_uv.py,
# textures/cabinet/cab_<finish>.png): the reference's own wood, placed panel by panel, under a satin coat
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cabinet_uv
CAB_TEX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "textures", "cabinet",
                       os.environ.get("CAB_TEX", "cab_%s.png" % FINISH))


def uv_mat(path):
    m = mat("CabinetUV", (0.1, 0.04, 0.02), 0.0, E("CAB_ROUGH", 0.42), coat=E("CAB_COAT", 0.22)); nt = m.node_tree
    b = nt.nodes["Principled BSDF"]; b.inputs["Coat Roughness"].default_value = E("CAB_COAT_ROUGH", 0.16)
    tc = nt.nodes.new("ShaderNodeTexCoord"); tx = nt.nodes.new("ShaderNodeTexImage"); tx.image = bpy.data.images.load(path)
    tx.interpolation = "Cubic"; nt.links.new(tc.outputs["UV"], tx.inputs["Vector"])
    hs = nt.nodes.new("ShaderNodeHueSaturation"); hs.inputs["Saturation"].default_value = E("CAB_SAT", {"rosewood": 1.35, "koa": 0.85}.get(FINISH, 1.0))
    hs.inputs["Value"].default_value = E("CAB_VAL", 1.0); hs.inputs["Hue"].default_value = E("CAB_HUE", {"koa": 0.514}.get(FINISH, 0.5))
    gm = nt.nodes.new("ShaderNodeGamma"); gm.inputs["Gamma"].default_value = E("CAB_GAMMA", 1.0)   # >1 deepens the dark grain bands
    nt.links.new(tx.outputs["Color"], hs.inputs["Color"]); nt.links.new(hs.outputs["Color"], gm.inputs["Color"]); nt.links.new(gm.outputs["Color"], b.inputs["Base Color"])
    if E("CAB_TOP_VAL", 1.0) != 1.0:   # the up-facing top takes the key light head-on; dim it alone (pearloid top was blown out)
        ge = nt.nodes.new("ShaderNodeNewGeometry"); sz = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(ge.outputs["Normal"], sz.inputs[0])
        up = nt.nodes.new("ShaderNodeMath"); up.operation = "GREATER_THAN"; up.inputs[1].default_value = 0.5; nt.links.new(sz.outputs["Z"], up.inputs[0])
        k_ = nt.nodes.new("ShaderNodeMath"); k_.operation = "MULTIPLY_ADD"; k_.inputs[1].default_value = E("CAB_TOP_VAL", 1.0) - 1.0; k_.inputs[2].default_value = 1.0
        nt.links.new(up.outputs[0], k_.inputs[0])
        mm = nt.nodes.new("ShaderNodeVectorMath"); mm.operation = "SCALE"; nt.links.new(gm.outputs["Color"], mm.inputs[0]); nt.links.new(k_.outputs[0], mm.inputs["Scale"])
        nt.links.new(mm.outputs[0], b.inputs["Base Color"])
    bw = nt.nodes.new("ShaderNodeRGBToBW"); nt.links.new(tx.outputs["Color"], bw.inputs[0])
    bm = nt.nodes.new("ShaderNodeBump"); bm.inputs["Strength"].default_value = 0.05; bm.inputs["Distance"].default_value = 0.0003
    nt.links.new(bw.outputs[0], bm.inputs["Height"]); nt.links.new(bm.outputs["Normal"], b.inputs["Normal"])
    return m


def cabinet_uvs(o):
    import bmesh
    bm_ = bmesh.new(); bm_.from_mesh(o.data); uvl = bm_.loops.layers.uv.new("UVMap")
    for f in bm_.faces:
        n = f.normal
        face = ("FRONT" if n.y < -0.5 else "BACK" if n.y > 0.5 else "TOP" if n.z > 0.5 else "BOTTOM" if n.z < -0.5
                else "END_LEFT" if n.x < 0 else "END_RIGHT")
        for lp in f.loops: lp[uvl].uv = cabinet_uv.uv_for(face, o.matrix_world @ lp.vert.co)
    bm_.to_mesh(o.data); bm_.free()


USE_UV = os.path.exists(CAB_TEX) and os.environ.get("CAB_UV", "1") == "1"
BACK_EXT = E("BACK_EXT", 0.150) if SHAPE == "double" else 0.0   # the double neck's cabinet grows away from the player
YF, YB = -BODY_D / 2, BODY_D / 2 + BACK_EXT; YC, DEPTH = (YF + YB) / 2, YB - YF
if SHAPE == "bullnose":   # half-round front apron, flat top behind it (the front-most point stays where the box's was)
    BN_R = APRON / 2
    cab = box("Cabinet", (0, (YF + BN_R + YB) / 2, -APRON / 2), (BODY_L, YB - YF - BN_R, APRON), M_WOOD, bevel=0.003)
    cyl("CabinetNose", (0, YF + BN_R, -APRON / 2), BN_R, BODY_L, M_WOOD, "X", 96)
    # the veneer is projected in object space: bake rotation and position into both parts, so the grain runs along the
    # cabinet on the round nose too (it ran up and down) and carries on across the seam
    for _o in (cab, bpy.context.object):
        bpy.ops.object.select_all(action="DESELECT"); _o.select_set(True); bpy.context.view_layer.objects.active = _o
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
else:
    cab = box("Cabinet", (0, YC, -APRON / 2), (BODY_L, DEPTH, APRON), uv_mat(CAB_TEX) if USE_UV else M_WOOD, bevel=E("CAB_BEVEL", 0.003), seg=int(E("CAB_SEG", 3)))
HL_MASK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "textures", "cabinet", "hl_%s.png" % FINISH)
USE_HL = os.path.exists(HL_MASK) and E("HL", 0) > 0
if USE_UV or USE_HL:
    cab.data.uv_layers.remove(cab.data.uv_layers[0]) if cab.data.uv_layers else None
    cabinet_uvs(cab)
if USE_HL:   # the concept's bright edge highlights, painted on the UV layout (fixed camera, so they read as gloss)
    hm = cab.data.materials[0].copy(); cab.data.materials[0] = hm; nt = hm.node_tree
    out_ = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL"); bsdf_ = out_.inputs["Surface"].links[0].from_node
    tcm = nt.nodes.new("ShaderNodeTexCoord"); txm = nt.nodes.new("ShaderNodeTexImage"); txm.image = bpy.data.images.load(HL_MASK)
    txm.image.colorspace_settings.name = "Non-Color"; nt.links.new(tcm.outputs["UV"], txm.inputs["Vector"])
    pw = nt.nodes.new("ShaderNodeMath"); pw.operation = "POWER"; pw.inputs[1].default_value = E("HL_GAMMA", 1.6)
    nt.links.new(txm.outputs["Color"], pw.inputs[0])
    em = nt.nodes.new("ShaderNodeEmission"); em.inputs["Color"].default_value = (1.0, 0.93, 0.88, 1)
    ml = nt.nodes.new("ShaderNodeMath"); ml.operation = "MULTIPLY"; ml.inputs[1].default_value = E("HL", 0)
    fl = next((n for n in nt.nodes if n.type == "MAP_RANGE"), None)        # let the sparkle glitter through the highlight
    if fl is not None:
        fm = nt.nodes.new("ShaderNodeMath"); fm.operation = "MULTIPLY_ADD"; fm.inputs[1].default_value = E("HL_FLAKE", 0.35); fm.inputs[2].default_value = 0.4
        nt.links.new(fl.outputs[0], fm.inputs[0]); m2 = nt.nodes.new("ShaderNodeMath"); m2.operation = "MULTIPLY"
        nt.links.new(pw.outputs[0], m2.inputs[0]); nt.links.new(fm.outputs[0], m2.inputs[1]); pw = m2
    tx_ = next((n for n in nt.nodes if n.type == "TEX_IMAGE" and n is not txm), None)
    lum = None
    if tx_ is not None:   # UV-painted cabinets (pearloid): the gloss lands on the bright pearl tiles, so they flash white
        bw = nt.nodes.new("ShaderNodeRGBToBW"); nt.links.new(tx_.outputs["Color"], bw.inputs[0])
        lum = nt.nodes.new("ShaderNodeMath"); lum.operation = "POWER"; lum.inputs[1].default_value = E("HL_TEXP", 3.0)
        nt.links.new(bw.outputs[0], lum.inputs[0])
        m3 = nt.nodes.new("ShaderNodeMath"); m3.operation = "MULTIPLY"
        nt.links.new(pw.outputs[0], m3.inputs[0]); nt.links.new(lum.outputs[0], m3.inputs[1]); pw = m3
    nt.links.new(pw.outputs[0], ml.inputs[0])
    if lum is not None and E("TILE_GLINT", 0) > 0:   # plus a little everywhere: individual tiles catch the light across the face
        tg = nt.nodes.new("ShaderNodeMath"); tg.operation = "MULTIPLY_ADD"; tg.inputs[1].default_value = E("TILE_GLINT", 0)
        nt.links.new(lum.outputs[0], tg.inputs[0]); nt.links.new(ml.outputs[0], tg.inputs[2]); ml = tg
    nt.links.new(ml.outputs[0], em.inputs["Strength"])
    if E("HL_CORE", 0) > 0:   # tinted glow fading to a white-hot core, like gloss on a coloured finish
        cp = nt.nodes.new("ShaderNodeMath"); cp.operation = "POWER"; cp.inputs[1].default_value = E("HL_CORE", 0)
        nt.links.new(txm.outputs["Color"], cp.inputs[0])
        mx3 = nt.nodes.new("ShaderNodeMix"); mx3.data_type = "RGBA"; nt.links.new(cp.outputs[0], mx3.inputs[0])
        mx3.inputs[6].default_value = (E("HL_TR", 1.0), E("HL_TG", 0.12), E("HL_TB", 0.10), 1); mx3.inputs[7].default_value = (1.0, 0.93, 0.88, 1)
        nt.links.new(mx3.outputs[2], em.inputs["Color"])
    ad = nt.nodes.new("ShaderNodeAddShader"); nt.links.new(bsdf_.outputs[0], ad.inputs[0]); nt.links.new(em.outputs[0], ad.inputs[1])
    nt.links.new(ad.outputs[0], out_.inputs["Surface"])
if SHAPE == "bullnose": os.environ["TOP_TRIM"] = os.environ["BOTTOM_BAR"] = "0"   # nothing straight to run along the round front
if E("TOP_TRIM", 1) > 0: box("TopTrim", (0, -BODY_D / 2 + 0.0012, 0.0004), (BODY_L - 0.004, 0.0020, 0.0014), M_ALU, bevel=0.0005)       # front top edge strip
if E("BOTTOM_BAR", 1) > 0:   # metal bar along the bottom edge of the front apron, endplate to endplate (Joshua 2026-11-11, red line)
    _bh = E("BAR_H", 0.0055)   # thin, like the concepts (Joshua)
    _bv = E("BAR_VAL", 0.16)   # darker satin metal where the concept's bar sits in shadow (koa, Joshua)
    _bm = mat("BarMetal", (_bv, _bv, _bv * 0.98), 1.0, E("BAR_ROUGH", 0.32)) if _bv > 0 else M_ALU
    box("BottomBar", (0, -BODY_D / 2 + 0.002, -APRON + _bh / 2 - 0.0015), (BODY_L - 0.004, 0.0070, _bh), _bm, bevel=0.0018, seg=5)
if E("EDGE_TRIM", 0) > 0:   # darker satin-silver strip wrapping the top front edge (pearloid concept, Joshua)
    _ev = E("EDGE_VAL", 0.50)
    box("EdgeTrim", (0, -BODY_D / 2 + 0.0020, -0.0020), (BODY_L - 0.004, 0.0062, 0.0062), mat("EdgeMetal", (_ev, _ev, _ev * 0.98), 1.0, E("EDGE_ROUGH", 0.30)), bevel=0.0018, seg=4)
EP_T = 0.025
for sx in (-1, 1):                                                           # endplates: polished cast aluminium, proud of the cabinet
    xe = sx * (BODY_L / 2 + EP_T / 2 - 0.004)
    if SHAPE == "bullnose":   # the end cap follows the round front and wraps it (s3_1; Joshua: square plates looked pasted on)
        import bmesh
        R2 = BN_R + 0.011; cy_, cz_ = YF + BN_R, -APRON / 2 - 0.004; zt, zb = cz_ + R2, cz_ - R2
        prof = [(YB + 0.007, zt), (YB + 0.007, zb)] + [(cy_ - R2 * math.sin(t), cz_ - R2 * math.cos(t))
                                                         for t in [i * math.pi / 32 for i in range(33)]]
        x0_, x1_ = xe - EP_T / 2, xe + EP_T / 2; n_ = len(prof)
        me = bpy.data.meshes.new("Endplate"); vs = [(x0_, y, z) for y, z in prof] + [(x1_, y, z) for y, z in prof]
        fs = [tuple(range(n_)), tuple(range(n_, 2 * n_))] + [(i, (i + 1) % n_, n_ + (i + 1) % n_, n_ + i) for i in range(n_)]
        me.from_pydata(vs, [], fs); bm_ = bmesh.new(); bm_.from_mesh(me); bmesh.ops.recalc_face_normals(bm_, faces=bm_.faces)
        bm_.to_mesh(me); bm_.free(); ep = bpy.data.objects.new("Endplate", me); sc.collection.objects.link(ep); me.materials.append(M_END)
        bv_ = ep.modifiers.new("bevel", "BEVEL"); bv_.width = 0.004; bv_.segments = 5; bv_.limit_method = "ANGLE"; bv_.angle_limit = math.radians(30)
        for pl in me.polygons: pl.use_smooth = True
        box("EndLip", (sx * (BODY_L / 2 - 0.004), (cy_ + YB) / 2 + 0.002, 0.0025), (0.026, YB - cy_ + 0.010, 0.005), M_END, bevel=0.002, seg=3)
        for a_ in (math.radians(40), math.radians(-40)):   # two screws on the round front of each cap
            n3 = Vector((0, -math.cos(a_), math.sin(a_))); p3 = Vector((xe, cy_, cz_)) + n3 * R2
            rod("Screw", p3 - n3 * 0.0006, p3 + n3 * 0.0014, 0.0046, M_CHROME, 20)
        for ye in (cy_, YB - 0.016):
            cyl("LipScrew", (sx * (BODY_L / 2 - 0.006), ye, 0.0052), 0.0030, 0.0012, M_CHROME, "Z", 16)
        continue
    box("Endplate", (xe, YC, -APRON / 2 - 0.004), (EP_T, DEPTH + 0.014, APRON + 0.022), M_END, bevel=0.006, seg=5)
    box("EndLip", (sx * (BODY_L / 2 - 0.004), YC, 0.0025), (0.026, DEPTH + 0.014, 0.005), M_END, bevel=0.002, seg=3)
    for z in (-0.016, -APRON + 0.006):                                       # screws on the front face
        cyl("Screw", (xe, -BODY_D / 2 - 0.0072, z), 0.0048, 0.0018, M_CHROME, "Y", 20, bevel=0.0006)
        box("ScrewSlot", (xe, -BODY_D / 2 - 0.0082, z), (0.0070, 0.0006, 0.0010), M_BLACK, rot=(0, math.radians(35), 0))
    for ye in (YF + 0.016, YB - 0.016):
        cyl("LipScrew", (sx * (BODY_L / 2 - 0.006), ye, 0.0052), 0.0030, 0.0012, M_CHROME, "Z", 16)

if FINISH == "maple" and SHAPE == "box":                                                       # dark inlaid stripe low on the front
    box("Inlay", (0, -BODY_D / 2 - 0.0002, -APRON * E("INLAY_Z", 0.62)), (BODY_L - 0.002, 0.0012, E("INLAY_H", 0.0055)), M_INLAY)
if FINISH == "redsparkle":                                                    # chrome trim strip along the front
    box("FrontTrim", (0, -BODY_D / 2 - 0.0016, -APRON * E("TRIM_Z", 0.42)), (BODY_L - 0.002, 0.0070, E("TRIM_H", 0.0140)), mat("TrimBright", (0.90, 0.89, 0.87), 1.0, E("TRIM_ROUGH", 0.20)), bevel=0.0034, seg=10)   # wider, rounded so it catches the light (Joshua)

# ---- neck plate (runs from the keyhead to the changer), fretboard
NECK_START = set(bpy.data.objects)   # everything up to the changer is copied for the double neck's second neck
if SHAPE == "bullnose" and FINISH == "maple":   # the 60s blonde: cream board, dark frets, maroon and pearl diamonds
    M_FB = mat("FretboardCream", (0.78, 0.70, 0.54), 0.0, 0.30, spec=0.4)
    M_FRET = mat("FretLineDark", (0.035, 0.030, 0.028), 0.0, 0.5)
    COL = {"red": (0.32, 0.035, 0.03), "cream": (0.80, 0.82, 0.84), "blue": (0.32, 0.035, 0.03), "yellow": (0.80, 0.82, 0.84)}
    M_MARK = {k: mat("Mark_" + k, v, 0.0, 0.35) for k, v in COL.items()}
if SHAPE == "double":   # pearl diamonds on both necks, as on the concept
    M_MARK = {k: mat("Mark_pearl", (0.82, 0.83, 0.84), 0.0, 0.30, coat=0.6) for k in COL}
FB_W = 0.122
X_GUIDE = X_NUT - 0.098                                                      # the roller bar where the keyhead ends
x0n, x1n = X_NUT - 0.009, X_BR + 0.004
for sy in (-1, 1):                                                           # between keyhead and nut: an open frame over the wood
    box("NeckRail", ((X_GUIDE + X_NUT) / 2, sy * (NECK_W / 2 - 0.0045), NECK_H / 2), (X_NUT - X_GUIDE + 0.006, 0.009, NECK_H), M_ALU_B, bevel=0.002)
box("Neck", ((x0n + x1n) / 2, 0, NECK_H / 2), (x1n - x0n, NECK_W, NECK_H), M_ALU_B, bevel=0.0025, seg=3)
for xs_ in (X_NUT + 0.02, fret_x(6.5), fret_x(14.5)):          # (no pair by the pickup: Joshua)
    for sy in (-1, 1):
        cyl("NeckScrew", (xs_, sy * (NECK_W / 2 - 0.0045), NECK_H + 0.0004), 0.0028, 0.0012, M_CHROME, "Z", 16)
fb0, fb1 = X_NUT + 0.004, X_NUT + 0.495
box("Fretboard", ((fb0 + fb1) / 2, 0, NECK_H + 0.0008), (fb1 - fb0, FB_W, 0.0016), M_FB, bevel=0.0006)
ZF_ = NECK_H + 0.00165
M_GUIDE = mat("GuideLine", (0.42, 0.41, 0.39) if M_FB.name == "Fretboard" else (0.50, 0.44, 0.33), 0.0, 0.5)
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
M_NUM = mat("FretNumber", (0.92, 0.90, 0.84) if M_FB.name == "Fretboard" else (0.05, 0.04, 0.035), 0.0, 0.5)
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
if os.environ.get("KEYBED", "1" if FINISH == "pearloid" else "0") == "1":   # dark plate under the keyhead and up to the nut
    # (the concept shows black there; bright pearl read as glare: Joshua)
    box("KeyBed", ((KH_X0 + X_NUT) / 2 - 0.004, 0, 0.0006), (X_NUT - KH_X0 + 0.004, NECK_W - 0.002, 0.0012),
        mat("KeyBed", (0.020, 0.020, 0.022), 0.0, 0.35, spec=0.4), bevel=0.0004)
kh = prism("Keyhead", outer, 0.0, KH_Z, M_KH)
inner = [(xl + WR + 0.006, -yL + WR + 0.004), (xr - 0.012, -yR + WR), (xr - 0.012, yR - WR), (xl + WR + 0.006, yL - WR - 0.004), (xl + WR, yL - WR - 0.012), (xl + WR, -yL + WR + 0.012)]
cut = prism("KeyheadWindow", inner, -0.01, KH_Z + 0.01, M_ALU); cut.hide_render = True; cut.display_type = "WIRE"
bo = kh.modifiers.new("window", "BOOLEAN"); bo.operation = "DIFFERENCE"; bo.object = cut; bo.solver = "EXACT"
bv = kh.modifiers.new("bevel", "BEVEL"); bv.width = 0.0045; bv.segments = 6; bv.limit_method = "ANGLE"; bv.angle_limit = math.radians(40)
for pl in kh.data.polygons: pl.use_smooth = True
FINGER = [(-0.011, -0.017), (-0.013, -0.009), (-0.010, -0.002), (-0.004, 0.0012), (0.003, 0.0010), (0.010, -0.002),
          (0.020, -0.006), (0.034, -0.0095), (0.043, -0.0110), (0.045, -0.0150), (0.043, -0.0190), (0.030, -0.0185),
          (0.016, -0.0150), (0.006, -0.0160), (-0.002, -0.0195)]          # side profile (x from the bridge, z from the string)
for i, y in enumerate(SY):
    far = i < 5; k = i if far else 9 - i; s = 1 if far else -1                # outer strings' tuners nearest the nut
    t = 0.14 + k * 0.175
    rp = RAIL0(s).lerp(RAIL1(s), t)                                           # this tuner's place on its rail
    # a real keyhead tuner: gear housing on the rail, the string capstan standing just inside the rail, the key's
    # shaft running out sideways through the rail to a flat oval key (the far row's keys behind, the near row's in front)
    box("GearBox", (rp.x, rp.y + s * 0.001, KH_Z + 0.0045), (0.015, 0.0150, 0.0090), M_ALU, bevel=0.0022, seg=3)
    cyl("GearScrew", (rp.x + 0.004, rp.y + s * 0.001, KH_Z + 0.0091), 0.0016, 0.0008, M_CHROME, "Z", 12)
    cy = rp.y - s * 0.0135
    cyl("Capstan", (cy * 0 + rp.x, cy, KH_Z * 0.55 + 0.004), 0.0030, KH_Z * 1.1, M_CHROME, "Z", 20, bevel=0.0005)
    cyl("CapstanTop", (rp.x, cy, KH_Z * 0.55 + 0.004 + KH_Z * 0.55), 0.0040, 0.0016, M_CHROME, "Z", 20, bevel=0.0005)
    zw = KH_Z * 0.55 + 0.006 - 0.0012 * (k % 2)
    cyl("Wind", (rp.x, cy, zw), 0.0034, 0.0035, M_STEEL, "Z", 20)              # the string's winds on the capstan
    rod("String", (rp.x, cy - s * 0.0030, zw), (X_GUIDE, y, Z_STR), GA[i] * SR, M_STEEL, 12)
    yo = rp.y + s * 0.0085
    rod("KeyShaft", (rp.x, yo, KH_Z + 0.0045), (rp.x, yo + s * 0.016, KH_Z + 0.0045), 0.0021, M_CHROME, 16)
    cyl("KeyBushing", (rp.x, yo + s * 0.0010, KH_Z + 0.0045), 0.0042, 0.0025, M_CHROME, "Y", 20, bevel=0.0006)
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=1, depth=1, location=(rp.x, yo + s * 0.025, KH_Z + 0.0045))
    kb = bpy.context.object; kb.name = "KeyButton"; kb.scale = (0.0076, 0.0104, 0.0056); kb.rotation_euler = (0, math.pi / 2, 0)
    finish(kb, M_KEY, 0.0014, 3)
    rod("String", (X_GUIDE, y, Z_STR), (X_BR + 0.002, y, Z_STR), GA[i] * SR, M_STEEL, 12)
    # changer finger: a long curved lever the string wraps over, then its square nut and coil spring in the tray
    fo = prism_xz("Finger", [(X_BR + 0.003 + fx, Z_STR - 0.0004 + fz) for fx, fz in FINGER], y - 0.0033, y + 0.0033, M_POL)
    fb = fo.modifiers.new("bevel", "BEVEL"); fb.width = 0.0011; fb.segments = 3; fb.limit_method = "ANGLE"
    for pl in fo.data.polygons: pl.use_smooth = True
    cyl("FingerEye", (X_BR - 0.0060, y, Z_STR - 0.0125), 0.0052, 0.0072, M_POL, "Y", 24, bevel=0.0008)
    cyl("FingerEyeHole", (X_BR - 0.0060, y, Z_STR - 0.0125), 0.0024, 0.0076, M_BLACK, "Y", 12)
    box("TuneNut", (X_BR + 0.0525, y, Z_STR - 0.0160), (0.0075, 0.0080, 0.0085), M_POL, bevel=0.0012)
    cyl("Spring", (X_BR + 0.0640, y, Z_STR - 0.0165), 0.0028, 0.0170, M_SPRING, "X", 14)
    for q in range(7):
        cyl("Coil", (X_BR + 0.0575 + q * 0.0022, y, Z_STR - 0.0165), 0.0036, 0.0010, M_COIL, "X", 14)
    # pickup pole piece: a steel slug standing a hair proud of the cover, with a dark ring around it
    cyl("PoleRing", (X_BR - 0.038, y, NECK_H + 0.0099), 0.0034, 0.0004, M_BLACK, "Z", 20)
    cyl("Pole", (X_BR - 0.038, y, NECK_H + 0.0102), 0.0024, 0.0009, M_STEEL, "Z", 20, bevel=0.0003)
box("Pickup", (X_BR - 0.038, 0, NECK_H + 0.0048), (0.026, FB_W - 0.004, 0.0100), M_CHROME if PICKUP == "chrome" else M_PICKUP, bevel=0.0035, seg=6)
if PICKUP == "ring":                                                          # chrome surround ring under the black cover
    box("PickupRing", (X_BR - 0.038, 0, NECK_H + 0.0012), (0.034, FB_W + 0.004, 0.0024), M_CHROME, bevel=0.0010, seg=3)
box("ChangerTray", (X_BR + 0.038, 0, 0.004), (0.074, NECK_W - 0.006, 0.008), M_BLACK)
box("TrayWall", (X_BR + 0.0735, 0, Z_STR / 2 - 0.004), (0.005, NECK_W - 0.002, Z_STR - 0.006), M_ALU, bevel=0.0015)
for s in (1, -1):
    box("TraySide", (X_BR + 0.040, s * (NECK_W / 2 - 0.0025), Z_STR / 2 - 0.004), (0.072, 0.005, Z_STR - 0.006), M_ALU, bevel=0.0015)
    cyl("ChangerCheek", (X_BR + 0.006, s * (NECK_W / 2 - 0.0005), Z_STR - 0.0150), 0.0165, 0.0100, M_ALU, "Y", 36, bevel=0.0025)
    box("CheekFoot", (X_BR + 0.006, s * (NECK_W / 2 - 0.0005), (Z_STR - 0.015) / 2), (0.033, 0.0100, Z_STR - 0.015), M_ALU, bevel=0.002)
    cyl("CheekScrew", (X_BR + 0.006, s * (NECK_W / 2 + 0.0048), Z_STR - 0.0150), 0.0040, 0.0016, M_CHROME, "Y", 16)

if SHAPE == "double":   # the second (far) neck: the same neck, keyhead, changer and strings, one neck-width further away
    DY = E("NECK2_DY", 0.150); DZ = E("NECK2_DZ", 0.0)
    made = [o for o in bpy.data.objects if o not in NECK_START]; twins = {}
    for o in made:
        c = o.copy(); sc.collection.objects.link(c); c.location = o.location + Vector((0, DY, DZ)); twins[o] = c
    for o, c in twins.items():
        for md in c.modifiers:
            if md.type == "BOOLEAN" and md.object in twins: md.object = twins[md.object]
    if DZ > 0:   # a wood riser under the raised neck
        box("NeckRiser", ((KH_X0 + X_BR + 0.08) / 2, DY, DZ / 2), (X_BR + 0.08 - KH_X0, NECK_W + 0.012, DZ), M_WOOD, bevel=0.002)

# ---- legs: chrome upper tube, thinner lower tube with a clutch collar, rubber foot; each stands straight down from
# its corner socket under the endplate (Joshua 2026-11-10: angled-in legs looked bent)
FEET = {}
for sx in (-1, 1):
    for sy in (-1, 1):
        top = Vector((sx * (BODY_L / 2 + EP_T / 2 - 0.004), (YF + 0.004) if sy < 0 else (YB - 0.004), -APRON - 0.010))   # under the endplate's corner
        if SHAPE == "bullnose" and sy < 0:   # the front legs meet the cap's curved underside
            top.z = cz_ - math.sqrt(max(R2 ** 2 - (cy_ - top.y) ** 2, 0.0)) + 0.004
        foot = Vector((top.x, top.y, Z_FLOOR + 0.012))
        ax = (foot - top).normalized(); mid = top.lerp(foot, 0.70)
        rod("LegSocket", top + ax * -0.006, top + ax * 0.020, 0.0122, M_ALU, 32)
        if SHAPE == "bullnose":   # one plain chrome tube on a flared bell foot (s3_1)
            rod("LegUpper", top, foot, 0.0085, M_CHROME, 40)
            bpy.ops.mesh.primitive_cone_add(vertices=48, radius1=0.0150, radius2=0.0085, depth=0.024, location=(foot.x, foot.y, Z_FLOOR + 0.012))
            finish(bpy.context.object, M_CHROME, 0.0015, 3).name = "LegBell"
            cyl("LegPad", (foot.x, foot.y, Z_FLOOR + 0.0012), 0.0150, 0.0024, M_RUBBER, "Z", 48)
        else:
            rod("LegUpper", top, mid, 0.0090, M_CHROME, 40); rod("LegLower", mid, foot, 0.0068, M_CHROME, 40)
            rod("LegCollar", mid - ax * 0.012, mid + ax * 0.008, 0.0114, M_ROD, 32)
            rod("LegFoot", foot - ax * 0.004, foot + ax * 0.014, 0.0100, M_RUBBER, 24)
        FEET[(sx, sy)] = foot

# ---- pedal rod clamped to the front legs, three ribbed pedals, pull rods; knee levers at the front apron
fl, fr = FEET[(-1, -1)], FEET[(1, -1)]
BAR_Z = Z_FLOOR + 0.036; BAR_Y = fl.y - 0.002
rod("PedalBar", (fl.x, BAR_Y, BAR_Z), (fr.x, BAR_Y, BAR_Z), 0.0062, M_ROD, 32)
for f in (fl, fr):
    box("BarClamp", (f.x, BAR_Y + 0.004, BAR_Z), (0.032, 0.034, 0.030), M_POL, bevel=0.008, seg=6)
    cyl("ClampBolt", (f.x - 0.008, BAR_Y - 0.0145, BAR_Z), 0.0036, 0.003, M_CHROME, "Y", 6)
    cyl("ClampBolt", (f.x + 0.008, BAR_Y - 0.0145, BAR_Z), 0.0036, 0.003, M_CHROME, "Y", 6)
PEDAL_X = [-0.0935, -0.0250, 0.0410]
# the double neck's C6 pedals: static, either side of the three that move (A B C stay where the plugin's sprites are)
PSTEP = PEDAL_X[1] - PEDAL_X[0]
EXTRA_X = ([PEDAL_X[0] - PSTEP * i for i in (2, 1)] + [PEDAL_X[2] + PSTEP * i for i in (1, 2, 3)]) if SHAPE == "double" else []
ALL_X = sorted(PEDAL_X + EXTRA_X)
for xs_ in (ALL_X[0] - 0.030, ALL_X[-1] + 0.030):                             # stop collars either side of the pedal group
    cyl("BarCollar", (xs_, BAR_Y, BAR_Z), 0.0095, 0.010, M_ROD, "X", 24, bevel=0.001)
PT = math.radians(-16)
cT, sT = math.cos(PT), math.sin(PT)
for k, px in list(zip("ABC", PEDAL_X)) + [("X%d" % i, x) for i, x in enumerate(EXTRA_X)]:
    box("PedalHinge_" + k, (px, BAR_Y, BAR_Z), (0.030, 0.020, 0.020), M_ALU, bevel=0.004, seg=3)
    pc = Vector((px, BAR_Y - 0.043, BAR_Z + 0.0004))                         # pedal plate centre; local Y runs down the tread
    box("Pedal_" + k, pc, (0.037, 0.074, 0.009), M_ALU, bevel=0.003, seg=4, rot=(PT, 0, 0))
    for r_ in range(19):                                                      # ribs across the tread
        ly = 0.0325 - r_ * 0.0036
        box("PedalRib_" + k, pc + Vector((0, ly * cT - 0.0055 * sT, ly * sT + 0.0055 * cT)), (0.033, 0.0017, 0.0016), M_ALU, bevel=0.0005, rot=(PT, 0, 0))
    box("RodClevis_" + k, (px, BAR_Y - 0.004, BAR_Z + 0.030), (0.009, 0.009, 0.016), M_ALU, bevel=0.0025)
    for sgn in (-1, 1):
        box("Yoke_" + k, (px + sgn * 0.011, BAR_Y - 0.006, BAR_Z + 0.018), (0.005, 0.012, 0.032), M_ALU, bevel=0.002)
    cyl("ClevisPin_" + k, (px, BAR_Y - 0.004, BAR_Z + 0.032), 0.0030, 0.016, M_CHROME, "X", 12)
    rod("PullRod_" + k, (px, BAR_Y - 0.004, BAR_Z + 0.036), (px, -BODY_D / 2 + 0.012, -APRON), E("PULL_R", 0.0024), M_PULL, 16)
# knee levers: the box steel's sit inboard with ribbed rubber pads; the double neck's hang out by the legs as bent chrome
# levers with flat chrome paddles (s6_3; Joshua 2026-11-12: "should match reference, moved to outer side")
KNEES = (("L", -0.392, 1), ("R", 0.392, -1)) if SHAPE == "double" else (("L", -0.1905, 1), ("R", 0.1366, -1))
for k, kx, _sg in KNEES:
    box("KneeBracket_" + k, (kx, -BODY_D / 2 + 0.008, -APRON - 0.005), (0.046, 0.020, 0.010), M_ALU, bevel=0.002)
    for bx in (-0.015, 0.015):
        cyl("KneeBolt_" + k, (kx + bx, -BODY_D / 2 - 0.0025, -APRON - 0.005), 0.0028, 0.002, M_CHROME, "Y", 6)
    if SHAPE == "double":
        kout = -_sg   # toward the nearer leg
        rod("KneeArm_" + k, (kx, -BODY_D / 2 + 0.002, -APRON - 0.010), (kx + kout * 0.012, -BODY_D / 2 - 0.004, -APRON - 0.050), 0.0040, M_CHROME, 20)
        cyl("KneePivot_" + k, (kx, -BODY_D / 2 - 0.002, -APRON - 0.014), 0.0042, 0.004, M_CHROME, "Y", 16)
        rod("KneeArm_" + k, (kx + kout * 0.012, -BODY_D / 2 - 0.004, -APRON - 0.050), (kx + kout * 0.016, -BODY_D / 2 - 0.006, -APRON - 0.075), 0.0040, M_CHROME, 20)
        box("KneePad_" + k, (kx + kout * 0.016, -BODY_D / 2 - 0.007, -APRON - 0.105), (0.024, 0.005, 0.064), M_POL, bevel=0.0022, seg=4)
        continue
    box("KneeArm_" + k, (kx, -BODY_D / 2 + 0.002, -APRON - 0.026), (0.010, 0.006, 0.040), M_ALU, bevel=0.0015)
    cyl("KneePivot_" + k, (kx, -BODY_D / 2 - 0.002, -APRON - 0.040), 0.0042, 0.004, M_CHROME, "Y", 16)
    box("KneePad_" + k, (kx, -BODY_D / 2 - 0.002, -APRON - 0.086), (0.021, 0.013, 0.082), M_RUBBER, bevel=0.0055, seg=4)
    for q in range(9):
        box("KneeRib_" + k, (kx, -BODY_D / 2 - 0.0088, -APRON - 0.056 - q * 0.0075), (0.017, 0.0016, 0.0026), M_RUBBER, bevel=0.0006)

# ---- tone bar: polished steel bullet bar lying across the strings (a separate live layer later)
BAR_FRET = E("STEEL_BARFRET", 8.7)
if BAR_FRET >= 0:
    bx_ = fret_x(BAR_FRET); br = 0.0162
    cyl("ToneBar", (bx_, 0.004, Z_STR + br), br, 0.112, M_CHROME, "Y", 48, bevel=0.003)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=br, location=(bx_, 0.060, Z_STR + br))
    finish(bpy.context.object, M_CHROME).name = "ToneBarNose"

# ---- floor (shadow catcher) and what the metal reflects
bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0.5, Z_FLOOR)); fl_ = bpy.context.object; fl_.name = "Floor"
fl_.data.materials.append(M_FLOOR); fl_.is_shadow_catcher = True
fl_.visible_glossy = False          # the apron reflects the room's light patches, not this stand-in floor

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
# a wider lens from the same spot = the same picture scaled about a point, so canvas' = ZOOM * canvas + (tx, ty) exactly;
# the anchor (the front of the floor, centre) stays put (double neck: its far neck would otherwise run under the top bar)
ZOOM = E("STEEL_ZOOM", 0.88 if SHAPE == "double" else 1.0); AFFINE = [1.0, 0.0, 0.0]
if ZOOM != 1.0:
    _anc = Vector((0, -BODY_D / 2 - 0.05, Z_FLOOR)); _p0 = proj(_anc)
    cam.data.lens *= ZOOM; cam.data.shift_x *= ZOOM; cam.data.shift_y *= ZOOM; bpy.context.view_layer.update()
    for _ in range(3):   # shift_x > 0 moves the picture left, shift_y > 0 moves it down (units: the canvas width)
        _p1 = proj(_anc); cam.data.shift_x -= (_p0[0] - _p1[0]) / CW; cam.data.shift_y += (_p0[1] - _p1[1]) / CW
        bpy.context.view_layer.update()
    _pa = proj(_anc); AFFINE = [ZOOM, _pa[0] - ZOOM * _p0[0], _pa[1] - ZOOM * _p0[1]]
    print("ZOOM affine", AFFINE, "anchor", _p0, "->", _pa)


# ---- lights: the room's warm lamp upper left, a broad soft top light, a low front fill
def area(name, loc, size, power, col, target=(0, 0, -0.1)):
    ld = bpy.data.lights.new(name, "AREA"); ld.energy = power; ld.shape = "RECTANGLE"; ld.size, ld.size_y = size; ld.color = col
    o = bpy.data.objects.new(name, ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
KP = E("STEEL_LIGHT", 1.0)
area("Key", (-1.2, -0.7, 1.5), (0.35, 0.30), 60 * KP, (1.0, 0.84, 0.66))
_top = area("Top", (0.2, 0.3, 1.9), (1.6, 0.5), 9 * KP, (1.0, 0.90, 0.78))
_fill = area("Fill", (0.6, -1.6, 0.5), (1.6, 0.8), 5 * KP, (1.0, 0.86, 0.72))
if E("BIG_LIGHT_GLOSSY", 1.0) <= 0: _top.visible_glossy = _fill.visible_glossy = False   # broad lights read as haze on gloss paint
# low soft light from the player's side under the cabinet: the knee levers and pedal hardware were in the apron's
# shadow, darker than the reference (Joshua)
area("UnderFill", (0.0, -1.3, -0.13), (1.4, 0.25), E("UNDER_FILL", 0.0) * KP, (1.0, 0.86, 0.70), target=(0, -0.10, -0.17))

# soft bright shapes in the room that only reflections see (the reference's light patches): in front and low for the
# glossy front apron, behind and high for the fretboard and the neck plate
def glint(name, loc, w, h, strength, col=(1.0, 0.86, 0.68)):
    bpy.ops.mesh.primitive_plane_add(size=1, location=loc); o = bpy.context.object; o.name = name
    o.scale = (w, h, 1); o.rotation_euler = (Vector((0, 0, -0.02)) - Vector(loc)).to_track_quat("Z", "Y").to_euler()
    m = bpy.data.materials.new(name); m.use_nodes = True; nt_ = m.node_tree; nt_.nodes.clear()
    e = nt_.nodes.new("ShaderNodeEmission"); e.inputs["Color"].default_value = (*col, 1); e.inputs["Strength"].default_value = strength
    # soft edges: fade the emission toward the plane's border
    tc_ = nt_.nodes.new("ShaderNodeTexCoord"); gr = nt_.nodes.new("ShaderNodeTexGradient"); gr.gradient_type = "SPHERICAL"
    mp_ = nt_.nodes.new("ShaderNodeMapping"); mp_.inputs["Location"].default_value = (0.5, 0.5, 0); mp_.inputs["Scale"].default_value = (2.0, 2.0, 1)
    nt_.links.new(tc_.outputs["UV"], mp_.inputs[0])
    sub = nt_.nodes.new("ShaderNodeVectorMath"); sub.operation = "SUBTRACT"; sub.inputs[1].default_value = (1.0, 1.0, 0)
    nt_.links.new(mp_.outputs[0], sub.inputs[0]); nt_.links.new(sub.outputs[0], gr.inputs[0])
    ml = nt_.nodes.new("ShaderNodeMath"); ml.operation = "MULTIPLY"; ml.inputs[1].default_value = strength
    nt_.links.new(gr.outputs["Fac"], ml.inputs[0]); nt_.links.new(ml.outputs[0], e.inputs["Strength"])
    nt_.links.new(e.outputs[0], nt_.nodes.new("ShaderNodeOutputMaterial").inputs[0]); o.data.materials.append(m)
    o.visible_camera = False; o.visible_diffuse = False; o.visible_shadow = False; o.visible_transmission = False
    o.visible_volume_scatter = False; o.visible_glossy = True
    return o
GL = E("STEEL_GLINT", 1.0)
d_front = Vector((0, -math.cos(TH), -math.sin(TH)))        # where the front apron's mirror direction points
d_top = Vector((0, math.cos(TH), math.sin(TH)))            # where the flat tops' mirror direction points
for i, gx in enumerate((-0.42, 0.02, 0.40)):
    glint("GlintTop%d" % i, d_top * 3.0 + Vector((gx * 1.0, 0, 0)), 0.55, 1.4, E("TOP_GLINT", {"redsparkle": 3.6}.get(FINISH, 1.6)) * GL)
# small hot spots that only show as sharp highlights on rounded glossy edges (Joshua's red sparkle concept)
if E("HOTSPOTS", 0) > 0:
    d_mid = (d_top + Vector((0, -1.2, 0.4))).normalized()
    spots = [d_mid * 3.0 + Vector((gx, 0, 0)) for gx in (-1.1, -0.35, 0.35, 1.1)]
    spots += [Vector((sx * 2.6, -1.6, 1.4)) for sx in (-1, 1)]
    for i, p_ in enumerate(spots):
        glint("Hot%d" % i, p_, E("HOT_SIZE", 0.10), E("HOT_SIZE", 0.10), E("HOTSPOTS", 0) * GL, col=(1.0, 0.95, 0.88))
# the front apron: one long soft sheen running past both ends (three panels read as blurry blotches: Joshua)
glint("GlintFront", d_front * 3.0, E("FRONT_W", 9.0), E("FRONT_H", 1.3), E("FRONT_GLINT", {"koa": 1.0}.get(FINISH, 2.2)) * GL)   # koa: less haze on the front (Joshua)

bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=9, location=(0, 0, 0))
dome = bpy.context.object; dome.name = "RoomDome"
if E("DOME_GLOSSY", 1.0) <= 0: dome.visible_glossy = False   # glossy paint shows only the highlight strips, not a hazy room
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
ramp.color_ramp.elements[0].color = (0.11, 0.045, 0.025, 1)                  # rug / dark room toward the horizon
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
        "camera": {"pitch_deg": math.degrees(TH), "dist": DIST, "px_per_m": PXM}, "shape": SHAPE, "affine": AFFINE}
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
for k, kx, sgn in KNEES:
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
