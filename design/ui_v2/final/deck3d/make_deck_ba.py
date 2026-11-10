"""Deck before/after (Joshua 2026-11-09: "send the whole reference into chatgpt ... the lkr pedal needs a before and
after solution, with proper spacing and keeping everything else the same"). Step 'before': whole approved screen in,
A/B/C + LKL/LKR respaced, all at rest. Step 'after': each before image in, same image with the controls engaged."""
import json, os, sys, glob
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
KEEP = ("This is the full approved UI of a photoreal pedal steel guitar audio plugin. Keep proportions and everything "
        "the same: output ONE image, landscape 1536x1024, identical to the input in framing, layout, scale, lighting, "
        "colours, the steel guitar, the rug, the top bar, every knob, label, display and the bottom strip. ")
BEFORE = (KEEP + "Change ONLY the A, B, C foot-switch pedals and the LKL, LKR knee-lever controls in the bottom deck: "
          "give them proper spacing. A, B and C are the same size, evenly spaced with real gaps between them, each "
          "centred under its letter and lamp, the group centred in its section. LKL and LKR are mirror images of each "
          "other (LKL's paddle toward the left, LKR's toward the right), each centred under its label, the pair centred "
          "in its section. This is the BEFORE position: all five at rest, all three lamps unlit. Keep their existing "
          "style (black ribbed rubber, chrome), just spaced and mirrored properly.")
AFTER = (KEEP + "This image shows the A, B, C pedals and LKL, LKR knee levers at rest (BEFORE). Now make the AFTER "
         "position: the same exact pedals and levers, same places and same spacing, but ENGAGED: A, B and C pressed "
         "down (tipped forward into the panel, the rubber pad lower with a tighter shadow) with their lamps lit warm "
         "amber; LKL swung outward to the left and LKR swung outward to the right on their pivots. No glow spilling "
         "on the panel. Nothing else changes.")
step = sys.argv[1]
if step == "before":
    jobs = [{"name": "before_%d" % n, "images": ["APPROVED_main.png"], "prompt": BEFORE} for n in (1, 2, 3, 4)]
else:
    jobs = [{"name": os.path.basename(f)[:-4].replace("before", "after"), "images": ["final/deck3d/ba/gen/" + os.path.basename(f)],
             "prompt": AFTER} for f in sorted(glob.glob("ba/gen/before_*.png"))]
json.dump(jobs, open("ba/jobs_%s.json" % step, "w"), indent=1); print(len(jobs))
