"""Fit physical cap geometry and fixed studio lights to the supplied reference.

Only global material/light and smooth lathe parameters are optimized. There are
no reference textures, per-pixel paint corrections, or moving studio lights.
"""
import bpy, json, sys, math, time
from pathlib import Path
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import knob_studio_geometry as KL

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'design/ui_v2/final/knobs/cycles_fit';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'design/ui_v2/final/knobs/studio_v40_cpu/knob_studio.blend'))
sc=bpy.context.scene;sc.cycles.device='CPU';sc.cycles.samples=24
sc.render.resolution_x=224;sc.render.resolution_y=224
sc.camera.data.ortho_scale=.112
sc.render.threads_mode='FIXED';sc.render.threads=8
for ob in sc.objects:
    if ob.type=='MESH' and ob.name not in ('KnobBody.001','Pointer.001'):
        ob.hide_render=True
body=next(o for o in bpy.data.collections['scallop'].objects if o.name.startswith('KnobBody'))
pointer=next(o for o in bpy.data.collections['scallop'].objects if o.name.startswith('Pointer'))
body.hide_render=False;pointer.hide_render=True
root=next(o for o in bpy.data.collections['scallop'].objects if o.name.startswith('Knob_scallop'))
tilt=body.parent.parent
key=bpy.data.objects['scallopKey'];rim=bpy.data.objects['scallopRim']
mat=body.data.materials[0];bs=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
tar=np.load(OUT/'target.npz');rgb=tar['rgb'];alpha=tar['alpha'];valid=tar['valid']
bg=np.array([30,26,21])/255
target=rgb*alpha[...,None]+bg*(1-alpha[...,None])

# radius, y, height/radius, tilt, groove depth, phase, shoulder radius,
# shoulder height, cap curvature, light elevation, energy, strip width,
# ambient, roughness, base RGB, rim energy, groove sharpness.
lower=np.array([.91,-.012,.35,0,.015,0,.65,.65,.005,25,2,.01,.05,.12,.40,.20,.04,0,.5])
upper=np.array([1.12,.004,1.1,30,.15,1,.9,1.10,.38,82,14,.10,.8,.40,.90,.50,.20,6,3])
initial=np.array([1,-.004,.65,8,.065,.75,.8,1.02,.30,44,6,.025,.18,.27,.68,.30,.06,2.5,1])

def decode(u):
    p=lower+np.clip(u,0,1)*(upper-lower)
    rs,y,hr,ti,depth,phase,rf,zf,bulge,el,power,width,ambient,rough,r,g,b,rim_power,sharp=p
    prof=[[1,0,1],[1,.25,1],[.985,.5,1],[.95,.70,1],[(.95+rf)/2,(.70+zf)/2,.85],
          [rf,zf,.45],[rf*.875,zf+bulge*.45,.06],[rf*.75,zf+bulge*.70,0],
          [rf*.5,zf+bulge*.90,0],[rf*.25,zf+bulge*.98,0],[0,zf+bulge,0]]
    return p,prof

def apply(u):
    p,prof=decode(u)
    rs,y,hr,ti,depth,phase,rf,zf,bulge,el,power,width,ambient,rough,r,g,b,rim_power,sharp=p
    radius=.0444*rs
    old=body.data;body.data=KL.lathe('Fitted molded cap',prof,radius,radius*hr,9,depth,sharp,phase=phase)
    body.data.materials.append(mat)
    if old.users==0:bpy.data.meshes.remove(old)
    root.location.y=y;tilt.rotation_euler.x=math.radians(-ti)
    key.location=(0,.36*math.sin(math.radians(el)),.36*math.cos(math.radians(el)))
    key.rotation_euler=(Vector((0,0,.03))-key.location).to_track_quat('-Z','Y').to_euler()
    key.data.energy=power;key.data.size_y=width;rim.data.energy=rim_power
    sc.world.node_tree.nodes['Background'].inputs[1].default_value=ambient
    bs.inputs['Roughness'].default_value=rough;bs.inputs['Base Color'].default_value=(r,g,b,1)
    return p,prof

best=float('inf');best_u=None;counter=0
def evaluate(u):
    global best,best_u,counter
    p,prof=apply(u);counter+=1
    sc.render.filepath=str(OUT/'current.png');bpy.ops.render.render(write_still=True)
    im=bpy.data.images.load(str(OUT/'current.png'),check_existing=False)
    a=np.array(im.pixels[:]).reshape(224,224,4)[::-1]
    bpy.data.images.remove(im)
    # Byte PNG image.pixels returns normalized display-encoded values.
    # Verified against Pillow: 189/255 == 0.74117648. Do not gamma it twice.
    a=a.reshape(112,2,112,2,4).mean((1,3))
    composited=a[...,:3]*a[...,3,None]+bg*(1-a[...,3,None])
    error=((composited-target)**2).mean(-1)
    color=float(error[valid].mean())
    shape=float(((a[...,3]-alpha)**2).mean())
    score=color+shape*.12
    if score<best:
        best=score;best_u=np.clip(u,0,1).copy()
        (OUT/'best.json').write_text(json.dumps(dict(score=score,color=color,shape=shape,index=counter,parameters=p.tolist(),normalized=best_u.tolist(),profile=prof),indent=2))
        (OUT/'best.png').write_bytes((OUT/'current.png').read_bytes())
        print('FIT_BEST',counter,round(score,6),flush=True)
    return score

# Bounded covariance adaptation; seeded for reproducible comparisons.
rng=np.random.default_rng(7482);n=len(initial);mu=6;lam=14
mean=(initial-lower)/(upper-lower);cov=np.eye(n);sigma=.14
weights=np.log(mu+.5)-np.log(np.arange(1,mu+1));weights/=weights.sum()
evaluate(mean)
for generation in range(30):
    ev,vec=np.linalg.eigh(cov);transform=vec@np.diag(np.sqrt(np.maximum(ev,.015)))
    candidates=np.clip(mean+sigma*(rng.normal(size=(lam,n))@transform.T),0,1)
    candidates[0]=best_u
    scores=np.array([evaluate(c) for c in candidates])
    selected=candidates[np.argsort(scores)[:mu]]
    previous=mean.copy();mean=weights@selected
    steps=(selected-previous)/sigma
    cov=.88*cov+.12*np.einsum('i,ij,ik->jk',weights,steps,steps)
    sigma*=.985
    print('FIT_GENERATION',generation,round(best,6),flush=True)
apply(best_u)
sc.cycles.samples=128;sc.render.resolution_x=448;sc.render.resolution_y=448
sc.render.filepath=str(OUT/'best_large.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'fitted_studio.blend'))
