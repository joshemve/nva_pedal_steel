"""Stack frames into vertical filmstrips (final/strips/knob_<style>.png) and write strips.json with frame size
and the knob centre/size in 1600x1000 UI px, then a check image: plate + strips at the deck's values vs full render."""
import glob, json, os
from PIL import Image
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "strips")
# centres/radii mirror final_ui.py (X = x*1600/1568); value angles match the approved screen
X = lambda x: x * 1600 / 1568
# deck knob centres = REF_CENTRE in final_ui.py (tick-ring centres measured on APPROVED_main.png)
KN = {"dome": [(132.8, 775.8, 29.0, 35), (271.3, 775.8, 29.0, -50)], "scallop": [(441.4, 776.8, 44.4, 30), (582.9, 776.7, 44.4, 25)],
      "space": [(746.4, 777.0, 38, -100), (876.3, 777.1, 38, -68)], "trim": [(X(1457), 60, 18, 0)]}
# measured offset of each reference knob from its tick-ring centre (matcap_strips.py -> offsets.json): draw it there
_op = os.path.join(D, "offsets.json")
if os.path.exists(_op):
    for st, offs in json.load(open(_op)).items():
        KN[st] = [(cx + o[0], cy + o[1], r, a) for (cx, cy, r, a), o in zip(KN[st], offs)]
meta = {}
strips = {}
for st in KN:
    FR = os.environ.get("FRAMES", "frames_fixed" if os.path.isdir(os.path.join(D, "frames_fixed")) else "frames")
    fs = sorted(glob.glob(os.path.join(D, FR, st + "_[0-9][0-9][0-9][0-9].png")))
    if not fs: continue
    ims = [Image.open(f).convert("RGBA") for f in fs]; w, h = ims[0].size
    s = Image.new("RGBA", (w, h * len(ims)))
    for i, im in enumerate(ims): s.paste(im, (0, i * h))
    s.save(os.path.join(D, "knob_%s.png" % st), optimize=True); strips[st] = ims
    side = json.load(open(os.path.join(D, "frames", st + ".json")))
    meta[st] = dict(frames=len(ims), frame_px=[w, h], scale=side["scale"], frame_ui_px=w / side["scale"],
                    angle_range_deg=[-135, 135], note="draw frame centred on the knob centre (UI px, 1600x1000 layout)",
                    instances=[dict(cx=round(cx, 2), cy=round(cy, 2), r=r) for cx, cy, r, _ in KN[st]])
json.dump(meta, open(os.path.join(D, "strips.json"), "w"), indent=1)
plate = os.path.join(D, "plate_walnut.png")
if os.path.exists(plate) and strips:
    P = Image.open(plate).convert("RGBA"); sc = P.size[0] / 1600
    for st, inst in KN.items():
        if st not in strips: continue
        ims = strips[st]
        for cx, cy, r, ang in inst:
            fr = ims[round((ang + 135) / 270 * (len(ims) - 1))]
            P.alpha_composite(fr, (round(cx * sc - fr.size[0] / 2), round(cy * sc - fr.size[1] / 2)))
    P.convert("RGB").save(os.path.join(D, "_check_composite.jpg"), quality=90)
print(json.dumps({k: (v["frames"], v["frame_px"]) for k, v in meta.items()}))
