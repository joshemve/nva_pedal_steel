"""The steel itself: tone bar removed (bar is drawn live), floor pedals pressed, under-neck knee levers pushed."""
import json, os
import make_jobs as MJ
from PIL import Image
os.chdir(os.path.dirname(os.path.abspath(__file__)))
PARTS = {"bar": (740, 145, 870, 295), "floorpedals": (620, 555, 885, 655), "kl_left": (515, 400, 580, 510),
         "kl_right": (850, 400, 915, 510)}
C = json.load(open("crops.json"))
for k, b in PARTS.items():
    cb = MJ.crop32(b, margin=0.35); C[k] = {"crop": cb, "part": list(b)}
    MJ.REF.crop(tuple(cb)).resize((1536, 1024), Image.LANCZOS).save("crops/%s.png" % k)
json.dump(C, open("crops.json", "w"), indent=1)
CH = {
 "bar_removed": ("bar", "remove the chrome cylindrical tone bar (the slide bar lying across the strings in the middle of the "
   "black fretboard) and its reflection/shadow completely. The strings, fret lines, fret dots/diamonds, printed fret numbers "
   "and wood continue naturally and evenly through where the bar was."),
 "floor_pressed": ("floorpedals", "the three black foot pedals (rubber-topped pedal blocks on the chrome cross bar under the "
   "steel) are all pressed DOWN: each pedal's top is tilted forward and sits lower, showing less of its front face, and its "
   "thin vertical pull rod above is pulled a little further down. Cross bar, legs, carpet and lighting unchanged."),
 "kl_left_pushed": ("kl_left", "the small black knee-lever block hanging on the thin chrome rod under the steel's front rail is "
   "pushed sideways to the LEFT by the player's knee: the block and the lower end of its rod swing about 8 degrees to the "
   "left (rod pivots at the rail). Rail, carpet and lighting unchanged."),
 "kl_right_pushed": ("kl_right", "the small black knee-lever block hanging on the thin chrome rod under the steel's front rail "
   "is pushed sideways to the RIGHT by the player's knee: the block and the lower end of its rod swing about 8 degrees to the "
   "right (rod pivots at the rail). Rail, carpet and lighting unchanged."),
}
jobs = []
for name, (part, ch) in CH.items():
    for n in (1, 2):
        jobs.append({"name": "%s_%d" % (name, n), "images": ["final/parts/crops/%s.png" % part], "prompt": MJ.SAME + ch + MJ.BG})
J = json.load(open("jobs.json")); names = {j["name"] for j in jobs}
json.dump([j for j in J if j["name"] not in names] + jobs, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs7.json", "w"), indent=1)
print({k: C[k]["crop"] for k in PARTS})
