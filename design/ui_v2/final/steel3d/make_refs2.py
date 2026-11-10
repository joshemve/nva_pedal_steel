"""Rosewood refinement (Joshua 2026-11-10: perfect the rosewood style first; feed a real pedal steel photo so spacing
and sizing are realistic, with our own style). Image 1 = our screen with ref_rosewood_1; image 2 = his real photo."""
import json, os
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
P = ("Two input images. IMAGE 1 is the full UI of a photoreal pedal steel guitar audio plugin. IMAGE 2 is a photograph "
     "of a real pedal steel guitar; use it ONLY as the reference for true proportions, sizes and spacing. "
     "Output ONE image, landscape 1536x1024, identical to IMAGE 1 in framing, camera angle (front-on, looking down), "
     "lighting, the rug and room, the cream top bar and the whole dark bottom control deck with every knob, label and "
     "switch. Keep IMAGE 1's black fretboard with its coloured markers and numbers, the ten strings, the chrome tone "
     "bar and the pickup in exactly the same place and size. Redraw ONLY the instrument so that its proportions match "
     "the real one in IMAGE 2: the slim, shallow cabinet (length to depth and height as in the photo), the raised "
     "aluminium neck and its width against the cabinet, the size of the keyhead and its ten tuners against the neck, "
     "the changer and its size, the small endplates, the long thin chrome legs and their slight splay, the flat pedal "
     "bar on the floor between the front legs, the three small pedals on it and their spacing, the thin pull rods "
     "running up to the cabinet, the knee levers. Finish in our style: dark rosewood-grain cabinet with a satin "
     "sheen and one thin aluminium inlay line along the front apron, polished aluminium neck, keyhead, changer and "
     "endplates, real wear. Exactly ten strings, ten tuners (five each side), ten changer fingers. No logo, brand "
     "name, badge or text anywhere on the instrument. It must read as a real photographed instrument.")
jobs = [{"name": "rw_%d" % n, "images": ["final/steel3d/gen/ref_rosewood_1_1.png", "final/steel3d/real/real_rosewood_a.png"],
         "prompt": P} for n in (1, 2, 3, 4)]
json.dump(jobs, open("jobs_refs2.json", "w"), indent=1)
