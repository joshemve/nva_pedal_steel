"""Fit a knob style's geometry to APPROVED_main.png by coordinate search over renders.
python final/fit_knob.py <style> [rounds]
Each evaluation renders the knob region (final/knob_iter.sh) and scores it with knob_compare.py:
  score = (1 - IoU) * 100 + 0.15 * mean|lum diff|      (lower is better)
Results are cached in final/knobs/fit/<style>.json so reruns continue. Prints the best parameter set."""
import json, os, re, subprocess, sys

STYLE = sys.argv[1]; ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 2
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
CACHE = os.path.join(HERE, "knobs", "fit", STYLE + ".json"); os.makedirs(os.path.dirname(CACHE), exist_ok=True)
cache = json.load(open(CACHE)) if os.path.exists(CACHE) else {}

# parameter -> candidate values (geometry in knob_lib STYLES keys, plus rscale for size)
SPACES = {
    "scallop": {"hr": [0.45, 0.5, 0.55, 0.6, 0.65], "depth": [0.05, 0.06, 0.07, 0.08, 0.09, 0.1],
                "phase": [0.6875, 0.75, 0.8125], "rscale": [1.0, 1.0075, 1.015, 1.0225, 1.03]},
    "dome":    {"rscale": [0.975, 0.985, 0.995], "hr": [0.45, 0.5, 0.55, 0.6, 0.65]},
}
START = {"scallop": {"rscale": 1.015, "phase": 0.75, "lobes": 9, "depth": 0.09, "hr": 0.65},
         "dome": {"rscale": 0.985, "hr": 0.65}}[STYLE]


def evaluate(p):
    key = json.dumps(p, sort_keys=True)
    if key in cache: return cache[key]
    env = dict(os.environ)
    env["KNOB_RSCALE_" + STYLE.upper()] = str(p["rscale"])
    env["KNOB_" + STYLE.upper()] = json.dumps({k: v for k, v in p.items() if k != "rscale"})
    tag = "fit_" + "_".join("%s%s" % (k[:2], v) for k, v in sorted(p.items()))
    out = subprocess.run(["bash", "final/knob_iter.sh", STYLE, tag], cwd=ROOT, env=env, capture_output=True, text=True).stdout
    m = re.search(r"IoU=([\d.]+).*?inside=([\d.]+)", out, re.S)
    if not m: print("FAILED", p, out[-400:]); return 1e9
    iou, lum = float(m.group(1)), float(m.group(2))
    sc = round((1 - iou) * 100 + 0.15 * lum, 3)
    cache[key] = sc; json.dump(cache, open(CACHE, "w"), indent=0)
    print("  %-70s IoU=%.3f lum=%.1f score=%.2f" % (key, iou, lum, sc), flush=True)
    return sc


best = dict(START); best_s = evaluate(best)
for r in range(ROUNDS):
    for name, vals in SPACES[STYLE].items():
        for v in vals:
            cand = dict(best); cand[name] = v
            s = evaluate(cand)
            if s < best_s: best, best_s = cand, s
        print("round %d after %s: best %s score %.2f" % (r, name, json.dumps(best), best_s), flush=True)
print("BEST", json.dumps(best), best_s)
