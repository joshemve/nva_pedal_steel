"""Headless test render of the instrument alone. blender -b -P test_steel.py -- out.png"""
import bpy, sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib, steel_lib; importlib.reload(steel_lib)
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
coll = bpy.data.collections.new("Steel"); sc.collection.children.link(coll)
M = steel_lib.materials()
steel_lib.build(coll, M)
# floor
bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, -0.107))
fl = bpy.context.active_object; fl.data.materials.append(steel_lib.mat("Floor", base=(0.72, 0.68, 0.6), rough=0.7))
# world HDRI
hdr = r"C:/Users/Joshua/Documents/farwilder/experiments/character_lab/downloads/hdri/studio_small_09_4k.hdr"
w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
nt = w.node_tree; env = nt.nodes.new("ShaderNodeTexEnvironment"); env.image = bpy.data.images.load(hdr)
nt.links.new(env.outputs[0], nt.nodes["Background"].inputs[0]); nt.nodes["Background"].inputs[1].default_value = 0.7
# key light upper left
bpy.ops.object.light_add(type="AREA", location=(-0.8, -0.9, 1.4)); k = bpy.context.active_object
k.data.energy = 120; k.data.size = 0.8; k.rotation_euler = (math.radians(40), 0, math.radians(-40))
# camera: player's seat, three-quarter, ~30 deg down
bpy.ops.object.camera_add(location=(0.05, -1.05, 0.62)); cam = bpy.context.active_object; sc.camera = cam
d = Vector((0.0, 0.0, -0.02)) - cam.location; cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
cam.data.lens = 50
sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = 96; sc.cycles.use_denoising = True
p = bpy.context.preferences.addons["cycles"].preferences; p.compute_device_type = "OPTIX"; p.get_devices()
for dv in p.devices: dv.use = dv.type == "OPTIX"
sc.render.resolution_x, sc.render.resolution_y = 1600, 900
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
sc.render.filepath = out
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=out.replace(".png", ".blend"))
