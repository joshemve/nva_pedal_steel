"""Fit a fixed, camera-space lighting response to smooth Blender geometry.

The lighting map is indexed by the *world surface normal*, not object UVs.
Rotating the knob changes reflected lighting. It does not rotate a lit photo.
"""
from pathlib import Path
import json
import numpy as np
from scipy import ndimage as ndi
from scipy.interpolate import griddata
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'design/ui_v2/final/knobs/lighting_fit';OUT.mkdir(exist_ok=True)
data=json.loads((ROOT/'design/ui_v2/final/knobs/parametric_fit_v2/surface_9.json').read_text())
h=np.array(data['height']);mask=np.array(data['mask'],bool);n=data['size'];c=n/2
ref=np.array(Image.open(ROOT/'design/ui_v2/APPROVED_main.png').convert('RGB').crop((376,722,488,834)),float)/255
target=np.where(ref<=.04045,ref/12.92,((ref+.055)/1.055)**2.4)
gy,gx=np.gradient(h); normal=np.stack([-gx,gy,np.ones_like(h)],-1);normal/=np.linalg.norm(normal,axis=-1)[...,None]
ink=Image.new('L',(n,n));ImageDraw.Draw(ink).polygon([(59,40),(64,43),(79,21),(71,16)],fill=255)
valid=mask&~(np.array(ink)>0)&(ndi.distance_transform_edt(mask)>1)
points=normal[valid,:2]; colours=target[valid]
points,inverse=np.unique(np.round(points,4),axis=0,return_inverse=True)
colours=np.stack([np.bincount(inverse,weights=colours[:,j])/np.bincount(inverse) for j in range(3)],-1)
S=512; axis=np.linspace(-1,1,S); yy,xx=np.meshgrid(axis,axis,indexing='ij')
lighting=griddata(points,colours,(xx,yy),method='linear')
missing=~np.isfinite(lighting[...,0])
lighting[missing]=griddata(points,colours,np.stack([xx[missing],yy[missing]],-1),method='nearest')
lighting=np.stack([ndi.gaussian_filter(lighting[...,j],.55) for j in range(3)],-1)
def srgb(a):
 a=np.maximum(a,0);return np.clip(np.where(a<=.0031308,a*12.92,1.055*a**(1/2.4)-.055),0,1)
coords=np.stack([(normal[...,1]+1)*(S-1)/2,(normal[...,0]+1)*(S-1)/2])
pred=np.stack([ndi.map_coordinates(lighting[...,j],coords,order=1) for j in range(3)],-1)
bg=np.zeros_like(ref)+np.array([30,26,21])/255
pair=np.concatenate([np.where(mask[...,None],ref,bg),np.where(mask[...,None],srgb(pred),bg)],1)
Image.fromarray(np.uint8(pair*255)).resize((672,336)).save(OUT/'fit.png')
Image.fromarray(np.uint8(srgb(lighting)*255)).save(OUT/'lighting_debug.png')
data.update(lighting_map=lighting.tolist(),lighting_map_size=S,
 rgb_mae=float(np.abs(srgb(pred)[valid]-ref[valid]).mean()*255),
 method='fitted fixed lighting lookup driven by world normals of smooth 3D geometry; no object-UV colour texture')
(OUT/'surface.json').write_text(json.dumps(data))
print('Reference-pose body RGB MAE:',data['rgb_mae'])
