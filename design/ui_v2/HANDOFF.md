# Pedal Steel UI v2: handoff (2026-11-06)

Branch: `ui-v2-design`. Read `TASTE.md` first: it holds every decision and preference from Joshua, in order. `BRIEF.md` is the original brief.

## Where it stands

- **Knob correction follow-up (2026-11-06):** Joshua rejected the previous knob match and asked for realistic native rotation with correct moving highlights. `final/knobs/studio_rotation/` now contains a separate Cycles studio, an animated `.blend`, 181-frame transparent atlases, a motion preview, and a reference/previous/new comparison. Read its `README.md` for validation and limitations. **Pixel-perfect reference fidelity remains unachieved**, particularly for the Feel contour and shoulder lighting. The earlier “done” label below describes the prior export only, not user acceptance. The new candidate does not overwrite the existing strips or change the JUCE UI.

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
   - **2026-11-07 rework (reference-fitted shading, replaces `correct_strips.py`):** `final/render_matcap.sh` (~28 min). Blender renders per-frame data passes (`KNOB_PASS=normal|id|ao` in `final_ui.py` STRIP mode); `final/matcap_strips.py` fits a lit sphere per material (colour as a function of surface normal, times AO^g) to `APPROVED_main.png` at the reference angle and shades every frame with it, so highlights stay fixed while the geometry turns. Shapes refitted through that shading by `final/fit_matcap.py <style>` (superellipse lathe `sprofile`; results in `knob_lib.SUPER`/`RSCALE`; caches in `final/knobs/mfit/`). Layout: knobs sit on the reference's tick-ring centres (`REF_CENTRE`), ticks on the knob sight line (were ~2 px off) and sized to the reference (`TICK_SCALE`). `final/plate_fix.py` takes panel tone and knob halos from the reference (static, so safe). Knob-body error at the reference angle: VOICE 4.8, FEEL 4.4, SPACE 2.4 (was 11.2/11.7/7.9); deck check `final/deck_compare.py`. Gotcha: `pw` is superellipse power in `SUPER` but pointer width in `STYLES` (kept apart via `PROFKEYS`).
2. **Lock the per-preset instruments:** show Joshua round21 options on the board, then lock each chosen design to the exact x6_1 framing (Codex pass over a Blender lock render, then `recomp.py`), applying the realism checklist.
3. Resolve the More drawer and MIDI Learn with new concepts.
4. Then implement in the JUCE UI (`Source/`): background per preset, knob filmstrips, pedal/lever states, LED preset display.

  - Rim layer (2026-11-07, later): `matcap_strips.py` adds a screen-fixed residual indexed by (angle around the knob,
    signed distance to the current frame's own silhouette, -8..48 frame px). It carries the crisp top glare arc on FEEL,
    SPACE's wider top rim and the dark contact edge; outside the silhouette it bakes plate+residual with fading alpha.
    Angular smoothing widens with depth so lobe-specific mismatch doesn't ghost. `NORIM=1` turns it off. Also fixed a
    wrap-around in the matcap hole-fill that put a white fleck under SPACE. Body error at ref angle: dome 2.2, scallop 2.5, space 1.2.
  - SPACE shape (2026-11-07, later): the reference's black knob is a lit lobed top face plus a dark flared, lobed base
    (only its top catches light). `sprofile` got `fl` (base flare); SPACE is now fl=0.1, wb=1.5, depth=0.08, pw=2.5,
    RSCALE 1.0 (checked by drawing our silhouette over a brightened reference: final/knobs/_echo_outline.png). Colour-only
    scores can't see a black-on-dark outline, so don't trust fit_matcap for SPACE size. SPACE alignment now searches only
    +-2 px and ignores the pointer; its rim layer uses wide angular smoothing (SIGMIN 8) so lobe leftovers don't ghost.
  - Two-view fit + pointer on a lobe (2026-11-07, later): Joshua wants each pointer centred on a lobe tip, so FEEL and
    SPACE use phase=0 (overrides the reference's flute positions). `final/render_refs.sh` renders both reference knobs per
    style (<style>_ref*, <style>_ref2*); `matcap_strips.py` fits the lit sphere and the rim layer from both, which
    removed the patchy edges REVERB showed when shading came from ECHO's angle alone.
  - SPACE rebuilt + pointers (2026-11-07, later): SPACE is a domed cap on a lobed shoulder (`profile2` in knob_lib:
    rface 0.85, depth 0.14, RSCALE 1.08), outline checked against Joshua's traced outline
    (final/knobs/gpt/joshua_echo_outline.jpg, final/echo_overlay.py) and a ChatGPT mask of the ref (final/knobs/gpt/).
    No 2D rim layer on SPACE (NORIM_STYLES default "space"). Pointers are rounded bars on the cap (pointer_mesh) shaded
    with the other material's lit sphere: cream pointer <- FEEL body, dark pointers <- SPACE body (pointer_matcap).

## Whole-plugin rebuild from ChatGPT parts (2026-11-07, late)

Joshua asked for the entire plugin to match `APPROVED_main.png` pixel for pixel, with ChatGPT generating parts from the
reference. Pipeline (all in `final/parts/`, run in this order):

1. `make_jobs.py`, `make_jobs2..5.py` -> `jobs*.json`: 3:2 crops of the reference (upscaled to 1536x1024) with prompts that
   change one thing only (empty knob plates, pedals lit/unlit, empty display/meter/boxes, pointer-free cream domes, empty
   and full slider grooves, 16 HD tiles of the whole screen). Run with `python run_codex_images.py final/parts/jobsN.json final/parts/gen`.
   ChatGPT keeps the framing almost exactly (registration shifts < 1 px, unchanged areas within 1-3 levels).
2. `align_parts.py [names]`: ECC affine registration of each output onto its crop, tone match, written in the 3200x2000
   canvas (= reference resized, the strips' 2x grid). Check images in `_check/`.
3. `build_ui.py`: `bg_hd.png` (reference + HD tile detail, back-projected so it scales down to the reference within 0.1
   levels), `bg_clean.png` (every moving part removed; dome bodies kept without pointers), `sprites/` (pedal A/B/C on,
   meter on, slider handle, slider fill) with alpha from their difference to the clean plate.
4. `plate_knobs.py` -> `strips/plate_walnut.png` (reference outside each knob's sweep area, clean plate inside), then
   `matcap_strips.py scallop space` + `stitch_strips.py`. Knob draw positions now include the measured offset of each
   reference knob from its tick ring (`strips/offsets.json`). SPACE is the FEEL shape made narrower (9 rounded lobes, RSCALE 0.95).
5. `dome_pointers.py`: TONE/PICK/TRIM are symmetric domes, so the plugin rotates only their pointer, cut from the reference.
6. `fit_display.py` (dot pitch, per-dot glow kernel measured from the reference), `fit_text.py` (Poppins Medium for values,
   Inter Medium for the mode box, Poppins SemiBold for the dB readout; fonts in `design/ui_v2/fonts/`, OFL).
7. `compose.py`: draws the UI exactly as the editor does and diffs it with the reference (0.74 mean |err| at the
   reference's resolution). `gen_layout.py`: exports `Source/ui_assets/` + `Source/UiLayout.h` + the editor's constants.

JUCE: `Source/PluginEditor.*` is the new v2 editor (1600x1000 design space, scaled, fixed aspect). Mapping: TONE=toneWarmth,
PICK=pickSnap, GLIDE=barGlide, SWELL=autoSwell, ECHO=delayMix, REVERB=reverbMix, TRIM=outputLevel (0 dB points up),
VOLUME PEDAL=volumePedal, A/B/C/LKL/LKR toggles, MODE popup, preset arrows.

### Interactive steel, MIDI LEARN, UX (2026-11-07, later)

- More state parts (`make_jobs6.py`, `make_jobs7.py`): knee levers engaged (deck), MIDI LEARN lit, the tone bar removed,
  floor pedals pressed, under-neck knee levers pushed. `build_ui.py` turns them into sprites (`lever_L/R_on`, `midi_on`,
  `floor_A/B/C`, `kl_left/right_pushed`, `tone_bar`). The plate under the bar is rebuilt by `bar_plate.py` (row-wise:
  detail from two fret spaces to the left, levels from the neighbours), so the bar can be drawn anywhere on the neck.
- `gen_layout.py` also exports the neck geometry: fret -> x through the printed fret numbers (bar at fret 10 = the
  reference position), the 10 string rows, the vibrating length, hit areas for the neck and the pluck zone.
- Editor: the bar follows the engine's bar position (eased) and can be played: click a string on the neck (plays it at
  that fret), drag along to slide every held string, drag across to strum; click right of the neck to pluck at the bar.
  Strings past the bar shimmer with their energy. Floor pedals / knee levers / deck pedals ease in and out and also
  show what the engine does on its own (CC 65-69, keyswitches, mod wheel) while audio runs.
- MIDI LEARN (`Source/MidiLearn.h`): button toggles learn mode (amber), click a control to arm it, the first CC moved
  takes it; mapped CCs drive the parameter with gestures; saved in the state as `midiMap`. Right-click any control:
  MIDI Learn / forget its CC / reset to default / forget all. Esc leaves learn mode.
- Hover tooltips (0.6 s), preset menu on the display (factory, user presets, save, show folder), wheel on display/arrows
  steps presets, wheel on MODE steps modes, left/right arrow keys step presets.
- `PedalSteelShot` writes `renders/ui_screenshot.png` (design state), `_live.png`, `_learn.png`, `_tooltip.png` and runs
  23 headless interaction tests (synthetic mouse events + MIDI on the real editor/processor, menus through the
  editor's `menuHook`); all pass. It also writes `_menus.png` (the menus drawn with `NvaMenuLookAndFeel`,
  `Source/UiLookAndFeel.h`: ink panel, cream Poppins, amber highlight, LED dot for the current item).
- `PedalSteelHostTest` loads the built VST3 like a DAW (scan, instantiate, audio from MIDI, editor open/resize, state
  round trip incl. MIDI LEARN map, learned CC driving REVERB): 12 checks, all pass.
- Neck hover: the string under the mouse lights, an amber guide marks the fret the bar would land on, and the tooltip
  names the note ("String 3 (G#4) at fret 12 = G#5", open tuning of the current steel voice).
- Keyboard: Tab / Shift-Tab move an amber focus ring through display, knobs, pedals, levers, mode, slider, MIDI LEARN,
  trim; Up/Down/Left/Right nudge (Shift = fine), Page Up/Down = 10 %, Home/End = ends, Space/Enter presses pedals and
  buttons or opens menus; Esc drops the ring. Covered by 3 more tests (`_focus.png`).
- In tune on the neck: a click lands on the nearest fret; slides are pulled onto whole frets within 0.18 of one
  (Shift locks to frets).
- Display: a `*` after the name when a sound parameter differs from the loaded preset (pedals, levers, volume pedal and
  trim don't count); names longer than the glass (22 characters) scroll (`_marquee.png`).
- Typed values: double-click any value box (the six deck pills, the volume box, the dB readout) to type ("200 ms",
  "42 %", "-6.5 dB"); Enter/click away applies (clamped), Esc cancels (`_typein.png`).
- The bar at its home fret (10) draws `tone_bar_home` (the reference strip) so the design state is exact; it fades out as
  the bar leaves. Fret 10 is pinned to the reference bar's x in the fret map.
- The chosen secondary screens are built (`Source/Overlays.h`, drawn in design px with the editor's scale transform):
  - mode menu `mode_2`: click MODE; each mode with a one-line description and a check on the current one.
  - preset browser `pb2_2`: click the LED display (right-click = quick list). Filters All/Clean/Grit/Swell/Ambient/
    Vintage/User, search, 4-column grid of photo cards (`card_01..24.jpg`: HD, logo-free ChatGPT redraws of the 12
    concept cards plus 12 new ones; `design/ui_v2/final/browser/make_cards.py`), current preset outlined amber, click
    loads, double-click loads and closes, wheel scrolls, Save As. Keyboard: arrows move a selection through the grid,
    Space loads, Enter loads and closes, F stars, typing goes to search, Enter in search drops into the grid.
    Favourites: a star on hover (amber when starred), a Favourites filter, saved in
    `%APPDATA%/Northern Valley Audio/NVA Pedal Steel/favourites.txt`.
  - preset editor / save `pe1_2`: name, category, steel finish (walnut, maple, red sparkle, pearloid, koa; swatches
    cut from the concept; Black left out, Joshua called black finishes fake), notes, Cancel/Save; writes the user
    preset with `category`, `finish`, `card`, `notes` attributes, asks before replacing; the finish picks the instrument.
- Per-preset instruments (2026-11-08; Joshua: "whatever's best"): 04 = round21 i04_2 (birdseye maple), 06 = i06_1 (red
  sparkle, no badge), 13 = i13_2 (pearloid), 18 = i18_1 (koa, brass hardware); all other presets walnut. `make_jobs10.py`
  has ChatGPT restyle only the body on the approved steel crop (neck, strings, bar, pedals, rods, knee levers kept in
  place, so every interactive layer still lines up); `skins.py` turns each into `skin_<name>.jpg` over the walnut plate
  with the bar strip rebuilt. The editor bakes the current preset's instrument into its cached background and
  crossfades (0.35 s) on change; the bar's walnut reflection is drawn on walnut only. Browser cards 04/06/13/18 are
  crops of these instruments in the plugin's room; 02/10/15 were redrawn with the walnut steel. The koa steel has brass
  rods: `brass_sprites.py` makes brass versions of the pressed floor pedals / pushed knee levers (drawn on koa), and
  `skins.py` adds a fine mask under the neck so thin rods come from the instrument layer.
  - small window `small_1`: below 1100 px wide the editor draws small_1's bolder labels and boxes
    (`small_labels.py` -> `sprites/small_deck.png`, `small_top.png`; small_1 is registered to the reference within
    0.4 px, its value boxes emptied by ChatGPT, `make_jobs9.py`) and live values sized from small_1 (`txtS_*`,
    Poppins SemiBold). Tooltips keep at least ~10 screen px. `_small.png` = 1000x625 screenshot.
  - Overlays in small windows: the mode menu keeps >= 3/4 size and the save dialog >= 2/3 (centred, inside the window);
    the browser enlarges its text (x 0.66/scale, up to 1.7) and drops to 3 columns (`_small_overlays.png`, 800x500).
    Empty results say why ("No presets match", or how to make a user preset).
- Realism / feedback (2026-11-08): strings a pedal or lever retunes glow amber along their length (engine
  `stringBendSemis`; pedal A lights strings 5+10, B 3+6 on E9); the changer finger of each retuned string rocks up to
  up to 2 px at the hook's top (a skew about its base, in thin rows, so no seam) with a glint, drawn from the current
  instrument's pixels (`_changer_frames.png` shows it ramping in); pressed floor pedals slide into place at partial
  travel instead of cross-fading; turning a control shows "GLIDE 145 MS" on the LED display for 1.4 s; the display's
  right-click menu has "Revert to saved preset" when edited; the resize corner is drawn in the panel's cream.
- First-launch tip (once per user, `%APPDATA%/Northern Valley Audio/NVA Pedal Steel/ui_settings.xml` playHintDone):
  "Click a string on the neck to play it..." on the wood below the neck; playing the neck or its x dismisses it. The
  shot tool resets it for its check and restores the user's file afterwards.
- Accessibility: `Source/AccessibleControls.h` puts a click-through screen-reader element over every control
  (sliders with the panel's value text, toggles for pedals/levers/MIDI LEARN, buttons for preset/mode); values are
  announced on change.
- On-screen host test: `PedalSteelHostTest <vst3> --show-editor` opens the hosted editor (VST3 wrapper, native child
  window) on screen for 7 s while chords and CC 65/66 pedal changes play, resizing it from the host half way;
  `tools/hosttest/capture_window.ps1 "NVA Pedal Steel host window" out.png <delayMs>` grabs the real window
  (PrintWindow). Checked: it renders fully, the bar follows the notes, MIDI pedal changes light the pedals.
- pluginval (Tracktion, strictness 10, in-process) passes on the built VST3: thread safety, state, fuzzing, editor.
  One random seed (0x1cefe01) exposed a real state bug: a host can leave a bool/choice at an in-between raw value
  (0.3 = off) that survived setStateInformation; the processor now snaps discrete parameters after replaceState.
  That seed and three new random seeds pass.
- Fixed: the editor opened at its 800x500 minimum (setResizeLimits resized it first); default is 1280x800 and the
  last size is kept in the state (`editorWidth`).
