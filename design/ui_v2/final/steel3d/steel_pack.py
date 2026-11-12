"""The 3D pedal steel in the plugin (blender/steel_3d.py, STEEL_MODE=layers / static).
  background(dir)   canvas image: the UI background with the instrument-free room and that render dir's static plate
  package(dir)      moving layers -> ../parts/sprites/f3d_*.png + floor3d.json, tone bar sprite + sprites.json entries,
                    ../parts/steel_layout.json (fret x, string rows, bridge: what gen_layout.py reads)
  skins(dirs)       other finishes' static plates -> ../parts/skins/skin_<name>.jpg + skins.json
  python steel_pack.py package <layers_dir> [<brass_layers_dir>]
  python steel_pack.py cards walnut=<dir> maple=<dir> ...   the five instrument cards of the preset browser
  python steel_pack.py skins maple=<dir> redsparkle=<dir> pearloid=<dir> koa=<dir>
  python steel_pack.py check <layers_dir> <out.png>       rest | everything engaged
A layer frame is stored as "what changes against the static plate": the frame composited on the plate, with alpha from
the difference, so parts hide behind the cabinet and carry their own shadows exactly as rendered."""
import json, os, shutil, sys
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); P = os.path.join(H, "..", "parts")
CW, CH = 3200, 2000; RX, RY = CW / 1568, CH / 1003
ZONE = (round(96 * RX), round(104 * RY), round(1494 * RX), round(668 * RY))     # the steel's room: never the bar or deck
ROOM = "room_empty_1"; SHADOW = 0.70                                              # floor shadow depth kept
LAYERS = ["knee_L", "knee_R", "pedal_A", "pedal_B", "pedal_C"]
_room = None


def grade(rgb):
    """the reference's crisp, deep look: a little local sharpening, deeper shadows, firmer contrast"""
    import cv2 as _cv
    x = rgb.astype(np.float32)
    x = x + 0.55 * (x - _cv.GaussianBlur(x, (0, 0), 1.3))
    x = np.clip(x, 0, 255) / 255.0
    x = x ** 1.12
    x = np.clip((x - 0.5) * 1.10 + 0.5 + 0.015, 0, 1)
    return x * 255.0


def room():
    global _room
    if _room is None:
        bg = np.asarray(Image.open(os.path.join(P, "bg_clean.png")).convert("RGB"), np.float32)
        m = json.load(open(os.path.join(P, "aligned/%s.json" % ROOM))); x0, y0, x1, y1 = m["canvas_box"]
        r = np.asarray(Image.open(os.path.join(P, "aligned/%s.png" % ROOM)).convert("RGB"), np.float32)
        a = np.zeros((CH, CW), np.float32); a[ZONE[1]:ZONE[3], ZONE[0]:ZONE[2]] = 1; a = cv2.GaussianBlur(a, (0, 0), 6)[..., None]
        full = bg.copy(); full[max(y0, 0):y1, x0:x1] = r[max(y0, 0) - y0:, :]
        _room = (full * a + bg * (1 - a), a)
    return _room


def over(base, rgba, x0=0, y0=0):
    """a render (straight alpha, shadow-catcher pixels black) over a canvas image; returns the composited region"""
    h, w = rgba.shape[:2]; st = rgba.astype(np.float32); a = room()[1][y0:y0 + h, x0:x0 + w]
    st[..., :3] = grade(st[..., :3])
    sh = np.clip(1 - st[..., :3].max(2, keepdims=True) / 6.0, 0, 1)
    al = st[..., 3:] / 255 * (1 - (1 - SHADOW) * sh) * a
    return st[..., :3] * al + base[y0:y0 + h, x0:x0 + w] * (1 - al)


def background(d):
    return over(room()[0], np.asarray(Image.open(os.path.join(d, "static.png")).convert("RGBA")))


def frame_sprite(p0, d, name, f, reg):
    x0, y0, x1, y1 = reg
    fr = np.asarray(Image.open(os.path.join(d, "%s_%02d.png" % (name, f))).convert("RGBA"))
    y1, x1 = y0 + fr.shape[0], x0 + fr.shape[1]                          # Blender rounds the border to whole pixels itself
    img = over(room()[0], fr, x0, y0); ref = p0[y0:y1, x0:x1]
    dd = np.abs(img - ref).max(2)
    core = (dd > 28).astype(np.uint8)
    core = cv2.morphologyEx(core, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    near = cv2.dilate(core, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (91, 91)))
    al = np.clip((dd - 4.0) / 10.0, 0, 1) * near
    al = cv2.GaussianBlur(al, (0, 0), 0.7)
    return np.dstack([img, al * 255])


def package(d, brass=None):
    """d: the chrome layers dir; brass: a layers dir rendered with STEEL_METAL=brass (same boxes for both sets)"""
    info = json.load(open(os.path.join(d, "layers.json"))); p0 = background(d)
    sets = {"chrome": (d, p0)}
    if brass: sets["brass"] = (brass, background(brass))
    os.makedirs(os.path.join(P, "sprites"), exist_ok=True)
    out = {"layers": {}, "body_y": {k: 180 for k in ("walnut", "maple", "redsparkle", "pearloid", "koa")}, "rect": list(ZONE)}
    # the cross bar is part of the plate now; the layer stays as an empty frame so the editor's list is unchanged
    for metal in ("chrome", "brass"):
        Image.new("RGBA", (8, 8), (0, 0, 0, 0)).save(os.path.join(P, "sprites", "f3d_%s_bar.png" % metal))
    out["layers"]["bar"] = {"box": [1600, 1200, 1608, 1208], "frames": 1}
    for name in LAYERS:
        reg = info["layers"][name]["region"]
        fs = {m: [frame_sprite(pp, dd, name, f, reg) for f in range(info["frames"])] for m, (dd, pp) in sets.items()}
        hh = min(f.shape[0] for v in fs.values() for f in v); ww = min(f.shape[1] for v in fs.values() for f in v)
        fs = {m: [f[:hh, :ww] for f in v] for m, v in fs.items()}
        al = np.max([f[..., 3] for v in fs.values() for f in v], 0); ys, xs = np.where(al >= 4)
        x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        for m in ("chrome", "brass"):
            v = fs.get(m, fs["chrome"]); strip = np.concatenate([f[y0:y1, x0:x1] for f in v], 0)
            Image.fromarray(np.clip(strip, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(P, "sprites", "f3d_%s_%s.png" % (m, name)), optimize=True)
        out["layers"][name] = {"box": [int(x0 + reg[0]), int(y0 + reg[1]), int(x1 + reg[0]), int(y1 + reg[1])], "frames": info["frames"]}
        print(name, out["layers"][name])
    json.dump(out, open(os.path.join(P, "sprites", "floor3d.json"), "w"), indent=1)
    # tone bar: its own pixels, cropped; a soft contact shadow under it
    b = info["bar"]; reg = b["region"]
    bar = np.asarray(Image.open(os.path.join(d, "bar.png")).convert("RGBA"), np.float32)
    sh = cv2.GaussianBlur(bar[..., 3], (0, 0), 5) * 0.45
    sh = np.roll(sh, 5, axis=0)                                           # falls a little toward the player
    a = np.maximum(bar[..., 3], sh * (1 - bar[..., 3] / 255))
    rgb = bar[..., :3] * (bar[..., 3:] / 255) / np.maximum(a[..., None] / 255, 1e-3)
    spr = np.dstack([np.clip(rgb, 0, 255), a])
    ys, xs = np.where(a >= 4); x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    Image.fromarray(spr[y0:y1, x0:x1].astype(np.uint8), "RGBA").save(os.path.join(P, "sprites", "tone_bar.png"), optimize=True)
    Image.new("RGBA", (8, 8), (0, 0, 0, 0)).save(os.path.join(P, "sprites", "tone_bar_home.png"))
    S = json.load(open(os.path.join(P, "sprites", "sprites.json")))
    S["tone_bar"] = {"box": [int(x0 + reg[0]), int(y0 + reg[1]), int(x1 + reg[0]), int(y1 + reg[1])], "bar_cx": b["cx"]}
    S["tone_bar_home"] = {"box": [1600, 400, 1608, 408]}
    json.dump(S, open(os.path.join(P, "sprites", "sprites.json"), "w"), indent=1)
    lay = json.load(open(os.path.join(d, "steel.json"))); lay["bar_fret"] = b["fret"]; lay["bar_cx"] = b["cx"]
    json.dump(lay, open(os.path.join(P, "steel_layout.json"), "w"), indent=1)
    Image.fromarray(np.clip(p0, 0, 255).astype(np.uint8)).save(os.path.join(P, "bg_steel3d.png"))
    print("tone bar", S["tone_bar"])


def skins(dirs):
    os.makedirs(os.path.join(P, "skins"), exist_ok=True); out = {}
    box = [ZONE[0] - 14, ZONE[1] - 14, ZONE[2] + 14, ZONE[3] + 14]
    for sk, d in dirs.items():
        img = background(d)
        Image.fromarray(np.clip(img[box[1]:box[3], box[0]:box[2]], 0, 255).astype(np.uint8)).save(os.path.join(P, "skins", "skin_%s.jpg" % sk), quality=94)
        Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((1600, 1000), Image.LANCZOS).save(os.path.join(P, "skins", "_full_%s.jpg" % sk), quality=88)
        out[sk] = {"box": box, "job": "steel3d:" + sk}; print(sk, box)
    json.dump(out, open(os.path.join(P, "skins", "skins.json"), "w"), indent=1)


def rest_image(plate_dir, metal="chrome"):
    """the plate with every moving layer at rest and the tone bar at its reference fret (what the plugin shows idle)"""
    im = background(plate_dir); S = json.load(open(os.path.join(P, "sprites", "sprites.json")))
    F = json.load(open(os.path.join(P, "sprites", "floor3d.json")))["layers"]
    for name in LAYERS:
        st = np.asarray(Image.open(os.path.join(P, "sprites", "f3d_%s_%s.png" % (metal, name))).convert("RGBA"), np.float32)
        b = F[name]["box"]; h = b[3] - b[1]; fr = st[:h]; a = fr[..., 3:] / 255
        im[b[1]:b[3], b[0]:b[2]] = fr[..., :3] * a + im[b[1]:b[3], b[0]:b[2]] * (1 - a)
    tb = S["tone_bar"]["box"]; t = np.asarray(Image.open(os.path.join(P, "sprites", "tone_bar.png")).convert("RGBA"), np.float32); a = t[..., 3:] / 255
    im[tb[1]:tb[3], tb[0]:tb[2]] = t[..., :3] * a + im[tb[1]:tb[3], tb[0]:tb[2]] * (1 - a)
    return im


def cards(dirs):
    """the five instrument cards of the preset browser (card number -> finish), 726x316, from the 3D steel"""
    num = {"walnut": 1, "maple": 4, "redsparkle": 6, "pearloid": 13, "koa": 18}
    dst = os.path.join(H, "..", "..", "..", "..", "Source", "ui_assets")
    for sk, d in dirs.items():
        im = rest_image(d, "chrome")   # koa is chrome now too (2026-11-11)
        c = Image.fromarray(np.clip(im, 0, 255).astype(np.uint8)).crop((190, 160, 3010, 1386)).resize((726, 316), Image.LANCZOS)
        c.save(os.path.join(dst, "card_%02d.jpg" % num[sk]), quality=92); print("card_%02d" % num[sk], sk)


def check(d, outp):
    info = json.load(open(os.path.join(d, "layers.json"))); p0 = background(d); S = json.load(open(os.path.join(P, "sprites", "sprites.json")))
    F = json.load(open(os.path.join(P, "sprites", "floor3d.json")))["layers"]; tiles = []
    for f in (0, info["frames"] - 1):
        im = p0.copy()
        for name in LAYERS:
            st = np.asarray(Image.open(os.path.join(P, "sprites", "f3d_chrome_%s.png" % name)).convert("RGBA"), np.float32)
            b = F[name]["box"]; h = b[3] - b[1]; fr = st[f * h:(f + 1) * h]; a = fr[..., 3:] / 255
            im[b[1]:b[3], b[0]:b[2]] = fr[..., :3] * a + im[b[1]:b[3], b[0]:b[2]] * (1 - a)
        tb = S["tone_bar"]["box"]; t = np.asarray(Image.open(os.path.join(P, "sprites", "tone_bar.png")).convert("RGBA"), np.float32)
        dx = 0 if f == 0 else 500; a = t[..., 3:] / 255
        im[tb[1]:tb[3], tb[0] + dx:tb[2] + dx] = t[..., :3] * a + im[tb[1]:tb[3], tb[0] + dx:tb[2] + dx] * (1 - a)
        tiles.append(Image.fromarray(np.clip(im, 0, 255).astype(np.uint8)).resize((1600, 1000), Image.LANCZOS).crop((0, 90, 1600, 680)))
    o = Image.new("RGB", (1600, 1190), "white"); o.paste(tiles[0], (0, 0)); o.paste(tiles[1], (0, 600)); o.save(outp)


if __name__ == "__main__":
    if sys.argv[1] == "package": package(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    elif sys.argv[1] == "cards": cards(dict(a.split("=") for a in sys.argv[2:]))
    elif sys.argv[1] == "skins": skins(dict(a.split("=") for a in sys.argv[2:]))
    elif sys.argv[1] == "check": check(sys.argv[2], sys.argv[3])
