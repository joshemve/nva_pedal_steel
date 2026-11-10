"""Composite the 3D steel render over the empty room plate inside the current UI background.
python comp.py <render_dir> <out.png> [room_job]"""
import json, os, sys
import numpy as np
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); P = os.path.join(H, "..", "parts")
CW, CH = 3200, 2000; RX, RY = CW / 1568, CH / 1003
d, outp = sys.argv[1], sys.argv[2]; job = sys.argv[3] if len(sys.argv) > 3 else "room_empty_1"
bg = np.asarray(Image.open(os.path.join(P, "bg_clean.png")).convert("RGB"), np.float32)
m = json.load(open(os.path.join(P, "aligned/%s.json" % job))); x0, y0, x1, y1 = m["canvas_box"]
room = np.asarray(Image.open(os.path.join(P, "aligned/%s.png" % job)).convert("RGB"), np.float32)
sx0, sy0, sx1, sy1 = round(96 * RX), round(104 * RY), round(1494 * RX), round(668 * RY)   # the steel's room, never bar/deck
import cv2
a = np.zeros((CH, CW), np.float32); a[sy0:sy1, sx0:sx1] = 1; a = cv2.GaussianBlur(a, (0, 0), 6)[..., None]
full = bg.copy(); full[max(y0, 0):y1, x0:x1] = room[max(y0, 0) - y0:, :]
base = full * a + bg * (1 - a)
st = np.asarray(Image.open(os.path.join(d, "steel.png")).convert("RGBA").resize((CW, CH), Image.LANCZOS), np.float32)
sh = np.clip(1 - st[..., :3].max(2, keepdims=True) / 6.0, 0, 1)        # pure shadow-catcher pixels (black, alpha = shadow)
al = st[..., 3:] / 255 * (1 - 0.30 * sh) * a                            # soften the floor shadow to the reference's depth
out = st[..., :3] * al + base * (1 - al)
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).resize((1600, 1000), Image.LANCZOS).save(outp)
