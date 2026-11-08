"""Fit a small, smooth radial model to the approved knob, with real rotating normals.

Spline cap + repeated finger grooves. No image-colour texture, arbitrary per-pixel
displacement, or screen-fixed image correction is used in the resulting material.
"""
from pathlib import Path
import json
import numpy as np
from scipy import ndimage as ndi
from scipy.interpolate import BSpline
from scipy.optimize import least_squares, lsq_linear
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'design/ui_v2/final/knobs/parametric_fit_v3'; OUT.mkdir(exist_ok=True)
N=112; C=N/2; cx,cy=432,778
ref=np.array(Image.open(ROOT/'design/ui_v2/APPROVED_main.png').convert('RGB').crop((cx-56,cy-56,cx+56,cy+56)),float)/255
target=np.where(ref<=.04045,ref/12.92,((ref+.055)/1.055)**2.4)
yy,xx=np.mgrid[:N,:N]; rad=np.hypot(xx-C,yy-C); theta=np.arctan2(yy-C,xx-C)
raw=(ref[...,0]>.28)&(ref[...,0]-ref[...,2]>.09)&(rad<49)
lab,_=ndi.label(raw); mask=ndi.binary_fill_holes(lab==lab[56,56])
# Smooth measured polar outline. This preserves the actual circumference without
# encoding grain, pointer edges, or illumination into the geometry.
ts=np.linspace(-np.pi,np.pi,720,endpoint=False); rs=np.linspace(20,49,291)
mx=np.rint(C+np.cos(ts[:,None])*rs).astype(int); my=np.rint(C+np.sin(ts[:,None])*rs).astype(int)
outline=np.where(mask[my,mx],rs[None,:],0).max(1)
outline=ndi.gaussian_filter1d(outline,5,mode='wrap')
boundary=np.interp(theta,ts,outline,period=2*np.pi); rho=rad/boundary
mask=rho<1
inkim=Image.new('L',(N,N)); ImageDraw.Draw(inkim).polygon([(59,40),(64,43),(79,21),(71,16)],fill=255)
ink=np.array(inkim)>0
valid=mask&~ink&(rho<.985)
local_contrast=np.abs(ref.mean(-1)-ndi.gaussian_filter(ref.mean(-1),2))
pixel_weight=np.sqrt(1+12*np.clip(local_contrast/.05,0,1))[valid]
knots=np.array([0,0,0,0,.3,.55,.7,.82,.92,1,1,1,1])
count=len(knots)-4
L=np.array([-.08,.76,.645]); L/=np.linalg.norm(L); H=L+np.array([0,0,1]); H/=np.linalg.norm(H)
white=np.array([1,.96,.9]); ambient_colour=np.array([1,.45,.17]); coeff=np.array([[.03,.015,.005],[.9,.55,.25],[.05,.048,.045],[.2,.192,.18]])

def make_basis(rho,theta,mask,lobes):
    spl=np.stack([BSpline(knots,np.eye(count)[i],3)(np.clip(rho,0,1)) for i in range(count)])
    # Equal centre coefficients give a horizontal tangent at the shaft axis;
    # the last coefficient is fixed at zero at the body's lower edge.
    b=[spl[0]+spl[1],*spl[2:-1]]
    for centre,width in [(.72,.18),(.88,.10)]:
        envelope=np.exp(-((rho-centre)/width)**2)*rho**3*(1-rho)
        b.extend([envelope*np.cos(lobes*theta),envelope*np.sin(lobes*theta)])
    return np.asarray(b)*mask

def srgb(a):
    a=np.maximum(a,0); return np.clip(np.where(a<=.0031308,a*12.92,1.055*a**(1/2.4)-.055),0,1)

records=[]
for lobes in [9]:
    B=make_basis(rho,theta,mask,lobes)
    By,Bx=np.gradient(B,axis=(1,2))
    P=Bx[:,valid].T; Q=By[:,valid].T
    t=target[valid]; h0=28*(1-np.exp(-ndi.distance_transform_edt(mask)/6))
    init=np.linalg.lstsq(B[:7,mask].T,h0[mask],rcond=None)[0]
    pars=np.r_[np.clip(init,1,45),np.zeros(4)]
    best=None
    def evaluate(p,with_jac=False):
        px=P@p; py=Q@p
        v=np.stack([-px,py,np.ones_like(px)],-1); length=np.linalg.norm(v,axis=-1); normal=v/length[:,None]
        d=np.maximum(0,normal@L); s=np.maximum(0,normal@H)
        basis=np.stack([np.ones_like(d),d,s**18,s**200],-1)
        pred=basis@coeff
        if not with_jac:return pred,basis
        deriv=coeff[1][None,:,None]*L[None,None,:]*(d>0)[:,None,None]
        deriv=deriv+coeff[2][None,:,None]*(18*s**17)[:,None,None]*H[None,None,:]
        deriv=deriv+coeff[3][None,:,None]*(200*s**199)[:,None,None]*H[None,None,:]
        nv=np.stack([-P,Q,np.zeros_like(P)],1)
        dn=(nv-normal[:,:,None]*np.einsum('ni,nij->nj',normal,nv)[:,None,:])/length[:,None,None]
        jac=np.einsum('nci,nip->ncp',deriv,dn)
        return pred,jac
    # Alternating constrained material and surface fit, a few dozen parameters
    # rather than thousands of unconstrained heights.
    for it in range(14):
        pred,sh=evaluate(pars)
        A=np.zeros((len(t)*3,6))
        for c in range(3):
            A[c::3,0]=ambient_colour[c]; A[c::3,1+c]=sh[:,1]
            A[c::3,4]=sh[:,2]*white[c]; A[c::3,5]=sh[:,3]*white[c]
        weights=np.repeat(pixel_weight,3)
        v=lsq_linear(A*weights[:,None],t.ravel()*weights,bounds=([0,0,0,0,0,.2],[.12,1.5,1.5,1.5,.1,2]),tol=1e-7).x
        coeff[0]=v[0]*ambient_colour; coeff[1]=v[1:4]; coeff[2]=v[4]*white; coeff[3]=v[5]*white
        def residual(p):
            pred,_=evaluate(p)
            return np.r_[((pred-t)*pixel_weight[:,None]).ravel(), .03*(p-pars)]
        def jacobian(p):
            _,j=evaluate(p,True)
            return np.vstack([(j*pixel_weight[:,None,None]).reshape(-1,len(p)),.03*np.eye(len(p))])
        result=least_squares(residual,pars,jac=jacobian,bounds=(np.r_[np.ones(7)*.1,np.ones(4)*-40],np.r_[np.ones(7)*50,np.ones(4)*40]),max_nfev=35)
        pars=result.x
        print('lobes',lobes,'iteration',it,'MAE',round(float(np.abs(srgb(evaluate(pars)[0])-ref[valid]).mean()*255),3),flush=True)
    height=np.einsum('p,pyx->yx',pars,B)
    gy,gx=np.gradient(height); normal=np.stack([-gx,gy,np.ones_like(height)],-1); normal/=np.linalg.norm(normal,axis=-1)[...,None]
    d=np.maximum(0,normal@L); s=np.maximum(0,normal@H)
    pred=np.stack([np.ones_like(d),d,s**18,s**200],-1)@coeff
    rgb=srgb(pred); bg=np.zeros_like(ref)+np.array([30,26,21])/255
    pair=np.concatenate([np.where(mask[...,None],ref,bg),np.where(mask[...,None],rgb,bg)],axis=1)
    Image.fromarray(np.uint8(pair*255)).resize((672,336)).save(OUT/f'lobes_{lobes}.png')
    mae=float(np.abs(rgb[valid]-ref[valid]).mean()*255)
    info=dict(size=N,centre=[C,C],source_centre=[cx,cy],light=L.tolist(),coeff=coeff.tolist(),height=height.tolist(),
              mask=mask.astype(int).tolist(),angle=30,lobes=lobes,parameters=pars.tolist(),rgb_mae=mae,outline=outline.tolist(),exponents=[18,200])
    (OUT/f'surface_{lobes}.json').write_text(json.dumps(info))
    records.append(dict(lobes=lobes,rgb_mae=mae))
(OUT/'fit_report.json').write_text(json.dumps(records,indent=2))
