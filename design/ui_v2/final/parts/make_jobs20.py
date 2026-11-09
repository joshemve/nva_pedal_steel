"""Floor plate for the 3D pedal assembly (Joshua: "carpet bending or textures patchy around pedals, make sure it's real
blender 3d models"). One ChatGPT redraw of the floor under the steel with the whole pedal assembly removed (three floor
pedals, their pull rods, the cross bar, the two hanging knee-lever paddles): one continuous rug, nothing else changed.
The Blender-rendered assembly (pedals_3d.py) is drawn over it in every state, so the rug never changes."""
import json, os
import make_jobs as MJ
from PIL import Image
os.chdir(os.path.dirname(os.path.abspath(__file__)))
C = json.load(open("crops.json"))
box = (480, 395, 1000, 672); cb = MJ.crop32(box, margin=0.04)
C["floor"] = {"crop": cb, "part": list(box)}; json.dump(C, open("crops.json", "w"), indent=1)
MJ.REF.crop(tuple(cb)).resize((1536, 1024), Image.LANCZOS).save("crops/floor.png")
p = (MJ.SAME + "remove the whole foot-pedal assembly under the instrument: the three black-rubber foot pedals, the long "
     "chrome horizontal cross bar they sit on, the thin vertical chrome pull rods going up from the pedals, and the two "
     "small black hanging knee-lever paddles with their chrome rods. Where they were, show only the same ornate red, "
     "rust and black patterned rug, its pattern continuing naturally and evenly, at the same scale, focus and lighting, "
     "with a very soft, faint shadow under the instrument body. The instrument's wooden body edge at the top and its "
     "chrome legs stay exactly as they are." + MJ.BG)
jobs = [{"name": "floorclean_%d" % n, "images": ["final/parts/crops/floor.png"], "prompt": p} for n in (1, 2)]
J = json.load(open("jobs.json")); names = {j["name"] for j in jobs}
json.dump([j for j in J if j["name"] not in names] + jobs, open("jobs.json", "w"), indent=1)
json.dump(jobs, open("jobs20.json", "w"), indent=1)
print(cb, len(jobs), "jobs")
