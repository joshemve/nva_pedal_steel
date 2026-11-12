cd /c/Users/Joshua/Documents/pedal_steel/design/ui_v2
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
r(){ out=$1; shift; ( export STEEL_PCT=50 "$@"; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/uvt/$out 32 2>&1 | grep -iE "Traceback|Error" ); python final/steel3d/comp.py final/steel3d/uvt/$out final/steel3d/uvt/$out.png
 python -c "
from PIL import Image
im=Image.open('final/steel3d/uvt/$out.png').convert('RGB').resize((1200,750)); c=im.crop((40,80,1160,440)); c.resize((1000,321)).save(r'C:\Users\Joshua\AppData\Local\Temp\claude\b29f79ff5003fadb\2ca7bf9a-2051-4091-9c79-e71084859de9\scratchpad\round3\$out.jpg',quality=86)"; }
r koa_g1 STEEL_FINISH=koa CAB_SAT=1.25 CAB_VAL=0.95 CAB_COAT=0.10 FRONT_GLINT=0.6
r koa_g2 STEEL_FINISH=koa CAB_SAT=1.6 CAB_VAL=1.0 CAB_COAT=0.06 FRONT_GLINT=0.5
r rs_g1 STEEL_FINISH=redsparkle CAB_UV=0 RS_DARK=0.6 RS_HI=2.0 FLAKE_COAT=0.35 TOP_GLINT=1.6 FRONT_GLINT=0.8
r rs_g2 STEEL_FINISH=redsparkle CAB_UV=0 RS_DARK=0.8 RS_HI=2.2 FLAKE_COAT=0.15 TOP_GLINT=1.4 FRONT_GLINT=0.6
