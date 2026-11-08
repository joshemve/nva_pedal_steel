#!/bin/bash
# Knob strips shaded from the reference (lit-sphere fit). Run from anywhere; ~5 min.
#  1. plate (NO_KNOBS, 128 spp) -> plate_walnut_raw.png, then plate_fix.py (panel tone + knob halos from the reference)
#  2. colour frame at the reference angle per style (alignment only) + normal/id/ao passes at that angle
#  3. normal/id/ao passes for the full N-frame sweep, matcap_strips.py -> frames_fixed/, stitch, deck check
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
N=${N:-128}; D=final/strips/frames; mkdir -p $D
declare -A REFANG=([dome]=35 [scallop]=30 [space]=-100)
NO_KNOBS=1 "$B" -b -P blender/final_ui.py -- "$(pwd -W)/final/strips/plate_walnut_raw.png" 128 2.0 2>&1 | grep -E "^[A-Za-z]*Error:" &
for s in dome scallop space; do
  STRIP="$s:1" STRIP_ANGLE=${REFANG[$s]} "$B" -b -P blender/final_ui.py -- "$(pwd -W)/$D/${s}_ref" 64 2.0 2>&1 | grep -E "^[A-Za-z]*Error:" &
done
wait
STRIP="trim:$N" "$B" -b -P blender/final_ui.py -- "$(pwd -W)/$D/trim" 64 2.0 2>&1 | grep -E "^[A-Za-z]*Error:" &
REFONLY=1 bash final/render_passes.sh 2>&1 | grep -E "^[A-Za-z]*Error:"
N=$N bash final/render_passes.sh 2>&1 | grep -E "^[A-Za-z]*Error:"
wait
python final/plate_fix.py
python final/matcap_strips.py dome scallop space
cp $D/trim_[0-9]*.png $D/trim.json final/strips/frames_fixed/
python final/stitch_strips.py
python final/deck_compare.py
