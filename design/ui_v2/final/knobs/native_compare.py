"""Reference deck vs native-render knobs placed on the production plate (static check of shape/colour).
python native_compare.py <studio_out_dir> <out.png>"""
import json, sys
from pathlib import Path
from PIL import Image
H = Path(__file__).resolve().parent
d = Path(sys.argv[1]).resolve(); meta = json.loads((d / "render.json").read_text())
still = Image.open(d / "reference_angles.png").convert("RGBA"); sc = meta["scale"]
S = json.load(open(H.parent / "strips" / "strips.json"))
plate = Image.open(H.parent / "strips" / "plate_walnut.png").convert("RGBA")
ps = plate.width / 1600
for j, st in enumerate(meta["styles"]):
    if st not in ("scallop", "space"): continue
    side = S[st]["frame_ui_px"]; cx = meta["centres"][j] * sc; cy = meta["centre_y"] * sc
    fr = still.crop((round(cx - side * sc / 2), round(cy - side * sc / 2), round(cx + side * sc / 2), round(cy + side * sc / 2)))
    fr = fr.resize((round(side * ps), round(side * ps)), Image.LANCZOS)
    for inst in S[st]["instances"]:
        plate.alpha_composite(fr, (round(inst["cx"] * ps - fr.width / 2), round(inst["cy"] * ps - fr.height / 2)))
ref = Image.open(H.parents[1] / "APPROVED_main.png").convert("RGB").resize((1600, 1000), Image.LANCZOS)
box = (380, 700, 950, 860)
a = ref.crop(box); b = plate.convert("RGB").resize((1600, 1000), Image.LANCZOS).crop(box)
out = Image.new("RGB", (a.width, a.height * 2)); out.paste(a, (0, 0)); out.paste(b, (0, a.height))
out = out.resize((out.width * 2, out.height * 2), Image.LANCZOS); out.save(sys.argv[2])
