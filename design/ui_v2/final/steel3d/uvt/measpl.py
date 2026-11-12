import sys, colorsys
import numpy as np
from PIL import Image
def st(p, box):
    a = np.asarray(Image.open(p).convert("RGB").resize((1200, 750)).crop(box)).reshape(-1, 3).astype(float)
    hsv = np.array([colorsys.rgb_to_hsv(*(c / 255)) for c in a[::5]])
    v = hsv[:, 2] * 100
    return "H%.0f S%.0f V p10/50/90 %.0f/%.0f/%.0f RGB %s" % (np.median(hsv[:, 0]) * 360, np.median(hsv[:, 1]) * 100, *np.percentile(v, [10, 50, 90]), a.mean(0).round().astype(int))
R = "steel3d/gen/p3d_pearloid_4_1.png"; O = sys.argv[1] if len(sys.argv) > 1 else "parts/skins/_full_pearloid.jpg"
print("ref top  ", st(R, (250, 228, 1000, 243))); print("ours top ", st(O, (250, 234, 1000, 246)))
print("ref front", st(R, (250, 258, 1000, 298))); print("ours frnt", st(O, (250, 258, 1000, 298)))
