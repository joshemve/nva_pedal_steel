"""Register every ChatGPT part image back onto its reference crop (ECC affine), match its tone to the reference in the
areas it didn't change, and write it in UI canvas space (3200x2000 = APPROVED_main.png resized, the strips' 2x grid).
Writes parts/aligned/<job>.png (full canvas-space patch of the crop box, RGB) + aligned/<job>.json (canvas box) and
parts/_check/<job>.png (reference crop | generated aligned | |diff| x3)."""
import json, os, sys, glob
import numpy as np, cv2
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
CW, CH = 3200, 2000
REF = Image.open("../../APPROVED_main.png").convert("RGB"); SX, SY = CW / REF.width, CH / REF.height
boxes = json.load(open("crops.json")); jobs = {j["name"]: j for j in json.load(open("jobs.json"))}
os.makedirs("aligned", exist_ok=True); os.makedirs("_check", exist_ok=True)


def ecc(tmpl, img, mask=None):
    """Affine warp taking img onto tmpl (both float32 gray), coarse-to-fine."""
    W = np.eye(2, 3, dtype=np.float32)
    for s in (0.25, 0.5, 1.0):
        t = cv2.resize(tmpl, None, fx=s, fy=s, interpolation=cv2.INTER_AREA)
        i = cv2.resize(img, None, fx=s, fy=s, interpolation=cv2.INTER_AREA)
        m = None if mask is None else cv2.resize(mask, (t.shape[1], t.shape[0]), interpolation=cv2.INTER_NEAREST)
        Ws = W.copy(); Ws[:, 2] *= s
        try:
            _, Ws = cv2.findTransformECC(t, i, Ws, cv2.MOTION_AFFINE,
                                         (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 200, 1e-6), m, 5)
        except cv2.error as e:
            print("   ecc failed at scale", s, e.msg if hasattr(e, "msg") else "");
        W = Ws.copy(); W[:, 2] /= s
    return W


def process(name, path):
    part = jobs[name]["images"][0].split("/")[-1][:-4]
    crop = np.asarray(Image.open("crops/%s.png" % part).convert("RGB"), np.float32)
    gen = np.asarray(Image.open(path).convert("RGB").resize((1536, 1024), Image.LANCZOS), np.float32)
    g = lambda a: cv2.GaussianBlur(a.mean(2), (0, 0), 1.5).astype(np.float32) / 255
    W = ecc(g(crop), g(gen))
    al = cv2.warpAffine(gen, W, (1536, 1024), flags=cv2.INTER_LINEAR | cv2.WARP_INVERSE_MAP, borderMode=cv2.BORDER_REFLECT)
    # tone match in the unchanged areas: per-channel gain/offset where generated and reference agree
    d = np.abs(cv2.GaussianBlur(al, (0, 0), 3) - cv2.GaussianBlur(crop, (0, 0), 3)).mean(2)
    keep = d < np.percentile(d, 60)
    for c in range(3):
        A = np.stack([al[..., c][keep], np.ones(keep.sum())], 1)
        k, b = np.linalg.lstsq(A, crop[..., c][keep], rcond=None)[0]
        al[..., c] = al[..., c] * k + b
    al = np.clip(al, 0, 255)
    err = np.abs(al - crop).mean(2); e_keep = err[keep].mean()
    X0, Y0, X1, Y1 = boxes[part]["crop"]
    cb = [round(X0 * SX), round(Y0 * SY), round(X1 * SX), round(Y1 * SY)]
    out = Image.fromarray(al.astype(np.uint8)).resize((cb[2] - cb[0], cb[3] - cb[1]), Image.LANCZOS)
    out.save("aligned/%s.png" % name)
    json.dump({"canvas_box": cb, "part": part, "part_box_canvas": [round(v * s) for v, s in zip(boxes[part]["part"], (SX, SY, SX, SY))],
               "warp": W.tolist(), "err_unchanged": round(float(e_keep), 2)}, open("aligned/%s.json" % name, "w"))
    chk = np.concatenate([crop, al, np.clip(err * 3, 0, 255)[..., None].repeat(3, 2)], 1)
    Image.fromarray(chk.astype(np.uint8)).resize((1536, 341), Image.LANCZOS).save("_check/%s.png" % name)
    print("%-18s err(unchanged) %.1f  warp scale %.3f %.3f  shift %.1f %.1f" % (name, e_keep, W[0, 0], W[1, 1], W[0, 2], W[1, 2]))


if __name__ == "__main__":
    want = sys.argv[1:]
    for p in sorted(glob.glob("gen/*_1.png")):
        name = os.path.basename(p)[:-6]
        if name in jobs and (not want or name in want): process(name, p)
