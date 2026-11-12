import bpy
m = bpy.data.materials.new("x"); m.use_nodes = True
n = m.node_tree.nodes.new("ShaderNodeMix"); n.data_type = "RGBA"
print("IN", [(i, s.name, s.type, s.enabled) for i, s in enumerate(n.inputs)])
print("OUT", [(i, s.name, s.type, s.enabled) for i, s in enumerate(n.outputs)])
