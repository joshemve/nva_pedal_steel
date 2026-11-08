"""Cream domes without their pointer line (the body is rotationally symmetric; the plugin rotates only the pointer)."""
import json, os
import make_jobs as MJ
os.chdir(os.path.dirname(os.path.abspath(__file__)))
jobs = []
for name, part in (("noptr_TONE", "knob_TONE"), ("noptr_PICK", "knob_PICK"), ("noptr_TRIM", "trim")):
    jobs.append({"name": name, "images": ["final/parts/crops/%s.png" % part], "prompt": MJ.SAME +
        "erase ONLY the short straight pointer line (indicator stripe) painted on the top of the cream knob in the centre, "
        "so the cream domed top is plain and smooth there, with the same shading and highlights it has around the line. "
        "The knob, its black knurled skirt, the tick marks and everything else stay exactly the same." + MJ.BG})
J = json.load(open("jobs.json")); names = {j["name"] for j in jobs}
json.dump([j for j in J if j["name"] not in names] + jobs, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs4.json", "w"), indent=1)
