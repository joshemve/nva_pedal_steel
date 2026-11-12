cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
r(){ out=$1; shift; ( export STEEL_PCT=50 "$@"; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/uvt/$out 64 2>&1 | grep -iE "Traceback|Error" ); python final/steel3d/comp.py final/steel3d/uvt/$out final/steel3d/uvt/$out.png; }
r rs_e3 STEEL_FINISH=redsparkle HL=6.0 HL_CORE=1.5 HL_GAMMA=1.0
r pl_e3 STEEL_FINISH=pearloid HL=3.0 HL_GAMMA=1.0 HL_TEXP=6 TILE_GLINT=2.0 CAB_GAMMA=1.25 CAB_SAT=0.8
