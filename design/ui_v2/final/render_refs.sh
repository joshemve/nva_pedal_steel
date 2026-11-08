#!/bin/bash
# Reference-angle renders for both reference knobs of each style (colour + normal/id/ao), used to fit the shading.
# <style>_ref* = first knob's pointer angle, <style>_ref2* = second knob's. STYLES="..." to limit.
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
D=final/strips/frames; mkdir -p $D
declare -A A1=([dome]=35 [scallop]=30 [space]=-100) A2=([dome]=-50 [scallop]=25 [space]=-68)
for s in ${STYLES:-dome scallop space}; do
  for v in 1 2; do
    if [ $v = 1 ]; then a=${A1[$s]}; t=${s}_ref; else a=${A2[$s]}; t=${s}_ref2; fi
    STRIP="$s:1" STRIP_ANGLE=$a "$B" -b -P blender/final_ui.py -- "$(pwd -W)/$D/$t" 64 2.0 2>&1 | grep -E "Traceback|Error:" &
    for p in normal id ao; do
      KNOB_PASS=$p STRIP="$s:1" STRIP_ANGLE=$a "$B" -b -P blender/final_ui.py -- "$(pwd -W)/$D/${t}_$p" 16 2.0 2>&1 | grep -E "Traceback|Error:" &
    done
    wait
  done
done
