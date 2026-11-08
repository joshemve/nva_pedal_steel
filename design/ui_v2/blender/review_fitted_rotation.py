"""Motion evidence for an unapproved fitted candidate; never a production export."""
import bpy,json,sys,math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import knob_studio_geometry as KL
ROOT=Path(__file__).resolve().parents[3]
source=ROOT/'design/ui_v2/final/knobs/cycles_fit_shoulder'
out=ROOT/'design/ui_v2/final/knobs/review_2026_11_07';out.mkdir(exist_ok=True)
data=json.loads((source/'best.json').read_text());p=data['parameters']
bpy.ops.wm.open_mainfile(filepath=str(source/'fitted_studio.blend'))
sc=bpy.context.scene;sc.cycles.samples=64
sc.render.resolution_x=sc.render.resolution_y=336
pointer=next(o for o in bpy.data.collections['scallop'].objects if o.name.startswith('Pointer'))
old=pointer.data;mat=old.materials[0]
pointer.data=KL.pointer_mesh('Inlaid pointer',data['profile'],.0444*p[0],.0444*p[0]*p[2],.36,.88,.105,0,cap_round=.2)
pointer.data.materials.append(mat);pointer.hide_render=False
spin=pointer.parent
fixed={o.name:tuple(v for row in o.matrix_world for v in row) for o in sc.objects if o.type in ('LIGHT','CAMERA')}
spin.rotation_euler.z=math.radians(60)
sc.render.filepath=str(out/'feel_reference_angle.png');bpy.ops.render.render(write_still=True)
angles=[-135+270*i/60 for i in range(61)]
sc.frame_start=0;sc.frame_end=60
for i,angle in enumerate(angles):
    spin.rotation_euler.z=math.radians(90-angle)
    spin.keyframe_insert('rotation_euler',index=2,frame=i)
    sc.frame_set(i)
    sc.render.filepath=str(out/f'feel_{i:03d}.png');bpy.ops.render.render(write_still=True)
    assert all(fixed[o.name]==tuple(v for row in o.matrix_world for v in row) for o in sc.objects if o.name in fixed)
sc.frame_set(30)
bpy.ops.wm.save_as_mainfile(filepath=str(out/'feel_unapproved.blend'))
(out/'motion_check.json').write_text(json.dumps({'angles':angles,'fixed_camera_and_lights':True,'reference_texture_used':False,'approval':'rejected: shoulder and highlight fidelity remain unresolved'},indent=2))
