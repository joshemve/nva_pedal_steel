#!/bin/bash
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
for s in walnut maple redsparkle koa pearloid; do
  "$B" -b -P blender/final_ui.py -- "$(pwd -W)/final/skin_$s.png" 96 1.0 58 $s 2>&1 | grep -E "Traceback|Error|Saved:" | grep -v "Failed to remove"
done
