"""Compose the plugin UI exactly as the JUCE editor will draw it (layer by layer), at the reference's state, and diff it
against APPROVED_main.png. This is the spec + test for the editor. Writes _compose.png, _compose_diff.png and prints
per-region errors. ui_layout.json collects every geometry/colour the editor needs."""
import json, os
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont
from dotfont import dots
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
ST = "../strips"; CW, CH = 3200, 2000
REF = np.asarray(Image.open("../../APPROVED_main.png").convert("RGB").resize((CW, CH), Image.LANCZOS), np.float32)
canvas = np.asarray(Image.open("bg_clean.png"), np.float32).copy()
S = json.load(open("sprites/sprites.json")); T = json.load(open("text.json")); DSP = json.load(open("display.json"))
strips = json.load(open(os.path.join(ST, "strips.json")))


def over(img_rgba, x, y):
    global canvas
    h, w = img_rgba.shape[:2]; x0, y0 = max(x, 0), max(y, 0); x1, y1 = min(x + w, CW), min(y + h, CH)
    s = img_rgba[y0 - y:y1 - y, x0 - x:x1 - x]; a = s[..., 3:] / 255
    canvas[y0:y1, x0:x1] = s[..., :3] * a + canvas[y0:y1, x0:x1] * (1 - a)


def sprite(name, clip_w=None):
    sp = np.asarray(Image.open("sprites/%s.png" % name), np.float32); b = S[name]["box"]
    if clip_w is not None: sp = sp.copy(); sp[:, clip_w:, 3] = 0
    over(sp, b[0], b[1])


# meter segments: columns of the lit sprite
mo = np.asarray(Image.open("sprites/meter_on.png"), np.float32)
Lm = mo[mo.shape[0] // 3:2 * mo.shape[0] // 3, :, :3].mean(2).mean(0); lit = Lm > 0.5 * (np.percentile(Lm, 90) + np.percentile(Lm, 10))
lit[:6] = False; lit[-6:] = False   # the bezel ends are bright too
edges = np.flatnonzero(np.diff(lit.astype(int)))
segs = [(edges[i] + 1, edges[i + 1] + 1) for i in range(0 if lit[0] == 0 else 1, len(edges) - 1, 2)]
print("meter segments:", len(segs))


def meter(level):   # level 0..1 -> lit segments (clip at the end of the last lit segment)
    n = int(round(level * len(segs)))
    if n: sprite("meter_on", clip_w=segs[n - 1][1] + 1)


def knob(style, inst, ang):
    m = strips[style]; n = m["frames"]; fs = m["frame_px"][0]; sc = m["scale"]
    k = int(round((ang + 135) / 270 * (n - 1)))
    strip = Image.open(os.path.join(ST, "knob_%s.png" % style))
    f = np.asarray(strip.crop((0, k * fs, fs, (k + 1) * fs)).convert("RGBA"), np.float32)
    over(f, int(round(inst["cx"] * sc - fs / 2)), int(round(inst["cy"] * sc - fs / 2)))


PS = json.load(open(os.path.join(ST, "ptr_strips.json")))


def pointer(name, ang):
    p = PS[name]; n = p["frames"]; fs = p["frame_px"]; k = int(round((ang + 135) / 270 * (n - 1)))
    f = np.asarray(Image.open(os.path.join(ST, "ptr_%s.png" % name)).crop((0, k * fs, fs, (k + 1) * fs)), np.float32)
    over(f, p["origin_canvas"][0], p["origin_canvas"][1])


K = np.load("display_kernel.npy")


def display(text):
    x0, y0, x1, y1 = DSP["box_canvas"]; p, py = DSP["pitch"], DSP["pitch_y"]; oy = DSP["origin"][1]
    D = dots(text); cols = max(c for c, r in D) + 1 if D else 1
    ox = DSP["text_centre_x"] - (cols - 1) * p / 2          # centred on the reference text's centre
    h, w = y1 - y0, x1 - x0; imp = np.zeros((h, w), np.float32)
    for c, r in D:
        x, y = ox + c * p, oy + r * py; xi, yi = int(np.floor(x)), int(np.floor(y)); fx, fy = x - xi, y - yi
        for dy, wy in ((0, 1 - fy), (1, fy)):
            for dx, wx in ((0, 1 - fx), (1, fx)):
                if 0 <= yi + dy < h and 0 <= xi + dx < w: imp[yi + dy, xi + dx] += wx * wy
    add = np.stack([cv2.filter2D(imp, -1, K[..., ch], borderType=cv2.BORDER_CONSTANT) for ch in range(3)], 2)
    canvas[y0:y1, x0:x1] = np.clip(canvas[y0:y1, x0:x1] + add, 0, 255)


def text(key, s=None):
    t = T[key]; s = s or t["text"]; FD = "../../fonts/%s.ttf" % t["font"]; SS = 4
    f = ImageFont.truetype(FD, round(t["size_canvas"] * SS))
    d0 = ImageDraw.Draw(Image.new("L", (8, 8))); l, tp, r, b = d0.textbbox((0, 0), s, font=f)
    W, Hh = (r - l) // SS + 8, int(t["size_canvas"] * 2)
    im = Image.new("L", (W * SS, Hh * SS)); d = ImageDraw.Draw(im)
    # centre the ink box horizontally on the reference ink centre (values) / keep left edge (mode)
    x_ink = t["ink_left"] if key == "mode" else t["ink_cx"] - (r - l) / SS / 2
    ox, oy = x_ink - 4, t["ink_cy"] - Hh / 2
    # vertical: ink-centre of the *reference* string so baselines stay put whatever the value
    _, rt, _, rb = d0.textbbox((0, 0), t["text"], font=f)
    d.text((4 * SS - l, Hh * SS / 2 - (rt + rb) / 2), s, fill=255, font=f)
    m = np.asarray(im.resize((W, Hh), Image.BOX), np.float32)[..., None] / 255
    rgba = np.concatenate([np.broadcast_to(np.array(t["colour"], np.float32), m.shape[:2] + (3,)), m * 255], 2)
    over(rgba, int(round(ox)), int(round(oy)))


if __name__ == "__main__":
    for k in ("A", "B"): sprite("pedal_%s_on" % k)
    meter(11 / 16)
    sprite("slider_thumb")
    for st, angs in (("scallop", (30, 25)), ("space", (-100, -68))):
        for inst, a in zip(strips[st]["instances"], angs): knob(st, inst, a)
    for K_, a in (("TONE", 35), ("PICK", -50), ("TRIM", 0.0)): pointer(K_, a)   # cream domes: body in the plate, pointer only
    display("01 SWEET GEORGIA E9")
    for k in T: text(k)
    Image.fromarray(canvas.clip(0, 255).astype(np.uint8)).save("_compose.png")
    d = np.abs(canvas - REF).mean(2)
    Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)).resize((1600, 1000), Image.BOX).save("_compose_diff.png")
    R = {"top bar": (0, 0, 3200, 200), "steel+room": (0, 200, 3200, 1370), "deck": (0, 1370, 3200, 2000),
         "knobs": (160, 1440, 1900, 1680), "pedals": (1960, 1380, 2500, 1750), "display": tuple(DSP["box_canvas"])}
    print("mean |err| vs reference (0-255): whole %.2f" % d.mean())
    ref0 = np.asarray(Image.open("../../APPROVED_main.png").convert("RGB"), np.float32)
    low = np.asarray(Image.fromarray(canvas.clip(0, 255).astype(np.uint8)).resize(ref0.shape[1::-1], Image.LANCZOS), np.float32)
    print("at the reference's own resolution: %.2f" % np.abs(low - ref0).mean())
    for k, (x0, y0, x1, y1) in R.items(): print("   %-11s %.2f" % (k, d[y0:y1, x0:x1].mean()))
