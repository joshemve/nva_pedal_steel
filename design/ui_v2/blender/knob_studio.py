"""Isolated Cycles studio for physical knob filmstrips.

blender --factory-startup -b -P knob_studio.py -- OUT_DIR [CONFIG.json]
The camera and lights are fixed. Only each knob's Spin node is animated.
No reference image, screen-space correction, or room emission enters shading.
"""
import bpy, json, math, os, sys
from pathlib import Path
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import knob_studio_geometry as KL
import steel_lib as S

args = sys.argv[sys.argv.index("--") + 1:]
out = Path(args[0]).resolve(); out.mkdir(parents=True, exist_ok=True)
cfg = json.loads(Path(args[1]).read_text()) if len(args) > 1 else {}
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
if hasattr(bpy.context.preferences.system, "use_gpu_subdivision"):
    bpy.context.preferences.system.use_gpu_subdivision = False
sc.render.engine = "CYCLES"; sc.cycles.device = cfg.get("device", "GPU")
sc.cycles.samples = cfg.get("samples", 96); sc.cycles.use_denoising = True
sc.render.use_persistent_data = True
prefs = bpy.context.preferences.addons["cycles"].preferences
if sc.cycles.device == "GPU":
    prefs.compute_device_type = "OPTIX"; prefs.get_devices()
    for device in prefs.devices: device.use = device.type == "OPTIX"
sc.world = bpy.data.worlds.new("Black studio")
sc.world.use_nodes = True
sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.7,0.65,0.6,1)
sc.world.node_tree.nodes["Background"].inputs[1].default_value = cfg.get("ambient", 0)
sc.view_settings.view_transform = "AgX"
sc.view_settings.look = "AgX - Medium High Contrast"
sc.view_settings.exposure = cfg.get("exposure", -0.7)
sc.render.film_transparent = True
sc.render.image_settings.file_format = "PNG"
sc.render.image_settings.color_mode = "RGBA"
sc.render.image_settings.color_depth = "8"
scale = cfg.get("scale", 3)
sc.render.resolution_x = 480 * scale; sc.render.resolution_y = 144 * scale
sc.render.resolution_percentage = 100

cam_data = bpy.data.cameras.new("OrthographicCamera")
cam = bpy.data.objects.new("OrthographicCamera", cam_data)
sc.collection.objects.link(cam); sc.camera = cam
cam.location = (0, 0, 0.65); cam_data.type = "ORTHO"; cam_data.ortho_scale = 0.48

M = {}; KL.materials(S, M)
for name, values in cfg.get("materials", {}).items():
    principled = next(n for n in M[name].node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    for prop, value in values.items(): principled.inputs[prop].default_value = value

roots = []
catcher = None
if cfg.get("shadows", False):
    bpy.ops.mesh.primitive_plane_add(size=2, location=(0,0,-.0005))
    catcher = bpy.context.object; catcher.name = "Contact shadow catcher"
    catcher.is_shadow_catcher = True
    catcher.data.materials.append(S.mat("Panel receiver", base=(.025,.021,.017), rough=.8))
for style, x, radius, anchor in (("dome", -.16, .029, 35), ("scallop", 0, .0444, 30), ("space", .16, .038, -100)):
    settings = cfg.get(style, {})
    if "geometry" in settings: os.environ["KNOB_" + style.upper()] = json.dumps(settings["geometry"])
    coll = bpy.data.collections.new(style); sc.collection.children.link(coll)
    style_materials = dict(M)
    for name, values in settings.get("materials", {}).items():
        style_materials[name] = M[name].copy()
        principled = next(n for n in style_materials[name].node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        for prop, value in values.items(): principled.inputs[prop].default_value = value
    root = KL.build(coll, style_materials, radius * settings.get("rscale", 1), style)
    root.location = (x, settings.get("y", 0), 0); KL.spin(root, anchor)
    receiver = bpy.data.collections.new(style + " receivers")
    if catcher: receiver.objects.link(catcher)
    for ob in root.children_recursive:
        if ob.type == "MESH": receiver.objects.link(ob)
    for light_name, defaults in (("Key", dict(power=6, pos=[0, .26, .16], size=[.38, .035])),
                                 ("Fill", dict(power=.08, pos=[0, 0, .4], size=[.4, .3])),
                                 ("Bounce", dict(power=0, pos=[0, -.24, .08], size=[.3, .12], specular=0)),
                                 ("Rim", dict(power=0, pos=[0,.26,.06], size=[.3,.05]))):
        light_cfg = {**defaults, **cfg.get(light_name.lower(), {}), **settings.get(light_name.lower(), {})}
        light_data = bpy.data.lights.new(style + light_name, "AREA")
        light_data.energy = light_cfg["power"]
        light_data.specular_factor = light_cfg.get("specular", 1)
        light_data.shape = "RECTANGLE"
        light_data.size, light_data.size_y = light_cfg["size"]
        light_data.color = light_cfg.get("color", [1, .96, .9])
        light = bpy.data.objects.new(style + light_name, light_data); sc.collection.objects.link(light)
        light.location = Vector(light_cfg["pos"]) + Vector((x, 0, 0))
        light.rotation_euler = (Vector((x, 0, radius * .65)) - light.location).to_track_quat("-Z", "Y").to_euler()
        light.light_linking.receiver_collection = receiver
    roots.append((style, root, anchor))

sc.render.filepath = str(out / "reference_angles.png")
bpy.ops.render.render(write_still=True)
angles = cfg.get("angles", [])
sc.frame_start = 0; sc.frame_end = max(1,len(angles)-1)
fixed = {o.name:list(v for row in o.matrix_world for v in row) for o in sc.objects if o.type in ("CAMERA","LIGHT")}
for index, angle in enumerate(angles):
    for style, root, anchor in roots:
        KL.spin(root, angle)
        spin = next(c for c in root.children_recursive if c.name.startswith("Spin"))
        spin.keyframe_insert("rotation_euler", index=2, frame=index)
sc.frame_set(len(angles)//2 if angles else 0)
bpy.ops.wm.save_as_mainfile(filepath=str(out / "knob_studio.blend"))
for index, angle in enumerate(angles):
    sc.frame_set(index)
    sc.render.filepath = str(out / ("rotation_%04d.png" % index))
    bpy.ops.render.render(write_still=True)
    for o in sc.objects:
        if o.name in fixed:
            assert fixed[o.name] == list(v for row in o.matrix_world for v in row), "Camera/light moved during rotation"
(out / "render.json").write_text(json.dumps(dict(config=cfg, angles=angles,
    width=480, height=144, scale=scale, centres=[80, 240, 400], centre_y=72,
    styles=[r[0] for r in roots], anchor_angles=[r[2] for r in roots]), indent=2))
