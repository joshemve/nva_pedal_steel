"""Deck control concepts (Joshua 2026-11-09, after the first Blender pass: "use ChatGPT image concept first because these
aren't spaced right or feeling correct"). Four directions x2 on the approved deck crop (deck_crop.png); only the
A/B/C pedals and LKL/LKR levers change. Winner then gets rebuilt in blender/deck_3d.py to match."""
import json, os
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
Image.open("../../round7/p01_2.png").convert("RGB").crop((880, 660, 1530, 880)).save("concepts/p01_2_cluster.png")
SAME = ("This is a crop of a photoreal audio-plugin UI for a pedal steel guitar instrument: the right half of its bottom "
        "control deck (dark matte leather panel, walnut trim, Persian rug above). Output ONE photoreal image, landscape "
        "1536x1024, with EXACTLY the same framing, scale, camera, lighting, colours, panel, labels A B C LKL LKR, divider "
        "lines, lamp lenses, slider and MIDI LEARN button as the input. Redesign ONLY the three foot-switch pedals under "
        "A, B, C and the two knee-lever controls under LKL, LKR. ")
SPACING = (" Layout rules: A, B and C are three separate pedals of identical size, evenly spaced with clear leather "
           "between them (gap about a third of a pedal's width), each centred exactly under its letter and lamp, the "
           "group centred in its bay between the divider lines. LKL and LKR are true mirror images of each other "
           "(LKL leans/points to the left, LKR to the right), each centred under its label, the pair centred in its bay. "
           "Show A and B engaged (their lamps lit warm amber, the pedal visibly pressed down/tilted) and C at rest "
           "(lamp unlit); show LKL engaged (swung outward) and LKR at rest, so on and off can be read at a glance. "
           "Real materials only: matte black ribbed rubber, satin brushed chrome, no glossy plastic, no orange glow "
           "spill on the leather, no text other than the existing labels. It must look like a real photographed "
           "piece of hardware made by a boutique pedal steel builder (Emmons, Sho-Bud, Fender 400 era), not a "
           "render or an icon. Everything outside the pedals and levers must stay identical to the input.")
D = {
 "refine": "Keep the same idea as the input (rubber-padded pedal blocks in chrome frames, chrome lever rods ending in "
           "small ribbed paddles) but make them feel like real, solid, heavy hardware with correct proportions, crisp "
           "rib detail, real depth and contact shadows on the leather.",
 "replica": "Make A, B and C miniature replicas of real pedal-steel FOOT PEDALS as seen from above at the deck's angle: "
            "a rounded-top black rubber pad with vertical ribs on a cast chrome pedal body hinged on a short chrome "
            "axle at its top end. Make LKL and LKR miniature replicas of real pedal-steel KNEE LEVERS: a chrome mounting "
            "block with a short chrome L-shaped rod ending in a black rubber knee pad, pivoting sideways.",
 "p01": "Use the SECOND input image (a reference I like) as the design for the controls: chunky dark rubber-padded pedal "
        "blocks with chrome bezels and amber-lit tops when engaged, and chrome knee-lever paddles on angled rods from a "
        "ring mount. Recreate that design faithfully on the dark deck of the first image, at the first image's scale.",
 "gate": "Seat each of A, B, C in its own recessed chrome-bezelled well cut into the leather, so the pressed pedal sinks "
         "into the well. Mount LKL and LKR as a chrome bat lever moving in a short horizontal slotted chrome gate "
         "plate, the slot showing the lever's travel left-right, with a small black ribbed paddle tip.",
}
jobs = []
for k, d in D.items():
    imgs = ["final/deck3d/deck_crop.png"] + (["final/deck3d/concepts/p01_2_cluster.png"] if k == "p01" else [])
    for n in (1, 2):
        jobs.append({"name": "dc_%s_%d" % (k, n), "images": imgs, "prompt": SAME + d + SPACING})
json.dump(jobs, open("concepts/jobs.json", "w"), indent=1); print(len(jobs), "jobs")
