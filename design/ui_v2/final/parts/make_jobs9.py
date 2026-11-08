"""small_1 (small-window variant: bolder, larger labels and values): empty value boxes / mode box / % box / dB readout,
made from small_1's own crops (same framing as the reference), for the small-window label layer (small_labels.py)."""
import json, os
import make_jobs as MJ
from PIL import Image
os.chdir(os.path.dirname(os.path.abspath(__file__)))
SM = Image.open("../../round18/small_1.png").convert("RGB")
C = json.load(open("crops.json"))
parts = {"pills": "remove the value text inside every outlined value box (65 %, 40 %, 145 ms, 35 %, 18 %, 28 %): the outlined "
                  "boxes stay, empty, with the same black panel inside. The labels TONE, PICK, GLIDE, SWELL, ECHO, REVERB stay exactly.",
         "modebox": "remove the text 'Smart Auto-Pedal E9' inside the MODE dropdown box; keep the box outline and the down-arrow triangle.",
         "pctbox": "remove the text '72 %' inside the small outlined box next to the slider; keep the box.",
         "trim": "remove only the '0.0 dB' text right of the TRIM knob (plain cream bar where it was); the knob, the TRIM label and everything else stay."}
jobs = []
for part, change in parts.items():
    cb = C[part]["crop"]; SM.crop(tuple(cb)).resize((1536, 1024), Image.LANCZOS).save("crops/small_%s.png" % part)
    for n in (1, 2):
        jobs.append({"name": "small_%s_empty_%d" % (part, n), "images": ["final/parts/crops/small_%s.png" % part], "prompt": MJ.SAME + change + MJ.BG})
json.dump(jobs, open("jobs9.json", "w"), indent=1); print(len(jobs), "jobs")
