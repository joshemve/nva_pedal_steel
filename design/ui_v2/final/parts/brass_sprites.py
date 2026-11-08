"""Brass versions of the steel's moving chrome (floor pedals pressed, knee levers pushed) for the koa instrument, whose
rods and cross bar are brass: chrome-looking pixels (bright, colourless) get the koa steel's brass colour; rug, rubber and
everything else stay. Appends *_brass entries to sprites/sprites.json. Run after build_ui.py (and small_labels.py)."""
import json, os
import numpy as np
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
S = json.load(open("sprites/sprites.json"))
# brass colour measured on the koa skin's rods would be ideal; this matches the brass there closely
BRASS = np.array([1.08, 0.84, 0.50], np.float32)
for k in ("floor_A", "floor_B", "floor_C", "kl_left_pushed", "kl_right_pushed"):
    sp = np.asarray(Image.open("sprites/%s.png" % k), np.float32); rgb = sp[..., :3]
    L = rgb.mean(2); sat = rgb.max(2) - rgb.min(2)
    chrome = np.clip((80.0 - sat) / 35.0, 0, 1) * np.clip((L - 60.0) / 40.0, 0, 1)
    brass = np.clip(L[..., None] * BRASS * 1.12, 0, 255)
    sp[..., :3] = brass * chrome[..., None] + rgb * (1 - chrome[..., None])
    Image.fromarray(sp.astype(np.uint8), "RGBA").save("sprites/%s_brass.png" % k)
    S[k + "_brass"] = dict(S[k])
json.dump(S, open("sprites/sprites.json", "w"), indent=1); print("brass sprites written")
