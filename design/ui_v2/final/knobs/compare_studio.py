"""Reference vs native Cycles pixels. No recolouring or reference-pixel substitution."""
import json, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

here = Path(__file__).resolve().parent
ref = Image.open(here.parents[1] / "APPROVED_main.png").convert("RGB")
folder = Path(sys.argv[1]); meta = json.loads((folder / "render.json").read_text())
ren = Image.open(folder / "reference_angles.png").convert("RGBA")
scale = meta["scale"]
sheet = Image.new("RGB", (720, 300), "#181613")
draw = ImageDraw.Draw(sheet)
report = {}
for i, (style, rx, rr) in enumerate(zip(meta["styles"], [130, 433, 733], [29, 44.4, 38])):
    a = ref.crop((rx-60, 717, rx+60, 837))
    cx = meta["centres"][i]
    b = ren.crop(((cx-60)*scale, 12*scale, (cx+60)*scale, 132*scale)).resize((120,120), Image.Resampling.LANCZOS)
    bg = Image.new("RGBA", (120,120), (30,26,21,255)); bg.alpha_composite(b)
    sheet.paste(a.resize((240,240)), (i*240,30))
    # Save per-style comparison without interpolating the original alpha.
    pair = Image.new("RGB", (480,240), "#181613")
    pair.paste(a.resize((240,240)), (0,0)); pair.paste(bg.convert("RGB").resize((240,240)), (240,0))
    pair.save(folder / ("compare_" + style + ".png"))
    ar, br = np.asarray(a).astype(float), np.asarray(bg.convert("RGB")).astype(float)
    yy,xx = np.mgrid[:120,:120]
    mask = ((xx-60)**2+(yy-63)**2 < (rr*.45)**2)
    report[style] = {"reference_centre_rgb":ar[mask].mean(0).round(2).tolist(),
                     "render_centre_rgb":br[mask].mean(0).round(2).tolist(),
                     "centre_rgb_mae":round(float(np.abs(ar[mask]-br[mask]).mean()),3)}
    draw.text((i*240+10,8), style + " / reference", fill="white")
sheet.save(folder / "reference_sheet.png")
(folder / "measurement.json").write_text(json.dumps(report, indent=2))
print(json.dumps(report))
