"""New instrument references (Joshua 2026-11-10: "not liking these references fully, make better pedal steel
references"). The old five read as lap steels on tables. Whole current screen in, only the instrument changes, each
direction after a real builder's design. Approved picks become the targets for the Blender models."""
import json, os
H = os.path.dirname(os.path.abspath(__file__)); os.chdir(H)
KEEP = ("This is the full UI of a photoreal pedal steel guitar audio plugin. Keep proportions and everything the same: "
        "output ONE image, landscape 1536x1024, identical to the input in framing, camera angle, lighting, the rug and "
        "room, the cream top bar, and the whole dark bottom control deck with every knob, label and switch. Keep the "
        "black fretboard with its coloured markers and fret numbers, the ten strings, the chrome tone bar and the "
        "pickup in exactly the same place and size. Change ONLY the instrument around them: redraw it as a REAL, "
        "photographed, professional 10-string single-neck PEDAL STEEL GUITAR, not a lap steel on a table. Real anatomy: "
        "a cast polished-aluminium keyhead at the left with exactly ten tuning machines, five along each side, the "
        "strings passing over a roller nut; at the right a real changer with ten fingers; polished aluminium endplates "
        "at both ends of the cabinet; a raised neck under the fretboard; a deep front apron; four slim chrome "
        "telescoping legs screwed into the endplates; a chrome pedal bar clamped between the two front legs near the "
        "floor carrying three foot pedals, each joined to the underside of the cabinet by a thin straight pull rod; two "
        "knee levers hanging under the cabinet. Understated, tasteful, slightly worn from real use, no logos or text on "
        "the instrument, nothing shiny-plastic or toy-like. ")
D = {
 "emmons": "Design after a 1960s Emmons push-pull: black lacquered cabinet with thin aluminium trim along the front, "
           "polished aluminium neck, squared endplates.",
 "shobud": "Design after a 1970s Sho-Bud: natural lacquered birdseye maple cabinet with a dark inlaid stripe along the "
           "front apron, a maple wood neck, gently rounded front edge, rounded polished endplates.",
 "rosewood": "Design after a modern boutique steel (Mullen, Zum style): dark rosewood-grain cabinet with a satin "
             "finish, slim polished aluminium neck and endplates, very clean lines.",
 "fender": "Design after a late-1950s Fender 400: blonde ash cabinet with a chrome control plate, chrome bridge cover "
           "over the changer, a long chrome frame keyhead, legs slightly splayed.",
}
jobs = [{"name": "ref_%s_%d" % (k, n), "images": ["final/steel3d/in_main.png"], "prompt": KEEP + d}
        for k, d in D.items() for n in (1, 2)]
json.dump(jobs, open("jobs_refs.json", "w"), indent=1); print(len(jobs))
