"""Re-composite the exact Blender top bar + deck over every photoreal steel image (round20, round21)."""
from PIL import Image, ImageFilter
import glob, os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
skin21 = {'i01': 'walnut', 'i01d': 'walnut', 'i04': 'maple', 'i06': 'redsparkle', 'i13': 'pearloid', 'i18': 'koa'}
mask = Image.new('L', (1600, 1000), 0); mask.paste(255, (0, 0, 1600, 99)); mask.paste(255, (0, 668, 1600, 1000))
mask = mask.filter(ImageFilter.GaussianBlur(1.2))
for f in sorted(glob.glob('round20/r_*_*.png') + glob.glob('round21/i*_*.png')):
    b = os.path.basename(f)
    sk = b.split('_')[1] if b.startswith('r_') else skin21[b.rsplit('_', 1)[0]]
    photo = Image.open(f).convert('RGB').resize((1600, 1000), Image.LANCZOS)
    frame = Image.open(f'final/skin_{sk}.png').convert('RGB')
    Image.composite(frame, photo, mask).save('comp/' + b)
print('recomposited')
