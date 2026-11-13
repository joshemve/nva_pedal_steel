"""SPACE tick rings redrawn even (Joshua 2026-11-12: "these ticks are off"). The AI-painted background had extra short
ticks crowded into both upper diagonals of ECHO and REVERB. Each ring is erased and redrawn as 13 ticks every 22.5 deg
from -135 to +135, every one a rotated copy of that ring's own top tick, so they keep the painted look.
apply(bg) -> bg (float RGB canvas, 3200x2000)."""
import numpy as np, cv2

RINGS = [(1491.0, 1555.3), (1751.5, 1555.3)]   # ECHO, REVERB tick-ring centres (canvas), fitted to the ticks
R_IN, R_OUT = 85.0, 100.0                       # tick extent
ANGLES = [-135 + 22.5 * i for i in range(13)]


def apply(bg):
    out = bg.copy()
    for cx, cy in RINGS:
        x0, y0, S = int(cx) - 120, int(cy) - 120, 240
        reg = out[y0:y0 + S, x0:x0 + S]
        lum = reg.mean(2)
        yy, xx = np.mgrid[0:S, 0:S].astype(np.float32); ccx, ccy = cx - x0, cy - y0
        rr = np.hypot(xx - ccx, yy - ccy)
        # the top tick as a template: alpha from how far above the panel it is, colour = its own cream
        top = (np.abs(xx - ccx) < 9) & (rr > R_IN - 4) & (rr < R_OUT + 4) & (yy < ccy)
        panel = np.median(lum[(rr > R_IN - 14) & (rr < R_IN - 6)])
        cream = np.percentile(reg[top & (lum > 150)], 70, axis=0)
        a_top = np.clip((lum - panel) / max(cream.mean() - panel, 1.0), 0, 1) * top
        # erase the whole ring (ticks and their stray copies) back to the panel
        ang_ = np.degrees(np.arctan2(xx - ccx, ccy - yy))          # 0 at the top, clockwise
        ring = ((rr > R_IN - 6) & (rr < R_OUT + 6) & (np.abs(ang_) < 150)).astype(np.uint8) * 255   # not the label below
        clean = cv2.inpaint(np.clip(reg, 0, 255).astype(np.uint8), ring, 7, cv2.INPAINT_TELEA).astype(np.float32)
        # redraw: the top tick rotated about the centre to each even angle
        acc = np.zeros((S, S), np.float32)
        for ang in ANGLES:
            M = cv2.getRotationMatrix2D((ccx, ccy), -ang, 1.0)   # cv2: positive = counter-clockwise
            acc = np.maximum(acc, cv2.warpAffine(a_top.astype(np.float32), M, (S, S), flags=cv2.INTER_CUBIC))
        acc = np.clip(acc, 0, 1)[..., None]
        out[y0:y0 + S, x0:x0 + S] = cream * acc + clean * (1 - acc)
    return out
