"""Clean deck plate for the Blender deck controls (Joshua: "do all" - the A/B/C deck pedals and LKL/LKR knee-lever
toggles as real 3D models). One ChatGPT redraw of the deck around those controls with them removed: plain leather
panel where they were; their LEDs and letters stay. Only the controls' boxes are pasted back (deck_3d.py clean)."""
import json, os, sys
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
sys.path.insert(0, os.path.join(H, "..", "parts"))
BOX = (1920, 1220, 3080, 1993)                     # canvas, 3:2
bg = Image.open("../parts/bg_clean.png").convert("RGB")
bg.crop(BOX).resize((1536, 1024), Image.LANCZOS).save("deck_crop.png")
json.dump({"box": BOX}, open("deck_crop.json", "w"))
SAME = ("This is an upscaled crop of a photoreal audio-plugin UI. Output ONE image, landscape 1536x1024, with EXACTLY "
        "the same framing, scale, perspective, lighting, colours and every detail as the input, so it overlays the input "
        "pixel for pixel. Change ONLY this: ")
p = (SAME + "remove the three black-rubber foot-switch pedals labelled A, B and C (their chrome frames and their shadows) "
     "and the two chrome knee-lever toggles labelled LKL and LKR (the chrome ring mounts, the angled chrome rods and the "
     "small ribbed black paddles, and their shadows). Where they were, show only the same plain dark brown-black leather "
     "panel texture, continuing naturally and evenly with the same grain, sheen and soft lighting. Keep the letters A, B, "
     "C, LKL, LKR, the small round lamp lenses above A, B and C, the thin vertical divider line, the knobs, the slider "
     "and everything else exactly as they are. Everything outside that change must stay identical to the input.")
jobs = [{"name": "deckclean_%d" % n, "images": ["final/deck3d/deck_crop.png"], "prompt": p} for n in (1, 2)]
json.dump(jobs, open("jobs_deckclean.json", "w"), indent=1); print(BOX)
