"""Knob-strip plate: the reference (HD) everywhere except inside the area some frame of the knob covers (union of the
Blender silhouettes over the whole sweep), where the ChatGPT clean plate shows. So halos and drop shadows around the
knobs are the reference's own pixels; between lobes the strips' rim layer paints the rest. -> strips/plate_walnut.png"""
import json, os, glob
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
FR = "../strips/frames"; meta = json.load(open("../strips/strips.json"))
hd = np.asarray(Image.open("bg_hd.png"), np.float32); clean = np.asarray(Image.open("bg_clean.png"), np.float32)
mask = np.zeros(hd.shape[:2], np.float32)
for style in ("dome", "scallop", "space"):
    fs = sorted(glob.glob(os.path.join(FR, style + "_normal_[0-9][0-9][0-9][0-9].png")))
    u = None
    for f in fs[::2]:
        a = np.asarray(Image.open(f))[..., 3] > 8
        u = a if u is None else (u | a)
    u = cv2.dilate(u.astype(np.uint8), np.ones((5, 5), np.uint8)).astype(np.float32)   # +2 px margin
    u = cv2.GaussianBlur(u, (0, 0), 1.5)
    m = meta[style]; sc = m["scale"]; n = u.shape[0]
    for inst in m["instances"]:
        x0 = round(inst["cx"] * sc - n / 2); y0 = round(inst["cy"] * sc - n / 2)
        mask[y0:y0 + n, x0:x0 + n] = np.maximum(mask[y0:y0 + n, x0:x0 + n], u)
plate = clean * mask[..., None] + hd * (1 - mask[..., None])
Image.fromarray(plate.astype(np.uint8)).save("../strips/plate_walnut.png")
print("plate: clean inside knob sweep areas (%.1f%% of canvas)" % (100 * (mask > 0.5).mean()))
