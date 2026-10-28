#!/bin/bash
# grab_view.sh <out png> [x y z pitch yaw]
cd "$(dirname "$0")/.."
X=${2:-0}; Y=${3:--9000}; Z=${4:-2500}; P=${5:--15}; W=${6:-90}
python -I - "$X" "$Y" "$Z" "$P" "$W" > scripts/_view.json <<PY
import json, math, sys, subprocess
x, y, z, p, w = map(float, sys.argv[1:6])
# FRotator(pitch, yaw, 0) -> quaternion
cp, sp = math.cos(math.radians(p) / 2), math.sin(math.radians(p) / 2)
cy, sy = math.cos(math.radians(w) / 2), math.sin(math.radians(w) / 2)
q = {"x": sp * sy, "y": -sp * cy, "z": cp * sy, "w": cp * cy}
print(json.dumps({"captureTransform": {"translation": {"x": x, "y": y, "z": z}, "rotation": q, "scale3D": {"x": 1, "y": 1, "z": 1}}, "annotations": None, "bShowUI": False}))
PY
python -I ue_mcp.py call EditorToolset.EditorAppToolset CaptureViewport "$(cat scripts/_view.json)" > scripts/_cap.json
python -I -c "
import json,base64,sys
d=json.load(open('scripts/_cap.json'))
def find(o):
    if isinstance(o,dict):
        for v in o.values():
            if isinstance(v,str) and len(v)>1000: return v
            r=find(v)
            if r: return r
    if isinstance(o,list):
        for v in o:
            r=find(v)
            if r: return r
b=find(d)
open(sys.argv[1],'wb').write(base64.b64decode(b.split(',')[-1])) if b else print(open('scripts/_cap.json').read()[:800])
" "$1"
