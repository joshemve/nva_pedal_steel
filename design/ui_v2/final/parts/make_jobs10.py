"""Per-preset instruments (Joshua: 'whatever's best' -> 04 i04_2 maple, 06 i06_1 red sparkle, 13 i13_2 pearloid,
18 i18_1 koa, from round21). Each is put onto the APPROVED screen's steel crop: same camera, room and every playable
part in place (neck, strings, bar, keyhead, changer, pickup, pedals, rods, knee levers); only the body changes."""
import json, os
import make_jobs as MJ
from PIL import Image
os.chdir(os.path.dirname(os.path.abspath(__file__)))
C = json.load(open("crops.json"))
box = (100, 100, 1490, 690); cb = MJ.crop32(box, margin=0.01)
C["steelbody"] = {"crop": cb, "part": list(box)}; json.dump(C, open("crops.json", "w"), indent=1)
MJ.REF.crop(tuple(cb)).resize((1536, 1024), Image.LANCZOS).save("crops/steelbody.png")
SK = {"maple": ("i04_2", "figured birdseye maple (warm natural blond, matte satin lacquer)"),
      "redsparkle": ("i06_1", "deep red metal-flake sparkle finish with polished chrome trim rails along the front"),
      "pearloid": ("i13_2", "white mother-of-pearl pearloid covering with chrome trim"),
      "koa": ("i18_1", "highly figured Hawaiian koa wood (golden-brown curl), with brass-toned changer and keyhead hardware")}
jobs = []
for sk, (ref, finish) in SK.items():
    p = ("Image 1 is a crop of a photoreal audio-plugin UI showing a pedal steel guitar. Image 2 shows another pedal steel "
         "design. Output ONE image, landscape 1536x1024, that is image 1 with ONLY the instrument's BODY (cabinet, apron, end "
         "caps, trim and legs) restyled after image 2: %s, with image 2's body shape, end caps and legs. EXACTLY the same "
         "camera, framing, scale, perspective, room, rug and lighting as image 1, so it overlays image 1 pixel for pixel. "
         "These parts stay exactly where they are and look the same as in image 1: the black fretboard with its white fret "
         "lines, numbers and coloured markers, the ten strings, the chrome tone bar, the keyhead with ten keys, the changer, "
         "the pickup, the three foot pedals on their cross bar, the pull rods and the two knee levers. The body's top edge, "
         "front rail and outline stay where image 1 has them. No text, letters, badges or nameplates anywhere. It must look "
         "like a real photographed instrument with honest wear." % finish)
    for n in (1, 2):
        jobs.append({"name": "skin_%s_%d" % (sk, n), "images": ["final/parts/crops/steelbody.png", "round21/%s.png" % ref], "prompt": p})
J = json.load(open("jobs.json")); names = {j["name"] for j in jobs}
json.dump([j for j in J if j["name"] not in names] + jobs, open("jobs.json", "w"), indent=1); json.dump(jobs, open("jobs10.json", "w"), indent=1)
print(cb, len(jobs), "jobs")
