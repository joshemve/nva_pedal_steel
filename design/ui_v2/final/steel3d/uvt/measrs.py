import sys, colorsys
import numpy as np
from PIL import Image
def st(p, box):
    a = np.asarray(Image.open(p).convert("RGB").resize((1200, 750)).crop(box)).reshape(-1, 3).astype(float)
    hsv = np.array([colorsys.rgb_to_hsv(*(c / 255)) for c in a[::7]])
    v = hsv[:, 2] * 100
    return "S%.0f V p10/50/90/99 %.0f/%.0f/%.0f/%.0f RGB %s" % (np.median(hsv[:, 1]) * 100, *np.percentile(v, [10, 50, 90, 99]), a.mean(0).round().astype(int))
R = "steel3d/gen/p3d_redsparkle_1_1.png"
O = sys.argv[1] if len(sys.argv) > 1 else "parts/skins/_full_redsparkle.jpg"
print("ref top  ", st(R, (300, 210, 900, 235))); print("ours top ", st(O, (300, 228, 900, 243)))
print("ref front", st(R, (300, 242, 900, 258))); print("ours frnt", st(O, (300, 256, 900, 272)))
print("ref low  ", st(R, (300, 268, 900, 285))); print("ours low ", st(O, (300, 285, 900, 298)))
