# three hardware options per model, half size, for the picker page
B="/c/Program Files/Blender Foundation/Blender 5.0/blender.exe"
run() { f=$1; tag=$2; shift 2; ( export STEEL_FINISH=$f STEEL_PCT=50 "$@"; "$B" --factory-startup -b -P blender/steel_3d.py -- final/steel3d/hw/${f}_$tag 48 >/dev/null 2>&1 ); python final/steel3d/comp.py final/steel3d/hw/${f}_$tag final/steel3d/hw/${f}_$tag.png; echo $f $tag; }
run rosewood A HW_KEYHEAD=satin HW_KEYS=alu HW_PICKUP=black HW_END=satin
run rosewood B HW_KEYHEAD=black HW_KEYS=alu HW_PICKUP=black HW_END=satin
run rosewood C HW_KEYHEAD=polished HW_KEYS=chrome HW_PICKUP=ring HW_END=chrome
run maple A HW_KEYHEAD=satin HW_KEYS=ivory HW_PICKUP=black HW_END=chrome
run maple B HW_KEYHEAD=polished HW_KEYS=ivory HW_PICKUP=ring HW_END=chrome
run maple C HW_KEYHEAD=satin HW_KEYS=ebony HW_PICKUP=chrome HW_END=nickel
run redsparkle A HW_KEYHEAD=satin HW_KEYS=alu HW_PICKUP=black HW_END=chrome
run redsparkle B HW_KEYHEAD=polished HW_KEYS=chrome HW_PICKUP=chrome HW_END=chrome
run redsparkle C HW_KEYHEAD=polished HW_KEYS=pearl HW_PICKUP=ring HW_END=chrome
run pearloid A HW_KEYHEAD=satin HW_KEYS=alu HW_PICKUP=black HW_END=satin
run pearloid B HW_KEYHEAD=polished HW_KEYS=pearl HW_PICKUP=ring HW_END=nickel
run pearloid C HW_KEYHEAD=black HW_KEYS=ivory HW_PICKUP=black HW_END=satin
run koa A HW_KEYHEAD=satin HW_KEYS=alu HW_PICKUP=black HW_END=satin
run koa B HW_KEYHEAD=black HW_KEYS=ebony HW_PICKUP=black HW_END=satin
run koa C HW_KEYHEAD=polished HW_KEYS=ivory HW_PICKUP=ring HW_END=nickel
