# Native rotating knob study — 2026-11-06

Built in Blender 5.0.1/Cycles from editable geometry, not a rotating screenshot.

## Deliverables

- `knob_studio.blend`: native scene with the three knob families, fixed studio lights, and a keyed -135 to +135 degree sweep (frames 0–180).
- `rotation_preview.mp4`: forward/reverse playback of the actual renders.
- `assets/knob_*_atlas.png`: transparent, tiled 2x assets, 181 frames per family.
- `assets/atlas.json`: exact frame sizes, pivots, tile coordinates, angular samples and QA results.
- `qa/reference_previous_new.png`: approved reference, prior export, and this native export assembled on the existing UI plate.
- `qa/rotation_contact_sheet.png`: six angles for visual comparison.

The lights, camera and material definitions remain stationary. Only the knob's Spin node rotates, including the body, scallops and pointer. Specular highlights are computed afresh by Cycles at each angle. No reference pixels, image-space colour-ratio correction or rotating baked illumination are applied to the exported frames.

## Validation and remaining work

- Camera/light matrices were asserted stationary for all 181 frames.
- All exported sprite borders have alpha below 3/255: no clipped knob silhouettes.
- Adjacent-frame RGB change on opaque overlapping pixels: mean 1.865/255; maximum 1.902/255. This checks gross discontinuities, not subjective realism.
- Inspected the assembled comparison and six-angle contact sheet.
- **This is not a pixel-perfect reference match.** The Feel contour and shoulder highlights still differ. The other families also retain visible differences. Do not mark reference matching complete based on centre-colour measurements.
- Transparent atlases contain the body only. The assembled UI uses the existing panel's contact shadows. The optional shadow-catcher experiment was preserved separately, but not exported because its shadow reached the frame boundary.
- Existing production strips and the legacy renderer remain unchanged. These assets are a separate native candidate, not an unverified replacement in the JUCE plugin.

## Reproduce

From the repository root in PowerShell:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --factory-startup -b -P design/ui_v2/blender/knob_studio.py -- design/ui_v2/final/knobs/studio_rotation design/ui_v2/final/knobs/studio_rotation.json
python design/ui_v2/final/knobs/package_studio.py design/ui_v2/final/knobs/studio_rotation
```

Geometry lives in `blender/knob_studio_geometry.py`; the independent rendering rig is `blender/knob_studio.py`. The exact accepted-for-this-study parameters are in `final/knobs/studio_rotation.json`. Earlier `studio_v*` experiments are retained for comparison; none is user-approved.
