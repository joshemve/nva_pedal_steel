cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
r(){ out=$1; shift; ( export STEEL_PCT=50 "$@"; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/uvt/$out 64 2>&1 | grep -iE "Traceback|Error" ); python final/steel3d/comp.py final/steel3d/uvt/$out final/steel3d/uvt/$out.png; }
r koa_d1 STEEL_FINISH=koa CAB_VAL=0.80 FRONT_GLINT=0.8
r koa_d2 STEEL_FINISH=koa CAB_VAL=0.64 FRONT_GLINT=0.6
r rs_d1 STEEL_FINISH=redsparkle CAB_UV=0 RS_DARK=0.70 RS_HI=2.0 TOP_GLINT=2.6 FRONT_GLINT=1.6
r rs_d2 STEEL_FINISH=redsparkle CAB_UV=0 RS_DARK=0.50 RS_HI=1.7 TOP_GLINT=2.0 FRONT_GLINT=1.2
echo done
