"""Render a fitted 3D surface with fixed normal-driven studio illumination.

The material contains diffuse and Blinn specular lobes, never the reference image.
Both surface normals and geometry rotate. Source depth is estimated, not measured.
"""
import bpy, sys, json, math
import numpy as np
from pathlib import Path

args=sys.argv[sys.argv.index('--')+1:]
data=json.loads(Path(args[0]).read_text()); out=Path(args[1]).resolve(); out.mkdir(exist_ok=True,parents=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene; sc.render.engine='CYCLES'; sc.cycles.samples=32
sc.render.film_transparent=True
sc.render.image_settings.color_mode='RGBA'; sc.render.image_settings.file_format='PNG'
sc.view_settings.view_transform='Standard'; sc.view_settings.look='None'; sc.view_settings.exposure=0
n=data['size']; c=n/2; height=np.array(data['height']); mask=np.array(data['mask'],bool)
pixel_size=data.get('pixel_size',1); logical_size=data.get('logical_size',n)
sc.render.resolution_x=sc.render.resolution_y=logical_size*4; sc.render.resolution_percentage=100
cam_d=bpy.data.cameras.new('ReferenceCamera'); cam=bpy.data.objects.new('ReferenceCamera',cam_d)
sc.collection.objects.link(cam); cam.location=(0,0,.5); cam_d.type='ORTHO'; cam_d.ortho_scale=logical_size*.001; sc.camera=cam
root=bpy.data.objects.new('Rotating knob',None); sc.collection.objects.link(root)
verts=[]; normals=[]; indices={}
gy,gx=np.gradient(height)
for y,x in np.argwhere(mask):
    indices[y,x]=len(verts)
    verts.append(((x-c+.5)*.001*pixel_size,(c-y-.5)*.001*pixel_size,height[y,x]*.001))
    normal=np.array(data['mesh_normals'][y][x]) if 'mesh_normals' in data else np.array([-gx[y,x]/pixel_size,gy[y,x]/pixel_size,1.])
    normal/=np.linalg.norm(normal); normals.append(tuple(normal))
faces=[]
for y in range(n-1):
    for x in range(n-1):
        quad=[indices.get((y,x)),indices.get((y+1,x)),indices.get((y+1,x+1)),indices.get((y,x+1))]
        if all(i is not None for i in quad): faces.append(quad)
mesh=bpy.data.meshes.new('Fitted moulded surface'); mesh.from_pydata(verts,[],faces); mesh.update()
for p in mesh.polygons:p.use_smooth=True
mesh.normals_split_custom_set_from_vertices(normals)
ob=bpy.data.objects.new('Cream knob body',mesh); sc.collection.objects.link(ob); ob.parent=root
normal_attribute=mesh.attributes.new('Calibrated surface normal', 'FLOAT_VECTOR', 'POINT')
for item, normal in zip(normal_attribute.data, normals): item.vector=normal

def material(name,coeff):
    mat=bpy.data.materials.new(name); mat.use_nodes=True; nt=mat.node_tree; nt.nodes.clear()
    nodes,links=nt.nodes,nt.links
    geo=nodes.new('ShaderNodeNewGeometry')
    normal_socket=geo.outputs['Normal']
    if 'mesh_normals' in data:
        attr=nodes.new('ShaderNodeAttribute');attr.attribute_name='Calibrated surface normal'
        transform=nodes.new('ShaderNodeVectorTransform');transform.vector_type='NORMAL';transform.convert_from='OBJECT';transform.convert_to='WORLD'
        links.new(attr.outputs['Vector'],transform.inputs['Vector'])
        norm=nodes.new('ShaderNodeVectorMath');norm.operation='NORMALIZE';links.new(transform.outputs['Vector'],norm.inputs[0]);normal_socket=norm.outputs[0]
    def dot(vec):
        node=nodes.new('ShaderNodeVectorMath'); node.operation='DOT_PRODUCT'; links.new(normal_socket,node.inputs[0]); node.inputs[1].default_value=vec
        clamp=nodes.new('ShaderNodeMath'); clamp.operation='MAXIMUM'; clamp.inputs[1].default_value=0; links.new(node.outputs['Value'],clamp.inputs[0]); return clamp.outputs[0]
    L=np.array(data['light']); H=L+np.array([0,0,1]); H/=np.linalg.norm(H)
    if 'half_vector' in data: H=np.array(data['half_vector'])
    d,s=dot(L),dot(H)
    result=nodes.new('ShaderNodeVectorMath'); result.operation='ADD'; result.inputs[0].default_value=coeff[0]
    scale=nodes.new('ShaderNodeVectorMath'); scale.operation='SCALE'; scale.inputs[0].default_value=coeff[1]; links.new(d,scale.inputs[3]); links.new(scale.outputs[0],result.inputs[1])
    for k,power in enumerate(data.get('exponents',[18,90])):
        pow=nodes.new('ShaderNodeMath'); pow.operation='POWER'; links.new(s,pow.inputs[0]); pow.inputs[1].default_value=power
        spec=nodes.new('ShaderNodeVectorMath'); spec.operation='SCALE'; spec.inputs[0].default_value=coeff[k+2]; links.new(pow.outputs[0],spec.inputs[3])
        add=nodes.new('ShaderNodeVectorMath'); add.operation='ADD'; links.new(result.outputs[0],add.inputs[0]); links.new(spec.outputs[0],add.inputs[1]); result=add
    emit=nodes.new('ShaderNodeEmission'); links.new(result.outputs[0],emit.inputs['Color'])
    output=nodes.new('ShaderNodeOutputMaterial'); links.new(emit.outputs[0],output.inputs['Surface'])
    return mat
if 'lighting_map' in data:
    rgb=np.array(data['lighting_map'],dtype=np.float32); sz=data['lighting_map_size']
    img=bpy.data.images.new('Fixed studio lighting response',width=sz,height=sz,float_buffer=True)
    img.colorspace_settings.name='Non-Color'
    rgba=np.concatenate([rgb,np.ones((sz,sz,1),np.float32)],-1)
    img.pixels.foreach_set(rgba.ravel()); img.pack()
    mat=bpy.data.materials.new('Normal-driven fixed studio lighting');mat.use_nodes=True
    nt=mat.node_tree;nt.nodes.clear();nodes,links=nt.nodes,nt.links
    geometry=nodes.new('ShaderNodeNewGeometry')
    scale=nodes.new('ShaderNodeVectorMath');scale.operation='MULTIPLY';scale.inputs[1].default_value=(.5,.5,0);links.new(geometry.outputs['Normal'],scale.inputs[0])
    offset=nodes.new('ShaderNodeVectorMath');offset.operation='ADD';offset.inputs[1].default_value=(.5,.5,0);links.new(scale.outputs[0],offset.inputs[0])
    texture=nodes.new('ShaderNodeTexImage');texture.image=img;texture.interpolation='Linear';texture.extension='EXTEND';links.new(offset.outputs[0],texture.inputs['Vector'])
    emit=nodes.new('ShaderNodeEmission');links.new(texture.outputs['Color'],emit.inputs['Color'])
    output=nodes.new('ShaderNodeOutputMaterial');links.new(emit.outputs[0],output.inputs['Surface'])
    mesh.materials.append(mat)
else:
    mesh.materials.append(material('Fixed studio / fitted bakelite',data['coeff']))

# Separate inlaid pointer; its hidden underlying cap was interpolated during fitting.
def sample(x,y):
    x=float(np.clip(x,0,n-1.001)); y=float(np.clip(y,0,n-1.001))
    xi,yi=int(x),int(y); u,v=x-xi,y-yi
    return float(height[yi,xi]*(1-u)*(1-v)+height[yi,xi+1]*u*(1-v)+height[yi+1,xi]*(1-u)*v+height[yi+1,xi+1]*u*v)
p0=np.array([64.,39.])/pixel_size; p1=np.array([73.,22.])/pixel_size; direction=p1-p0; side=np.array([-direction[1],direction[0]])/np.linalg.norm(direction)*1.65/pixel_size
pv=[]
for i in range(41):
    pos=p0+(p1-p0)*i/40
    for sign in [-1,1]:
        x,y=pos+side*sign
        pv.append(((x-c+.5)*.001*pixel_size,(c-y-.5)*.001*pixel_size,(sample(x,y)+.5)*.001))
pm=bpy.data.meshes.new('Pointer inlay'); pm.from_pydata(pv,[],[(2*i,2*i+1,2*i+3,2*i+2) for i in range(40)]); pm.update()
pointer=bpy.data.objects.new('Pointer',pm); sc.collection.objects.link(pointer); pointer.parent=root
pm.materials.append(material('Black pointer paint',[[.003,.0025,.002],[.005,.004,.003],[.002,.002,.002],[.01,.01,.01]]))

sc.render.filepath=str(out/'reference_pose.png'); bpy.ops.render.render(write_still=True)
angles=[-135,-90,-45,0,30,60,90,135]
sc.frame_start=0; sc.frame_end=len(angles)-1
for i,ang in enumerate(angles):
    root.rotation_euler.z=math.radians(-(ang-data['angle']))
    root.keyframe_insert('rotation_euler',index=2,frame=i)
    sc.frame_set(i)
    sc.render.filepath=str(out/f'angle_{i:02d}.png'); bpy.ops.render.render(write_still=True)
sc.frame_set(4)
bpy.ops.wm.save_as_mainfile(filepath=str(out/'fitted_knob.blend'))
(out/'angles.json').write_text(json.dumps(angles))
