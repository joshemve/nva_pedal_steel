"""Slider groove states: completely empty and completely filled (the fill follows the value)."""
import json, os
import make_jobs as MJ
os.chdir(os.path.dirname(os.path.abspath(__file__)))
jobs = [{"name": "groove_empty", "images": ["final/parts/crops/slider.png"], "prompt": MJ.SAME +
         "remove the cream slider handle AND the warm cream/brass fill inside the VOLUME PEDAL slider groove, so the whole "
         "groove from its left end to its right end is empty and dark exactly like its unfilled right-hand part. Tick marks "
         "below and the groove outline stay the same." + MJ.BG},
        {"name": "groove_full", "images": ["final/parts/crops/slider.png"], "prompt": MJ.SAME +
         "remove the cream slider handle and make the warm cream/brass fill inside the VOLUME PEDAL slider groove run evenly "
         "along the WHOLE groove from its left end to its right end (fully filled, same colour and sheen as its left part). "
         "Tick marks below and the groove outline stay the same." + MJ.BG}]
J = json.load(open("jobs.json")); names = {j["name"] for j in jobs}
json.dump([j for j in J if j["name"] not in names] + jobs, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs5.json", "w"), indent=1)
