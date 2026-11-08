"""Clean-plate retexturing (Joshua: "patchy spotting ... around knobs", around the pedal lamps). ChatGPT's empty-panel
patches are blotchy (texture std ~12 vs ~3 on the real leatherette, mid-scale ~9 vs ~2) and leave halos where glow or
text was removed. For every patched pixel of flat deck panel: colour from the untouched panel around it (normalized
convolution), grain synthesised to the real panel's statistics. Labels, outlines, ticks, lamps, chrome, pedal blocks,
the slider and the knob sweep areas (their filmstrips bake the plate there) are left alone."""
import numpy as np, cv2

CW, CH = 3200, 2000; RX, RY = CW / 1568, CH / 1003
blur = lambda a, s: cv2.GaussianBlur(a, (0, 0), s)
EXCLUDE_REF = [(962, 712, 1228, 880),     # lamps + pedals A B C (real pixels from the reference pedal C)
               (1250, 712, 1495, 870),    # knee levers
               (750, 904, 1172, 966)]     # volume slider and groove
KNOBS_REF = [(432.6, 779.1, 56), (571.2, 779.0, 56), (731.5, 779.3, 50), (858.8, 779.4, 50)]   # scallop + space sweeps


def retexture(clean, hd, seed=7):
    out = clean.copy()
    deck = np.zeros((CH, CW), bool); deck[round(676 * RY):round(985 * RY), round(14 * RX):round(1554 * RX)] = True
    D = (np.abs(clean - hd).mean(2) > 4) & deck
    D = cv2.dilate(D.astype(np.uint8), np.ones((5, 5), np.uint8)) > 0
    L = clean.mean(2)
    # real detail = bright (labels, outlines, ticks, lamps, chrome); blotches are dark, so brightness alone decides
    struct = (blur(L, 1.0) > 62) | ((np.hypot(*np.gradient(blur(L, 1.0))) > 16) & (blur(L, 3) > 45))
    struct = cv2.dilate(struct.astype(np.uint8), np.ones((3, 3), np.uint8)) > 0
    keep = np.zeros((CH, CW), bool)
    for x0, y0, x1, y1 in EXCLUDE_REF: keep[round(y0 * RY):round(y1 * RY), round(x0 * RX):round(x1 * RX)] = True
    yy, xx = np.mgrid[0:CH, 0:CW]
    # (knob sweep areas used to be kept for the filmstrips' baked rim; fix_knob_halo.py replaced that with a clean shadow)
    F = D & ~struct & ~keep & (blur(L, 4) < 70)
    # colour: flat reference panel around it
    Lh = hd.mean(2)
    struct_ref = (blur(Lh, 1.0) > 62) | ((np.hypot(*np.gradient(blur(Lh, 1.0))) > 16) & (blur(Lh, 3) > 45))
    struct_ref = cv2.dilate(struct_ref.astype(np.uint8), np.ones((7, 7), np.uint8)) > 0
    ref_flat = deck & ~D & ~struct_ref & (blur(Lh, 2) < 70)   # the closest real panel, e.g. a value box's own interior
    W = ref_flat.astype(np.float32)
    low = blur(clean, 6); have = np.zeros((CH, CW), np.float32)
    for sig in (40, 14, 6):   # coarse to fine: the nearest real panel wins, nothing falls back to the blotchy patch
        den = blur(W, sig)
        f = np.stack([blur(hd[..., c] * W, sig) for c in range(3)], 2) / np.maximum(den, 1e-4)[..., None]
        a = np.clip(den / 0.06, 0, 1)
        low = f * a[..., None] + low * (1 - a[..., None]); have = np.maximum(have, a)
    # grain: the real panel's fine (std ~2.6) and mid-scale (std ~1.9) luminance texture
    rng = np.random.default_rng(seed)
    n1 = blur(rng.standard_normal((CH, CW)).astype(np.float32), 0.7); n1 *= 2.8 / max(n1[deck].std(), 1e-6)
    n2 = blur(rng.standard_normal((CH, CW)).astype(np.float32), 3.0); n2 *= 0.7 / max(n2[deck].std(), 1e-6)
    grain = (n1 + n2)[..., None] * (low / np.maximum(low.mean(2, keepdims=True), 1.0))
    new = np.clip(low + grain, 0, 255)
    a = np.clip(blur(F.astype(np.float32), 1.5), 0, 1)[..., None]
    out = new * a + out * (1 - a)
    return out, F
