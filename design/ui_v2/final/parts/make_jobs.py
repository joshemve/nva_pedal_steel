"""Cut reference crops (3:2, upscaled to 1536x1024) around every UI part and write ChatGPT jobs that make each
part's other states / clean plate from the reference. Boxes are in APPROVED_main.png pixels (1568x1003).
Outputs: parts/crops/<name>.png + crops.json (crop box per job), parts/jobs.json."""
import json, os
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
REF = Image.open("../../APPROVED_main.png").convert("RGB")

# part boxes (ref px)
KNOBS = {"TONE": (130.1, 778.1), "PICK": (265.8, 778.1), "GLIDE": (432.6, 779.1), "SWELL": (571.2, 779.0),
         "ECHO": (731.5, 779.3), "REVERB": (858.8, 779.4)}   # REF_CENTRE (UI px) * (1568/1600, 1003/1000)
PARTS = {
    "pedals": (962, 690, 1222, 875), "display": (592, 38, 974, 84), "meter": (1232, 42, 1392, 64),
    "trim": (1428, 28, 1490, 84), "slider": (752, 912, 1168, 962), "pills": (90, 862, 930, 900),
    "modebox": (183, 912, 462, 950), "pctbox": (1178, 915, 1256, 948),
}
for k, (x, y) in KNOBS.items():
    r = 52 if k in ("GLIDE", "SWELL") else 46
    PARTS["knob_" + k] = (x - r, y - r, x + r, y + r)

def crop32(box, margin=0.25, minw=180):
    x0, y0, x1, y1 = box; w, h = x1 - x0, y1 - y0
    W = max(w * (1 + 2 * margin), h * (1 + 2 * margin) * 1.5, minw); Hh = W / 1.5
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    X0 = min(max(cx - W / 2, 0), REF.width - W); Y0 = min(max(cy - Hh / 2, 0), REF.height - Hh)
    return [round(X0, 2), round(Y0, 2), round(X0 + W, 2), round(Y0 + Hh, 2)]

SAME = ("This is an upscaled crop of a photoreal audio-plugin UI. Output ONE image, landscape 1536x1024, with EXACTLY "
        "the same framing, scale, perspective, lighting, colours and every detail as the input, so it overlays the input "
        "pixel for pixel. Change ONLY this: ")
BG = " Everything outside that change must stay identical to the input."
CHANGE = {
    "pedals_unlit": ("pedals", "turn every amber light OFF. The round LED indicators above A and B become unlit dark grey "
                     "glass exactly like the unlit one above C, and the warm amber glow under the top edge of the A and B "
                     "pedal blocks disappears, and A and B come back up to the raised, un-pressed position, so all three pedal blocks look exactly like pedal C (same height, same chrome, no glow)."),
    "pedals_lit": ("pedals", "turn pedal C ON exactly like A and B: its round LED indicator glows amber like A's and B's, "
                   "and its pedal block is pressed down exactly like A and B (same shorter, tilted look) with the same warm amber glow showing under its top edge."),
    "display_empty": ("display", "remove all the amber text characters from the LED dot-matrix display. The display window "
                      "stays, with its dark glass and only very faint unlit dots; bezel, arrows and bar unchanged."),
    "meter_off": ("meter", "the OUTPUT level meter shows NO level: every segment is unlit (dark, like the rightmost unlit "
                  "segments now)."),
    "meter_on": ("meter", "the OUTPUT level meter is fully lit: every segment glows amber like the lit ones on the left."),
    "trim_clean": ("trim", "remove the small TRIM knob entirely; show the cream top-bar surface where it sat, with its "
                   "printed tick mark above it still there, the TRIM label and 0.0 dB text unchanged."),
    "slider_empty": ("slider", "remove the cream slider handle (thumb) of the VOLUME PEDAL slider; the slider groove/track "
                     "continues straight through where the handle was, and the tick marks below are unchanged."),
    "slider_thumb": ("slider", "keep ONLY the cream slider handle (thumb) of the VOLUME PEDAL slider, at the same size and "
                     "position, and replace everything else with flat pure magenta #FF00FF (no shadow on the magenta)."),
    "pills_empty": ("pills", "remove the value text inside every outlined value box (65 %, 40 %, 145 ms, 35 %, 18 %, 28 %): "
                    "the outlined boxes stay, empty, with the same black panel inside. Labels TONE, PICK etc. unchanged."),
    "modebox_empty": ("modebox", "remove the text 'Smart Auto-Pedal E9' inside the MODE dropdown box; keep the box outline "
                      "and the down-arrow triangle."),
    "pctbox_empty": ("pctbox", "remove the text '72 %' inside the small outlined box next to the slider; keep the box."),
}
for k in KNOBS:
    CHANGE["knobclean_" + k] = ("knob_" + k, "remove the knob in the centre completely (body, skirt and pointer), leaving "
        "the flat matte black panel where it sat, with the ring of short printed cream tick marks around it unchanged and "
        "only a very soft, faint round shadow on the panel where the knob stood. Neighbouring knobs untouched.")
CHANGE["steel_hd_l"] = ("steel_l", "nothing: reproduce it as a sharper, higher-detail version (crisper strings, frets, "
                        "chrome reflections and wood grain), same glare and highlights in the same places.")
CHANGE["steel_hd_r"] = ("steel_r", CHANGE["steel_hd_l"][1])
PARTS["steel_l"] = (130, 120, 800, 560); PARTS["steel_r"] = (770, 120, 1460, 560)

if __name__ == "__main__":   # importing this module (for crop32/SAME/BG) must not rewrite crops.json
    os.makedirs("crops", exist_ok=True); boxes = {}; jobs = []
    for name, (part, change) in CHANGE.items():
        if part not in boxes:
            b = crop32(PARTS[part], margin=0.05 if part.startswith("steel") else 0.25)
            boxes[part] = {"crop": b, "part": PARTS[part]}
            REF.crop(tuple(b)).resize((1536, 1024), Image.LANCZOS).save("crops/%s.png" % part)
        jobs.append({"name": name, "images": ["final/parts/crops/%s.png" % part], "prompt": SAME + change + BG})
    json.dump(boxes, open("crops.json", "w"), indent=1)
    json.dump(jobs, open("jobs.json", "w"), indent=1)
    print(len(jobs), "jobs,", len(boxes), "crops")
