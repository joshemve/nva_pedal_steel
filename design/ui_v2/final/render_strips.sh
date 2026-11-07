#!/bin/bash
# Knob filmstrips (N frames, -135..+135 deg, 2x) rendered in place in the walnut scene, the knob-less plate,
# one reference-angle frame per style, then per-pixel correction toward APPROVED_main.png and stitching.
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
N=${N:-128}
mkdir -p final/strips/frames
declare -A REFANG=([dome]=35 [scallop]=30 [space]=-100)
NO_KNOBS=1 "$B" -b -P blender/final_ui.py -- "$(pwd -W)/final/strips/plate_walnut.png" 128 2.0 2>&1 | grep -E "Traceback|Error:" &
for s in dome scallop space; do
  STRIP="$s:1" STRIP_ANGLE=${REFANG[$s]} "$B" -b -P blender/final_ui.py -- "$(pwd -W)/final/strips/frames/${s}_ref" 128 2.0 2>&1 | grep -E "Traceback|Error:"
done
wait
for s in dome scallop space trim; do   # two at a time
  STRIP="$s:$N" "$B" -b -P blender/final_ui.py -- "$(pwd -W)/final/strips/frames/$s" 64 2.0 2>&1 | grep -E "Traceback|Error:" &
  if [ "$s" = "scallop" ]; then wait; fi
done
wait
python final/correct_strips.py dome scallop space
cp final/strips/frames/trim_* final/strips/frames/trim.json final/strips/frames_fixed/ 2>/dev/null
python final/stitch_strips.py
