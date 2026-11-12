"""Paint a concept's gloss highlights into a mask on the cabinet's UV layout.
The steel is always seen from the same camera, so highlights baked where the concept shows them read as real gloss.
Positions were read off the concepts at the plugin framing (cabinet spans image x 130..1035 of 1200; a = 0 keyhead end).
  redsparkle (p3d_redsparkle_1_1): a thin hot line right on the front edge along the whole length, hottest at the keyhead
    end, middle-right and changer end, plus soft glows in the middle of the top strip (Joshua 2026-11-11, circled twice).
  pearloid (p3d_pearloid_4_1): broad glossy reflections over the top strip, mostly the left half, and the front edge.
python highlight_mask.py <finish> <out.png>"""
import sys
import numpy as np
from PIL import Image, ImageFilter
import cabinet_uv as cu

W, H = cu.W, cu.H
mask = np.zeros((H, W), np.float32)


def blob(face, a, b, ra, rb, k=1.0, sharp=2.5):
    """soft elliptical spot centred at panel coords (a, b) in 0..1, radii ra, rb in panel fractions"""
    x0, y0, x1, y1 = cu.px(cu.ISLANDS[face])
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    u = (xx - x0) / (x1 - x0); v = 1 - (yy - y0) / (y1 - y0)
    d = ((u - a) / ra) ** 2 + ((v - b) / rb) ** 2
    mask[y0:y1, x0:x1] = np.maximum(mask[y0:y1, x0:x1], k * np.exp(-d * sharp))


def edge_line(face, b, rb, base, spots):
    """a line at panel height b running the full length; brightness = base + gaussian hot spots (a, ra, k)"""
    x0, y0, x1, y1 = cu.px(cu.ISLANDS[face])
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    u = (xx - x0) / (x1 - x0); v = 1 - (yy - y0) / (y1 - y0)
    along = np.full_like(u, base)
    for a, ra, k in spots:
        along = np.maximum(along, k * np.exp(-((u - a) / ra) ** 2 * 2.5))
    mask[y0:y1, x0:x1] = np.maximum(mask[y0:y1, x0:x1], along * np.exp(-((v - b) / rb) ** 2 * 2.5))


finish = sys.argv[1]
if finish == "redsparkle":
    hot = ((0.10, 0.10, 1.0), (0.60, 0.07, 0.9), (0.93, 0.05, 1.0), (0.36, 0.05, 0.55))
    edge_line("TOP", 0.99, 0.035, 0.85, hot)          # the rounded front edge seen from above...
    edge_line("FRONT", 1.0, 0.06, 0.85, hot)        # ...and from the front: together one thin line on the corner
    edge_line("TOP", 0.93, 0.06, 0.38, ((0.15, 0.2, 0.6), (0.6, 0.15, 0.55), (0.93, 0.08, 0.6)))   # broad red sheen over the top strip
    for a, ra, k in ((0.15, 0.06, 0.9), (0.60, 0.05, 0.85), (0.94, 0.04, 0.8)):
        blob("TOP", a, 0.93, ra, 0.03, k)           # red glows in the middle of the top strip
    blob("END_LEFT", 0.12, 0.75, 0.20, 0.30, 0.8)
    blob("END_RIGHT", 0.88, 0.75, 0.20, 0.30, 0.8)
if finish == "pearloid":
    for a, ra, k in ((0.16, 0.13, 0.7), (0.42, 0.07, 0.5), (0.70, 0.09, 0.45)):   # none at the changer end: it washed out (Joshua)
        blob("TOP", a, 0.93, ra, 0.05, k, 1.5)       # broad gloss over the visible top strip
    edge_line("TOP", 0.99, 0.035, 0.5, ((0.2, 0.15, 1.0), (0.75, 0.1, 0.8)))
    edge_line("FRONT", 1.0, 0.10, 0.5, ((0.2, 0.15, 1.0), (0.75, 0.1, 0.8)))
if finish == "pearloid":                             # plus the red's glare spots on the top strip (Joshua liked them)
    for a, ra, k in ((0.15, 0.06, 0.8), (0.60, 0.05, 0.7)):
        blob("TOP", a, 0.93, ra, 0.03, k)
if finish in ("koa", "maple", "rosewood"):         # the red sparkle's gloss on the wood cabinets (Joshua 2026-11-11)
    hot = ((0.10, 0.10, 1.0), (0.60, 0.07, 0.9), (0.93, 0.05, 1.0), (0.36, 0.05, 0.55))
    edge_line("FRONT", 1.0, 0.06, 0.7, hot)
    for a, ra, k in ((0.15, 0.06, 0.9), (0.60, 0.05, 0.85), (0.94, 0.04, 0.8)):
        blob("TOP", a, 0.93, ra, 0.03, k)
elif finish not in ("redsparkle", "pearloid"):
    sys.exit("no highlight layout for " + finish)

im = Image.fromarray((np.clip(mask, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5))
im.save(sys.argv[2]); print("saved", sys.argv[2])
