"""Fit object-space surface normals for smooth geometry, not baked illumination.

Normal vectors rotate with the mesh; diffuse fill and specular strip directions
remain fixed. Reference RGB is used only as a fitting target, never as a texture.
"""
from pathlib import Path
import json
import numpy as np
from scipy import ndimage as ndi
from scipy.interpolate import RBFInterpolator
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parents[1]; BASE=ROOT/'design/ui_v2/final/knobs/parametric_fit_v2/surface_9.json'
OUT=ROOT/'design/ui_v2/final/knobs/normal_fit_v2'; OUT.mkdir(exist_ok=True)
data=json.loads(BASE.read_text()); height=np.array(data['height']); mask=np.array(data['mask'],bool)
n=data['size']; c=n/2; yy,xx=np.mgrid[:n,:n]; radius=np.hypot(xx-c,yy-c)
ref=np.array(Image.open(ROOT/'design/ui_v2/APPROVED_main.png').convert('RGB').crop((376,722,488,834)),float)/255
target=np.where(ref<=.04045,ref/12.92,((ref+.055)/1.055)**2.4)
inkim=Image.new('L',(n,n)); ImageDraw.Draw(inkim).polygon([(59,40),(64,43),(79,21),(71,16)],fill=255)
ink=np.array(inkim)>0
valid=mask&~ink&(ndi.distance_transform_edt(mask)>1)
gy,gx=np.gradient(height)
normal0=np.stack([-gx,gy,np.ones_like(gx)],-1); normal0/=np.linalg.norm(normal0,axis=-1)[...,None]
normal=normal0.copy()
L=np.array([0.,-.2,.98]); L/=np.linalg.norm(L)
H=np.array([0.,.6,.8]); H/=np.linalg.norm(H)
ambient=np.array([.012,.005,.002]); diffuse=np.array([.76,.47,.235]); specular=np.array([.4,.384,.36]); power=80
ds=(target-ambient)@np.linalg.pinv(np.stack([diffuse,specular],axis=1)).T
glint=(ds[...,1]>.12)&valid&(yy<c)
dd=np.clip(ds[...,0],0,1); qq=np.clip(ds[...,1],0,1)**(1/power)
gamma=float(L@H)
aa=(dd-gamma*qq)/(1-gamma*gamma); bb=(qq-gamma*dd)/(1-gamma*gamma)
candidate=aa[...,None]*L+bb[...,None]*H
nx=np.sqrt(np.maximum(0,1-(candidate*candidate).sum(-1)))
candidate[...,0]=np.where(xx<c,-nx,nx)
candidate/=np.maximum(np.linalg.norm(candidate,axis=-1)[...,None],1e-8)
normal[glint]=candidate[glint]
weight=valid.astype(float); weight/=weight.sum()
mom=np.zeros_like(normal); var=mom.copy()

def shade(N):
    d=np.maximum(0,N@L); s=np.maximum(0,N@H)
    return ambient+d[...,None]*diffuse+(s**power)[...,None]*specular,d,s
def srgb(a):
    a=np.maximum(a,0); return np.clip(np.where(a<=.0031308,a*12.92,1.055*a**(1/2.4)-.055),0,1)

for step in range(1,3001):
    pred,d,s=shade(normal); err=pred-target
    grgb=2*weight[...,None]*err
    grad=(grgb@diffuse)[...,None]*(d>0)[...,None]*L
    grad+=(grgb@specular)[...,None]*(power*s**(power-1))[...,None]*H
    grad+=.0008*(normal-normal0)/valid.sum()
    grad-=.0006*np.stack([ndi.laplace(normal[...,j]) for j in range(3)],-1)/valid.sum()
    grad*=mask[...,None]
    grad-=normal*(grad*normal).sum(-1)[...,None]
    mom=.9*mom+.1*grad; var=.999*var+.001*grad*grad
    normal-=.012*(mom/(1-.9**step))/(np.sqrt(var/(1-.999**step))+1e-9)
    normal[...,2]=np.maximum(normal[...,2],.015)
    # Retain the outward-facing cap branch of the photometric ambiguity.
    normal[...,0]=np.where(xx<c,-np.abs(normal[...,0]),np.abs(normal[...,0]))
    normal/=np.linalg.norm(normal,axis=-1)[...,None]
    if step%500==0: print(step,round(float(np.abs(srgb(pred)[valid]-ref[valid]).mean()*255),3),flush=True)

hole=ink&mask; ring=ndi.binary_dilation(hole,iterations=6)&~hole&mask
for j in range(3):
    normal[...,j][hole]=RBFInterpolator(np.argwhere(ring),normal[...,j][ring],smoothing=.5)(np.argwhere(hole))
normal/=np.linalg.norm(normal,axis=-1)[...,None]
pred,d,s=shade(normal)
# A restrained, smooth material-colour correction, excluding glints entirely.
# The diagnostic albedo is exported so baked highlights can be checked directly.
residual=np.clip((target-ambient-(s**power)[...,None]*specular)/(np.maximum(d,.08)[...,None]*diffuse),.8,1.2)
matvalid=valid&~ndi.binary_dilation(glint,iterations=2)
albedo=np.stack([ndi.gaussian_filter(residual[...,j]*matvalid,3)/np.maximum(ndi.gaussian_filter(matvalid.astype(float),3),1e-5) for j in range(3)],-1)
albedo=np.where((ndi.gaussian_filter(matvalid.astype(float),3)>.01)[...,None],albedo,1)
pred=ambient+d[...,None]*diffuse*albedo+(s**power)[...,None]*specular
bg=np.zeros_like(ref)+np.array([30,26,21])/255
pair=np.concatenate([np.where(mask[...,None],ref,bg),np.where(mask[...,None],srgb(pred),bg)],1)
Image.fromarray(np.uint8(pair*255)).resize((672,336)).save(OUT/'fit.png')
data.update(light=L.tolist(),half_vector=H.tolist(),coeff=[ambient.tolist(),diffuse.tolist(),[0,0,0],specular.tolist()],exponents=[18,power],normal_map=normal.tolist(),
            albedo_map=albedo.tolist(),rgb_mae=float(np.abs(srgb(pred)[valid]-ref[valid]).mean()*255),method='smooth mesh + fitted object-space normals under fixed fill and strip illumination')
(OUT/'surface.json').write_text(json.dumps(data))
Image.fromarray(np.uint8(np.clip(normal*.5+.5,0,1)*255)).save(OUT/'normal_debug.png')
Image.fromarray(np.uint8(srgb(diffuse*albedo)*255)).save(OUT/'albedo_debug.png')
