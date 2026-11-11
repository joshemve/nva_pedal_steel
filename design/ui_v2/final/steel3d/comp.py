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


def grade(rgb):
    """the reference's crisp, deep look: a little local sharpening, deeper shadows, firmer contrast"""
    import cv2 as _cv
    x = rgb.astype(np.float32)
    x = x + 0.55 * (x - _cv.GaussianBlur(x, (0, 0), 1.3))
    x = np.clip(x, 0, 255) / 255.0
    x = x ** 1.12
    x = np.clip((x - 0.5) * 1.10 + 0.5 + 0.015, 0, 1)
    return x * 255.0
a = np.zeros((CH, CW), np.float32); a[sy0:sy1, sx0:sx1] = 1; a = cv2.GaussianBlur(a, (0, 0), 6)[..., None]
full = bg.copy(); full[max(y0, 0):y1, x0:x1] = room[max(y0, 0) - y0:, :]
base = full * a + bg * (1 - a)
st = np.asarray(Image.open(os.path.join(d, "steel.png")).convert("RGBA").resize((CW, CH), Image.LANCZOS), np.float32)
sh = np.clip(1 - st[..., :3].max(2, keepdims=True) / 6.0, 0, 1)        # pure shadow-catcher pixels (black, alpha = shadow)
st[..., :3] = grade(st[..., :3])
al = st[..., 3:] / 255 * (1 - 0.30 * sh) * a                            # soften the floor shadow to the reference's depth
out = st[..., :3] * al + base * (1 - al)
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).resize((1600, 1000), Image.LANCZOS).save(outp)
