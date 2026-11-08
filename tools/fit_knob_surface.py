"""Constrained shape-from-shading study, using no reference-colour texture.

Fits a smooth height field and a low-dimensional diffuse/specular studio model.
The result is an estimated 3D surface, not calibrated depth from a photograph.
"""
from pathlib import Path
import json
import numpy as np
from scipy import ndimage as ndi
from scipy.optimize import nnls, lsq_linear
from scipy.interpolate import RBFInterpolator
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'design/ui_v2/final/knobs/surface_fit_bounded'; OUT.mkdir(exist_ok=True)
im=Image.open(ROOT/'design/ui_v2/APPROVED_main.png').convert('RGB')
N=112; C=N//2; cx,cy=432,778
ref=np.asarray(im.crop((cx-C,cy-C,cx+C,cy+C)),dtype=float)/255
target=np.where(ref<=.04045,ref/12.92,((ref+.055)/1.055)**2.4)
yy,xx=np.mgrid[:N,:N]; rad=np.hypot(xx-C,yy-C)
raw=(ref[...,0]>.28)&(ref[...,0]-ref[...,2]>.09)&(rad<49)
labels,num=ndi.label(raw)
mask=labels==labels[C,C]
mask=ndi.binary_fill_holes(mask)
mask=ndi.binary_closing(mask,iterations=1)
dist=ndi.distance_transform_edt(mask)
height0=(28*(1-np.exp(-dist/6))+2*np.maximum(0,1-(rad/44)**2))*mask
height=height0.copy()
# Pointer is not used as a surface/shading observation.
ink=(ref[...,0]<.35)&(rad<40)&(dist>4)
ilab,inum=ndi.label(ink)
counts=np.bincount(ilab.ravel()); counts[0]=0
ink=ndi.binary_dilation(ilab==counts.argmax(),iterations=3)
weight=(mask&~ink&(dist>1.5)).astype(float)
weight/=weight.sum()
L=np.array([-.08,.76,.645]); L/=np.linalg.norm(L)
H=L+np.array([0.,0.,1.]); H/=np.linalg.norm(H)
exponents=[18,90]
coeff=np.array([[.05,.025,.01],[.6,.3,.12],[.12,.12,.12],[.1,.1,.1]])

def shade(z):
    gy,gx=np.gradient(z)
    v=np.stack([-gx,gy,np.ones_like(z)],-1)
    length=np.linalg.norm(v,axis=-1)
    normal=v/length[...,None]
    d=np.maximum(0,normal@L); s=np.maximum(0,normal@H)
    basis=np.stack([np.ones_like(z),d,s**18,s**90],-1)
    return basis@coeff,normal,length,basis,d,s

def srgb(a):
    a=np.maximum(a,0)
    return np.clip(np.where(a<=.0031308,a*12.92,1.055*a**(1/2.4)-.055),0,1)

def save(step):
    pred,*_=shade(height)
    a=np.uint8(ref*255); b=np.uint8(srgb(pred)*255)
    bg=np.full_like(b,[30,26,21]); b=np.where(mask[...,None],b,bg)
    a=np.where(mask[...,None],a,bg)
    pair=np.concatenate([a,b],axis=1)
    Image.fromarray(pair).resize((N*6,N*3)).save(OUT/f'fit_{step:04d}.png')

mom=np.zeros_like(height); var=mom.copy()
history=[]
for step in range(1,4801):
    pred,normal,length,basis,d,s=shade(height)
    if step%40==1:
        valid=weight>0
        # Dielectric specular is the colour of the light, not the plastic.
        # Constrain both lobes to a shared warm-white spectrum.
        white=np.array([1,.96,.9]); B=basis[valid]; T=target[valid]
        design=np.zeros((len(B)*3,8))
        for c in range(3):
            design[c::3,c]=1; design[c::3,3+c]=B[:,1]
            design[c::3,6]=B[:,2]*white[c]
            design[c::3,7]=B[:,3]*white[c]
        values=lsq_linear(design,T.ravel(),bounds=(0,[.06,.04,.02,1.5,1.5,1.5,4,4]),tol=1e-6).x
        coeff[0]=values[:3]; coeff[1]=values[3:6]
        coeff[2]=values[6]*white; coeff[3]=values[7]*white
        pred=basis@coeff
    err=pred-target
    image_loss=float((weight[...,None]*err**2).sum())
    grgb=2*weight[...,None]*err
    gn=np.zeros_like(normal)
    gn+=(grgb@coeff[1])[...,None]*(d>0)[...,None]*L
    for j,p in enumerate(exponents):
        gn+=(grgb@coeff[j+2])[...,None]*(p*s**(p-1))[...,None]*H
    gv=(gn-normal*(gn*normal).sum(-1)[...,None])/length[...,None]
    gp=-gv[...,0]; gq=gv[...,1]
    grad=np.zeros_like(height)
    grad[:,2:]+=gp[:,1:-1]/2; grad[:,:-2]-=gp[:,1:-1]/2
    grad[2:,:]+=gq[1:-1,:]/2; grad[:-2,:]-=gq[1:-1,:]/2
    lap=ndi.laplace(height)
    grad+=.006*ndi.laplace(lap)/mask.sum()
    grad+=.00004*(height-height0)/mask.sum()
    grad*=mask
    mom=.9*mom+.1*grad; var=.999*var+.001*grad*grad
    height-=.08*(mom/(1-.9**step))/(np.sqrt(var/(1-.999**step))+1e-8)
    height=np.clip(height,0,55)*mask
    if step%400==0:
        mae=float(np.abs(srgb(pred)-ref)[weight>0].mean()*255)
        print(step,'linear loss',round(image_loss,6),'RGB MAE',round(mae,3),flush=True)
        history.append(dict(step=step,linear_loss=image_loss,rgb_mae=mae)); save(step)

# Fill the unobserved surface under the pointer from its neighbourhood. The
# pointer itself will be separate rotating geometry, never a baked dent.
hole=ink&mask
neighbour=ndi.binary_dilation(hole,iterations=9)&~hole&mask
interp=RBFInterpolator(np.argwhere(neighbour),height[neighbour],kernel='thin_plate_spline',smoothing=.2)
height[hole]=interp(np.argwhere(hole))
save(9999)
np.savez(OUT/'surface.npz',height=height,mask=mask,reference=ref,light=L,coeff=coeff,weight=weight)
(OUT/'surface.json').write_text(json.dumps(dict(size=N,centre=[C,C],source_centre=[cx,cy],
    light=L.tolist(),coeff=coeff.tolist(),height=height.tolist(),mask=mask.astype(int).tolist(),
    angle=30,history=history),indent=1))
print('coefficients',coeff.tolist())
