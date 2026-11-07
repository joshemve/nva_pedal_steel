# Pedal Steel UI v2: handoff (2026-11-06)

Branch: `ui-v2-design`. Read `TASTE.md` first: it holds every decision and preference from Joshua, in order. `BRIEF.md` is the original brief.

## Where it stands

- **Approved main screen:** `APPROVED_main.png` (= `round26/x6_1_sp_c_2.png`). Layout x6_1: cream top bar (serif "Pedal Steel" wordmark left, big single-line amber LED preset name dead centre with arrows, OUTPUT meter + TRIM right, no SAVE, no tabs); the steel lying horizontal and front-on, centred, ~85% of the width, on a Persian rug in a warm studio; one black walnut-trimmed control deck: VOICE (TONE, PICK: cream domes), FEEL (GLIDE, SWELL: scalloped cream), SPACE (ECHO, REVERB: black scalloped), A B C pedal blocks (A, B lit amber), LKL LKR knee levers, strip with MODE, VOLUME PEDAL slider, MIDI LEARN.
- **Other screens chosen** (choice board, below): preset browser `round18/pb2_2` (cards with skin photos), preset editor `round18/pe1_2` (saving lives here), mode menu `round18/mode_2`, small window `round18/small_1`.
- **Rejected / still open:** the More drawer for Motion + Effects (rejected both round18 versions as "separate, not well thought out"; round19 ideas `d1`-`d4` not picked yet, `d4` taller one-deck console was the recommendation) and MIDI Learn (round18 rejected; round19 `mi1`-`mi3` not picked, `mi1` right-click-to-learn recommended).
- **Preset skins:** Joshua kept 04 Montana maple, 06 Bakersfield red flake, 13 pearloid, 18 koa (plus 01 walnut); 02 black-violet and 15 dark-anodized felt "cheesy/fake". He then asked for a *different instrument design per preset*, not the same box recoloured: round21 (`comp/i*.png`) has them; not yet picked.

## Joshua's rules (short form; full list in TASTE.md)

No orange accents (amber only for LEDs/meter). Matte panels, no gloss. Cream-dominant, calm, uncluttered, legible from across a room. One camera, never mixed viewpoints; knobs/dials straight-on. Steel at true proportions, never stretched wide or flat. Controls form one cohesive deck. **Realism rule:** every steel must look like a real photographed instrument (10 strings + 10 keys, changer on the right, correct fret spacing, real pull rods and knee levers, real wear, no invented nameplates such as the "BAKERSFIELD" badge in round21). He likes many options presented visually and picked on the choice board, and wants work kept moving in parallel.

## Tools and how they're used

- **Codex image generation** (ChatGPT image model, headless): `python run_codex_images.py <jobs.json> <out_dir>` runs jobs in parallel; each job is `{"name","prompt","images":[refs]}`. Codex CLI path: newest `%LOCALAPPDATA%\OpenAI\Codex\bin\*\codex.exe`. Images land in `~/.codex/generated_images/<thread>/`, copied to `<out_dir>/<name>_N.png`; logs in `<out_dir>/logs/`. Each `roundN/jobs.json` keeps the exact prompts. Joshua's preference: send the plugin image as the base and let ChatGPT design new parts from a written description; don't paste part images in. `SPEC_PROMPT.md` is a full text spec of the approved screen if a from-scratch generation is needed.
- **Blender 5.0.1** headless at `C:/Program Files/Blender Foundation/Blender 5.0/blender.exe` (RTX 4090, OptiX). `blender/final_ui.py -- <out.png> [samples] [res] [elev] [skin]` renders the x6_1 layout with exact geometry (`steel_lib.py` instrument, `ui_lib.py` helpers) and a skin switch (walnut, maple, redsparkle, koa, pearloid). LED textures are pre-built by `blender/make_led.py` (Blender's Python has no PIL). `final/render_skins.sh` renders all skins; `final/recomp.py` composites the exact Blender top bar + deck over the photoreal steel images into `comp/`.
- **Choice board artifact:** https://claude.ai/artifact/BkiKx3tPDwjMAp8CBB51Wt. Joshua's picks are in its db collection `choices` (read with ArtifactData list). Source page: scratchpad copy only, so rebuild it from this repo if needed.
- Images are in Git LFS; `.blend` files are gitignored (scripts regenerate them).

## Next steps (in order)

1. ~~Knobs in Blender~~ **done (2026-11-06), fitted to `APPROVED_main.png`:**
   - `blender/knob_lib.py`: dome (VOICE, cream on knurled skirt), scallop (FEEL: 9 soft concave flutes, flat face, flute phase 0.75), space (black scalloped, also TRIM). Geometry was fitted by `final/fit_knob.py` (render + score loop over size/height/flute depth/phase; IoU ~0.97 vs the reference). Joshua's notes: head-on, soft (not creased) concave edges, top-lit shading; the pointer line need not be exact.
   - `blender/final_ui.py`: each knob sits on its own sight line with its axis aimed at the camera (no parallax side-walls); printed ticks stay flat on the panel. Deck knobs are lit only by their own soft top strip light (Cycles light linking), excluded from room/UI lights; per-style power/angle in the `knob_light` loop.
   - Tools: `final/knob_iter.sh <style> <tag>` renders one knob region and runs `final/knob_compare.py` (size, IoU, colour, shading zones top/bottom/centre/rim vs the reference). Env hooks: `KNOB_<STYLE>='{json}'`, `KNOB_RSCALE_<STYLE>`, `KNOB_KEY[_<STYLE>]`, `KNOB_KEY_EL[_<STYLE>]`, `KNOB_FILL`, `BORDER`, `STRIP`, `STRIP_ANGLE`, `NO_KNOBS`.
   - Filmstrips: `final/render_strips.sh` (plate, reference-angle frames, 128-frame strips -135..+135, correction, stitch) -> `final/strips/knob_<style>.png` + `strips.json` (frame size, scale 2x, knob centres in 1600x1000 UI px; draw each frame centred on its knob) and `plate_walnut.png` (background with ticks and knob shadows, no knob bodies). `final/correct_strips.py` multiplies every frame by a screen-fixed lighting/colour ratio (ref vs render, blurred) and a ring-averaged term, both safe under rotation; the angle-specific detail is NOT applied (it made the light swing). Knob-body error vs the reference: VOICE 17.5->11.2, FEEL 21.6->11.7, SPACE 18.1->7.9 (0-255). Checks: `final/knobs/_deck_from_strips.png`, `_strip_angles.png`.
2. **Lock the per-preset instruments:** show Joshua round21 options on the board, then lock each chosen design to the exact x6_1 framing (Codex pass over a Blender lock render, then `recomp.py`), applying the realism checklist.
3. Resolve the More drawer and MIDI Learn with new concepts.
4. Then implement in the JUCE UI (`Source/`): background per preset, knob filmstrips, pedal/lever states, LED preset display.
