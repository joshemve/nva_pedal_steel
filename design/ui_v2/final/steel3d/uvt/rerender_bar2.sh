cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
for f in koa pearloid maple; do ( export STEEL_MODE=static STEEL_FINISH=$f; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/finish_$f 160 2>&1 | grep -iE "Traceback|Error" ); echo $f done; done
( export STEEL_MODE=layers; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/layers 160 2>&1 | grep -iE "Traceback|Error" ); echo layers done
