"""HD tiles of the whole reference (4x4, 3:2, overlapping) + one empty-pill job per value box."""
import json, os
from PIL import Image
import make_jobs as MJ
os.chdir(os.path.dirname(os.path.abspath(__file__)))
REF = MJ.REF; boxes = json.load(open("crops.json")); jobs = []
W, Hh = 560, 373.33
for r in range(4):
    for c in range(4):
        x0 = c * (REF.width - W) / 3; y0 = r * (REF.height - Hh) / 3
        part = "tile_%d%d" % (r, c); b = [round(x0, 2), round(y0, 2), round(x0 + W, 2), round(y0 + Hh, 2)]
        boxes[part] = {"crop": b, "part": b}
        REF.crop(tuple(b)).resize((1536, 1024), Image.LANCZOS).save("crops/%s.png" % part)
        jobs.append({"name": "hd_" + part, "images": ["final/parts/crops/%s.png" % part], "prompt": MJ.SAME +
            "nothing: reproduce it as a sharper, higher-detail version of exactly the same picture (crisper edges, text, "
            "strings, frets, chrome reflections, wood grain, rug weave and panel texture), with every glare, highlight, "
            "shadow, letter and object in exactly the same place, shape and colour. Do not add, remove or restyle anything." + MJ.BG})
PILLS = {"TONE": (134, 884, 74), "PICK": (268.5, 884, 74), "GLIDE": (434, 884, 86), "SWELL": (575, 884, 74),
         "ECHO": (735, 884, 74), "REVERB": (865, 884, 74)}
for k, (x, y, w) in PILLS.items():
    part = "pill_" + k; pb = (x - w / 2, y - 15, x + w / 2, y + 15)
    b = MJ.crop32(pb, margin=0.4, minw=200); boxes[part] = {"crop": b, "part": [round(v, 2) for v in pb]}
    REF.crop(tuple(b)).resize((1536, 1024), Image.LANCZOS).save("crops/%s.png" % part)
    jobs.append({"name": "pillempty_" + k, "images": ["final/parts/crops/%s.png" % part], "prompt": MJ.SAME +
        "remove the value text (the number and unit) inside the outlined value box in the centre, leaving the outlined box "
        "empty with the same black panel inside it. The label above it stays." + MJ.BG})
json.dump(boxes, open("crops.json", "w"), indent=1)
J = json.load(open("jobs.json")); J = [j for j in J if not j["name"].startswith(("hd_", "pillempty_"))] + jobs
json.dump(J, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs2.json", "w"), indent=1)
print(len(jobs), "new jobs")
