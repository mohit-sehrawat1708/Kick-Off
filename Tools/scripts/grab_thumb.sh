#!/bin/bash
# grab_thumb.sh <asset path> <out png>
cd "$(dirname "$0")/.."
python -I ue_mcp.py call EditorToolset.EditorAppToolset CaptureAssetImage "{\"assetPath\":\"$1\"}" > scripts/_thumb.json
python -I -c "
import json,base64,sys
d=json.load(open('scripts/_thumb.json'))
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
open(sys.argv[1],'wb').write(base64.b64decode(b.split(',')[-1])) if b else print(open('scripts/_thumb.json').read()[:500])
" "$2"
