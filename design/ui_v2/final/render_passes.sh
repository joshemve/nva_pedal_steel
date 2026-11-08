#!/bin/bash
# Data passes (normal, id, ao) for knob strips: ref angle only (REFONLY=1) or the full N-frame sweep.
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
N=${N:-128}; D=final/strips/frames; mkdir -p $D
declare -A REFANG=([dome]=35 [scallop]=30 [space]=-100)
for s in ${STYLES:-dome scallop space}; do
  for p in normal id ao; do
    if [ -n "$REFONLY" ]; then
      KNOB_PASS=$p STRIP="$s:1" STRIP_ANGLE=${REFANG[$s]} "$B" -b -P blender/final_ui.py -- "$(pwd -W)/$D/${s}_ref_$p" 16 2.0 2>&1 | grep -E "Traceback|Error:" &
    else
      KNOB_PASS=$p STRIP="$s:$N" "$B" -b -P blender/final_ui.py -- "$(pwd -W)/$D/${s}_$p" 16 2.0 2>&1 | grep -E "Traceback|Error:" &
    fi
  done
  wait
done
