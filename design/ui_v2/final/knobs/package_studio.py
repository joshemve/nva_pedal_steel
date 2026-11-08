"""Package unmodified Cycles frames as GPU-sized tiled atlases and visual QA.

python package_studio.py studio_rotation
No image-based lighting correction or reference artwork is applied to a frame.
"""
import json, math, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

folder=Path(sys.argv[1]).resolve()
meta=json.loads((folder/'render.json').read_text())
angles=meta['angles']; scale=meta['scale']; n=len(angles)
frames=[Image.open(folder/f'rotation_{i:04d}.png').convert('RGBA') for i in range(n)]
out=folder/'assets'; out.mkdir(exist_ok=True)
qa=folder/'qa'; qa.mkdir(exist_ok=True)
manifest={'renderer':'Blender Cycles','reference':'../../../APPROVED_main.png',
          'lighting':'Fixed physical lights; rotating geometry; no screen-space recolouring',
          'alpha':'straight RGBA, body only; use a panel contact shadow underneath',
          'frames':n,'angles_degrees':angles,'scale':scale,'styles':{}}
sizes={'dome':90,'scallop':120,'space':108}
sprites={}
for j,style in enumerate(meta['styles']):
    side=sizes[style]*scale; x=meta['centres'][j]*scale; y=meta['centre_y']*scale
    box=(x-side//2,y-side//2,x+side//2,y+side//2)
    crops=[im.crop(box) for im in frames]; sprites[style]=crops
    edge_max=0
    for im in crops:
        a=np.asarray(im)[...,3]
        edge_max=max(edge_max,int(max(a[0].max(),a[-1].max(),a[:,0].max(),a[:,-1].max())))
    assert edge_max < 3, (style,'clipped edge',edge_max)
    cols=16; rows=math.ceil(n/cols)
    atlas=Image.new('RGBA',(cols*side,rows*side))
    for i,im in enumerate(crops): atlas.paste(im,((i%cols)*side,(i//cols)*side))
    atlas.save(out/f'knob_{style}_atlas.png',optimize=True)
    manifest['styles'][style]={'file':f'knob_{style}_atlas.png','frame_px':[side,side],
        'frame_ui_px':sizes[style],'columns':cols,'rows':rows,'pivot_px':[side/2,side/2],
        'reference_angle':meta['anchor_angles'][j], 'border_alpha_max':edge_max}

# Assemble the current UI plate using the exported atlas source frames.
base=folder.parent.parent/'strips'
plate=Image.open(base/'plate_walnut.png').convert('RGBA')
pscale=plate.width/1600
instances=[('dome',130,35),('dome',265,-50),('scallop',433,30),('scallop',572,25),
           ('space',733,-100),('space',860,-68)]
for style,x,angle in instances:
    index=min(range(n),key=lambda i:abs(angles[i]-angle))
    fr=sprites[style][index]
    if pscale != scale: fr=fr.resize((round(fr.width*pscale/scale),round(fr.height*pscale/scale)),Image.Resampling.LANCZOS)
    cx=x*1600/1568*pscale; cy=777*pscale
    plate.alpha_composite(fr,(round(cx-fr.width/2),round(cy-fr.height/2)))
plate.save(qa/'assembled_plate.png')
root=folder.parents[2]
ref=Image.open(root/'APPROVED_main.png').convert('RGB')
comp=plate.convert('RGB').resize(ref.size,Image.Resampling.LANCZOS)
box=(0,688,920,895); a=ref.crop(box); b=comp.crop(box)
old=Image.open(base/'_check_composite.jpg').convert('RGB').resize(ref.size,Image.Resampling.LANCZOS).crop(box)
comparison=Image.new('RGB',(920,3*(207+28)),(25,23,21)); draw=ImageDraw.Draw(comparison)
for i,(im,title) in enumerate([(a,'REFERENCE'),(old,'PREVIOUS EXPORT'),(b,'NEW NATIVE BLENDER EXPORT — SHAPE MATCH STILL IN PROGRESS')]):
    yy=i*235; draw.text((12,yy+8),title,fill=(240,231,216)); comparison.paste(im,(0,yy+28))
comparison.save(qa/'reference_previous_new.png')

# Rotation contact sheet and video-ready frames, composited only over a plain background.
sheet=Image.new('RGB',(960,6*170),(29,26,23)); draw=ImageDraw.Draw(sheet)
preview=folder/'preview_frames'; preview.mkdir(exist_ok=True)
for i,fr in enumerate(frames):
    bg=Image.new('RGBA',fr.size,(29,26,23,255)); bg.alpha_composite(fr)
    bg.convert('RGB').save(preview/f'{i:04d}.png')
for row,target in enumerate([-135,-90,-45,0,45,135]):
    i=min(range(n),key=lambda k:abs(angles[k]-target))
    fr=Image.open(preview/f'{i:04d}.png').resize((960,288),Image.Resampling.LANCZOS)
    # Fit all three true renders into each labelled strip, without stretching.
    fr.thumbnail((960,145),Image.Resampling.LANCZOS)
    sheet.paste(fr,((960-fr.width)//2,row*170+22)); draw.text((12,row*170+5),f'{angles[i]:g} degrees',fill='white')
sheet.save(qa/'rotation_contact_sheet.png')

# Temporal continuity: adjacent frames at 1.5 degrees, no silhouette clipping.
deltas=[]
for prev,cur in zip(frames,frames[1:]):
    p=np.asarray(prev).astype(float); c=np.asarray(cur).astype(float)
    mask=(p[...,3]>250)&(c[...,3]>250)
    deltas.append(float(np.abs(c[...,:3]-p[...,:3])[mask].mean()))
manifest['qa']={'camera_and_lights':'asserted stationary by renderer at every frame',
                'adjacent_rgb_mae_mean':float(np.mean(deltas)),
                'adjacent_rgb_mae_max':float(np.max(deltas)),
                'pixel_perfect_reference_match':False,
                'limitation':'Feel contour and shoulder lighting still visibly differ; full reference fidelity is not achieved.'}
(out/'atlas.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps(manifest['qa']))
