"""Round 2 of the knee-lever before/after (Joshua: the engaged levers swing to the outside, which makes the spacing look
weird). All on before_3 (the pick). in: engaged swings toward the centre; small: same place, short travel; vert: rest
hangs straight down centred under its label, engaged tilts a little (needs vert_before first, then vert_after)."""
import json, os, sys, glob
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
KEEP = ("This is the full UI of a photoreal pedal steel guitar audio plugin. Keep proportions and everything the same: "
        "output ONE image, landscape 1536x1024, identical to the input in framing, layout, scale, lighting, colours and "
        "every detail. ")
SPACE = (" The spacing must stay balanced: each lever's paddle stays centred under its LKL / LKR label, the pair stays "
         "evenly spaced and centred in its section, well clear of the divider line and the panel edge. Nothing else "
         "changes.")
ENG = "Also light the A, B and C lamps warm amber and press A, B and C down slightly into the panel (no glow on the panel). "
B3 = "final/deck3d/ba/gen/before_3_1.png"
J = {
 "in": KEEP + ENG + "Show the LKL and LKR knee levers ENGAGED by swinging each one INWARD, toward the centre between "
       "them (LKL's paddle moves right, LKR's paddle moves left), a short travel of about 20 degrees on its pivot ring." + SPACE,
 "small": KEEP + ENG + "Show the LKL and LKR knee levers ENGAGED with only a small, tidy movement: each lever rotates "
          "about 12 degrees on its pivot ring and the paddle stays almost in the same place, visibly pushed (slightly "
          "lower and nearer the panel, tighter shadow)." + SPACE,
 "vert": KEEP + "Change ONLY the LKL and LKR knee levers: make each hang straight down from its pivot ring, perfectly "
         "vertical, the paddle directly below the ring and centred under its label, both identical. This is the rest "
         "position; A, B and C stay exactly as they are." + SPACE,
}
step = sys.argv[1]
if step == "1":
    jobs = [{"name": "r2_%s_%d" % (k, n), "images": [B3], "prompt": p} for k, p in J.items() for n in (1, 2)]
else:
    VA = (KEEP + ENG + "Show the LKL and LKR knee levers ENGAGED: each tilts about 20 degrees on its pivot ring, LKL's "
          "paddle swinging a little to the left and LKR's a little to the right, the travel kept short." + SPACE)
    jobs = [{"name": os.path.basename(f)[:-6].replace("vert", "vertA"), "images": ["final/deck3d/ba/gen/" + os.path.basename(f)],
             "prompt": VA} for f in sorted(glob.glob("ba/gen/r2_vert_*_1.png"))]
json.dump(jobs, open("ba/jobs_r2_%s.json" % step, "w"), indent=1); print(len(jobs))
