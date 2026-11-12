cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
r(){ out=$1; shift; ( export STEEL_PCT=50 "$@"; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/uvt/$out 64 2>&1 | grep -iE "Traceback|Error" ); python final/steel3d/comp.py final/steel3d/uvt/$out final/steel3d/uvt/$out.png; }
r koa_d3 STEEL_FINISH=koa CAB_VAL=0.48 FRONT_GLINT=0.5
r koa_d4 STEEL_FINISH=koa CAB_VAL=0.36 CAB_SAT=1.0 FRONT_GLINT=0.4
r rs_d3 STEEL_FINISH=redsparkle CAB_UV=0 RS_DARK=0.35 RS_HI=1.4 TOP_GLINT=1.8 FRONT_GLINT=1.0
r rs_d4 STEEL_FINISH=redsparkle CAB_UV=0 RS_DARK=0.25 RS_HI=1.1 TOP_GLINT=1.5 FRONT_GLINT=0.8
echo done
