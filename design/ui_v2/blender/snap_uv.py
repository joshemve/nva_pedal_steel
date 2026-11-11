"""Fit each panel ChatGPT painted back into its exact UV island (it keeps the layout but can stretch panels a little).
python snap_uv.py <in.png> <out.png>"""
import sys
import numpy as np
from PIL import Image
import cabinet_uv as cu

src = Image.open(sys.argv[1]).convert("RGB").resize((cu.W, cu.H), Image.LANCZOS)
a = np.asarray(src.convert("L"), np.float32); out = Image.new("RGB", (cu.W, cu.H), (0, 0, 0))
for name, box in cu.ISLANDS.items():
    x0, y0, x1, y1 = cu.px(box)
    # search a little beyond the island for where the painted panel really starts and ends
    sx0, sy0, sx1, sy1 = max(0, x0 - 40), max(0, y0 - 8), min(cu.W, x1 + 90), min(cu.H, y1 + 8)
    m = a[sy0:sy1, sx0:sx1] > 12
    rows = np.where(m.mean(1) > 0.5)[0]; cols = np.where(m[len(m) // 2 - 3: len(m) // 2 + 3].mean(0) > 0.5)[0]
    bx0, bx1 = sx0 + cols.min(), sx0 + cols.max() + 1; by0, by1 = sy0 + rows.min(), sy0 + rows.max() + 1
    out.paste(src.crop((bx0 + 2, by0 + 2, bx1 - 2, by1 - 2)).resize((x1 - x0, y1 - y0), Image.LANCZOS), (x0, y0))
    print(name, "painted", (bx0, by0, bx1, by1), "-> island", (x0, y0, x1, y1))
out.save(sys.argv[2])
