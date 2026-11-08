"""Empty TRIM readout (the 0.0 dB text changes with the knob)."""
import json, os
from PIL import Image
import make_jobs as MJ
os.chdir(os.path.dirname(os.path.abspath(__file__)))
boxes = json.load(open("crops.json")) if os.path.exists("crops.json") else {}
part = "trimdb"; pb = (1494, 40, 1560, 70)
b = MJ.crop32(pb, margin=0.4, minw=200); boxes[part] = {"crop": b, "part": list(pb)}
MJ.REF.crop(tuple(b)).resize((1536, 1024), Image.LANCZOS).save("crops/%s.png" % part)
jobs = [{"name": "trimdb_empty", "images": ["final/parts/crops/%s.png" % part], "prompt": MJ.SAME +
         "remove the text '0.0 dB' to the right of the TRIM knob, leaving the plain cream top-bar surface there." + MJ.BG}]
json.dump(boxes, open("crops.json", "w"), indent=1)
J = json.load(open("jobs.json")); J = [j for j in J if j["name"] != "trimdb_empty"] + jobs
json.dump(J, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs3.json", "w"), indent=1)
