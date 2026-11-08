"""Preset browser card photos (pb2_2 style): HD, logo-free redraws of the 12 concept cards and new cards for the other
12 presets (same instrument and camera, a room that fits the preset). Inputs are padded to 3:2; the card is the middle
band (2.3:1) of each 1536x1024 output. Run with run_codex_images.py final/browser/jobs.json final/browser/gen."""
import json, os
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
NAMES = {1: "Sweet Georgia E9", 2: "Hedonist Bootgaze", 3: "Mikey's Grit Vibey (tweed breakup)", 4: "Montana Lush Swell",
         5: "Nashville 400 Crying Lead", 6: "Bakersfield Tape Slapback", 7: "Cosmic Bootgaze Dotted-8th",
         8: "Slow-Gear Dream Steel (ambient)", 9: "Cryin' In My Beer Ballad (honky-tonk)", 10: "Hot Tweed Roadhouse Grit",
         11: "Manual 10-String E9th Workstation", 12: "Studio Phrase Lick Machine", 13: "4-Track Cassette Steel",
         14: "Golden Hour Velvet", 15: "Dark Side Phase Steel", 16: "Desert Mirage Spring", 17: "Late-Night R&B Sample Bed",
         18: "Sunset Slack-Key Lounge", 19: "Apollo Ascent Shimmer (space-age)", 20: "Starlight Palm Chimes",
         21: "Sacred Gospel Leslie (church)", 22: "Blown Console Reverb-X (vintage studio console)",
         23: "Western Swing C6th Lounge (1940s dance hall)", 24: "Paris Texas Baritone (dusty desert roadhouse)"}
os.makedirs("in", exist_ok=True)
def pad(src, dst):   # card (2.3:1) centred on a 1536x1024 dark canvas
    im = Image.open(src).convert("RGB"); w = 1536; h = round(w * im.height / im.width)
    c = Image.new("RGB", (1536, 1024), (24, 20, 16)); c.paste(im.resize((w, h), Image.LANCZOS), (0, (1024 - h) // 2)); c.save(dst)
RULES = (" Output ONE photoreal landscape image 1536x1024. The pedal steel guitar is a real, photographed 10-string "
         "instrument (10 tuning keys, changer on the right, real pull rods, three foot pedals, knee levers), front-on, "
         "centred, filling about 80% of the width, all of it inside the middle 60% of the image height (the top and bottom "
         "20% are just floor and wall, they will be cropped). No text, letters, logos or brand names anywhere (amps, "
         "cases and gear stay plain). Warm, natural, slightly cinematic light; matte, believable surfaces.")
jobs = []
have = [1, 2, 4, 6, 8, 10, 13, 14, 15, 16, 17, 18]
for i, n in NAMES.items():
    if i in have:
        pad("card_%02d.png" % i, "in/card_%02d.png" % i)
        p = ("Redraw this small preset thumbnail as a sharp, high-detail photo with the SAME instrument, finish, room, "
             "props, camera and lighting (it is the card for the preset '%s')." % n) + RULES
        jobs.append({"name": "card_%02d" % i, "images": ["final/browser/in/card_%02d.png" % i], "prompt": p})
    else:
        p = ("Make the preset card photo for '%s' in the same style as this card: the SAME pedal steel model, walnut "
             "finish and front-on camera, in a different room that fits the preset's mood." % n) + RULES
        jobs.append({"name": "card_%02d" % i, "images": ["final/browser/in/card_01.png"], "prompt": p})
json.dump(jobs, open("jobs.json", "w"), indent=1); print(len(jobs), "jobs")
