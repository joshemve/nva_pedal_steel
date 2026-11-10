"""Reference (ref_rosewood_1) mapped into UI space next to / blended with a composite.  python overlay.py comp.png out.png"""
import sys
from PIL import Image
c = Image.open(sys.argv[1]).convert("RGB")
r = Image.open(__file__.replace("overlay.py", "gen/ref_rosewood_1_1.png")).convert("RGB").resize((1600, 1000), Image.LANCZOS)
# the reference's steel zone (y 75..630) onto ours (96..672)
k = 576 / 555; r2 = r.resize((1600, round(1000 * k)), Image.LANCZOS); off = round(96 - 75 * k)
ref = Image.new("RGB", (1600, 1000)); ref.paste(r2, (0, off))
box = (0, 96, 1600, 672)
o = Image.new("RGB", (1600, 576 * 3 + 20), "white")
o.paste(ref.crop(box), (0, 0)); o.paste(c.crop(box), (0, 586)); o.paste(Image.blend(ref, c, 0.5).crop(box), (0, 1172))
o.save(sys.argv[2])
