import sys
from PIL import Image
ref, ours, out = sys.argv[1:4]
def crop(p):
    im = Image.open(p).convert("RGB").resize((1200, 750))
    return im.crop((80, 160, 1160, 330))
a, b = crop(ref), crop(ours)
c = Image.new("RGB", (1080, 340)); c.paste(a, (0, 0)); c.paste(b, (0, 170)); c.resize((1620, 510)).save(out)
