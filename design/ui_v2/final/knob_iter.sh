#!/bin/bash
# One tuning iteration for a knob: render its region at 4x, then measure against APPROVED_main.png.
# usage: KNOB_SCALLOP='{"depth":0.16}' final/knob_iter.sh <style> <tag>
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
case $1 in
  scallop) RB="366.8,702,516.8,852"; FB="358,704,508,854"; DK="";;
  dome)    RB="72.7,717,192.7,837";  FB="71,719,191,839";  DK="";;
  space)   RB="672,710,824,862";     FB="658,712,808,864"; DK="dark";;
esac
mkdir -p final/knobs/iter
BORDER="$RB" "$B" -b -P blender/final_ui.py -- "$(pwd -W)/final/knobs/iter/$1_$2.png" 96 4.0 2>&1 | grep -E "Traceback|Error:|File \""
python final/knob_compare.py "final/knobs/iter/$1_$2.png" "$RB" "$FB" "final/knobs/iter/_cmp_$1_$2.png" $DK
