"""Exact pedal steel fretboard texture: 24-inch scale, fret lines at true equal-tempered positions.
Writes textures/fretboard_albedo.png (and a matching roughness map). u runs from nut (0) to the
24th fret position + margin (1). Usage: python make_fretboard.py <out_dir>"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

SCALE = 0.6096            # 24 in
FRETS = 26
LEN = SCALE * (1 - 2 ** (-FRETS / 12)) + 0.012   # board length in metres (nut margin incl.)
W_M = 0.092               # board width in metres
PX_PER_M = 8000
W, H = int(LEN * PX_PER_M), int(W_M * PX_PER_M)
NUT = int(0.006 * PX_PER_M)

def fret_x(n):
    return NUT + int(SCALE * (1 - 2 ** (-n / 12)) * PX_PER_M)

def main(out):
    img = Image.new("RGB", (W, H), (14, 14, 15))
    rough = Image.new("L", (W, H), 150)
    d, dr = ImageDraw.Draw(img), ImageDraw.Draw(rough)
    # thin silver border lines along both edges
    for y in (int(H * 0.035), int(H * 0.965)):
        d.line([(NUT, y), (W, y)], fill=(170, 170, 172), width=3); dr.line([(NUT, y), (W, y)], fill=60, width=3)
    for n in range(0, FRETS + 1):
        x = fret_x(n); w = 7 if n == 0 else 4
        d.line([(x, int(H * .035)), (x, int(H * .965))], fill=(200, 200, 202), width=w)
        dr.line([(x, 0), (x, H)], fill=55, width=w)
    # position markers centred between frets (classic diamonds, double at 12 and 24)
    marks = {3: 1, 5: 1, 7: 1, 9: 1, 12: 2, 15: 1, 17: 1, 19: 1, 21: 1, 24: 2}
    cols = {3: (215, 60, 40), 5: (235, 225, 205), 7: (40, 110, 200), 9: (235, 225, 205), 12: (230, 190, 60),
            15: (215, 60, 40), 17: (235, 225, 205), 19: (40, 110, 200), 21: (235, 225, 205), 24: (230, 190, 60)}
    for n, k in marks.items():
        cx = (fret_x(n - 1) + fret_x(n)) // 2
        s = int(H * 0.07)
        ys = [H // 2] if k == 1 else [int(H * .33), int(H * .67)]
        for cy in ys:
            poly = [(cx, cy - s), (cx + s * 0.7, cy), (cx, cy + s), (cx - s * 0.7, cy)]
            d.polygon(poly, fill=cols[n]); dr.polygon(poly, fill=70)
    # small fret numbers near the edge, as on real steel fretboards
    try:
        f = ImageFont.truetype("arial.ttf", int(H * 0.06))
    except Exception:
        f = ImageFont.load_default()
    for n in (3, 5, 7, 9, 12, 15, 17, 19, 21, 24):
        cx = (fret_x(n - 1) + fret_x(n)) // 2
        d.text((cx, int(H * 0.88)), str(n), fill=(150, 150, 150), font=f, anchor="mm")
    os.makedirs(out, exist_ok=True)
    img.save(os.path.join(out, "fretboard_albedo.png")); rough.save(os.path.join(out, "fretboard_rough.png"))
    print(f"fretboard {W}x{H}px, length {LEN:.4f} m")

if __name__ == "__main__":
    main(sys.argv[1])
