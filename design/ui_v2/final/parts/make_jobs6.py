"""Knee levers engaged (LKL, LKR) and the MIDI LEARN button active. Two takes each."""
import json, os
import make_jobs as MJ
from PIL import Image
os.chdir(os.path.dirname(os.path.abspath(__file__)))
PARTS = {"levers": (1250, 712, 1492, 868), "midibtn": (1326, 906, 1490, 956)}
C = json.load(open("crops.json"))
for k, b in PARTS.items():
    cb = MJ.crop32(b); C[k] = {"crop": cb, "part": list(b)}
    MJ.REF.crop(tuple(cb)).resize((1536, 1024), Image.LANCZOS).save("crops/%s.png" % k)
json.dump(C, open("crops.json", "w"), indent=1)
LEV = ("engage BOTH knee levers (LKL and LKR): each lever's black ribbed pad is pressed in and down, sitting a little "
       "lower, with its chrome stalk tilted a little further, and a warm amber glow showing under the top edge of each pad, "
       "exactly the glow the pressed, lit A and B foot pedals have in this plugin. Pivot rings, labels LKL/LKR and the "
       "panel stay the same.")
MID = ("the MIDI LEARN button is switched ON: its thin outline and the words MIDI LEARN glow warm amber (the same amber as "
       "lit LED segments), with a very soft amber glow just around the outline on the black panel. Same font, size, "
       "position and box shape.")
jobs = []
for n in (1, 2):
    jobs.append({"name": "levers_on_%d" % n, "images": ["final/parts/crops/levers.png"], "prompt": MJ.SAME + LEV + MJ.BG})
    jobs.append({"name": "midi_on_%d" % n, "images": ["final/parts/crops/midibtn.png"], "prompt": MJ.SAME + MID + MJ.BG})
J = json.load(open("jobs.json")); names = {j["name"] for j in jobs}
json.dump([j for j in J if j["name"] not in names] + jobs, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs6.json", "w"), indent=1)
print(C["levers"], C["midibtn"])
