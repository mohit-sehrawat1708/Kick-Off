"""
Updates the shirt colours of the 3D fans in place: rewrites M_PK_FanVAT's colour node from build_crowd3d.FAN_COLOR without
re-baking the crowd (build_crowd3d.py makes the same graph from scratch). Also drops inputs the colour code no longer
uses (a collection parameter created from Python never resolved its id).

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/patch_fan_colour.py
"""
import os
import re
import unreal

MEL = unreal.MaterialEditingLibrary
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "build_crowd3d.py"), encoding="utf-8").read()
FAN_COLOR = re.search(r'FAN_COLOR = r"""(.*?)"""', src, re.S).group(1)
KEEP = ("Kind", "Team", "Seed", "Tone", "HomeA", "AwayA", "HairMap", "BrowMap", "BeardMap")
path = "/Game/PitchKings/Crowd3D/M_PK_FanVAT"
mat = unreal.load_asset(path)
node = None
for k in range(12):  # expressions are subobjects named by class and index
    x = unreal.load_object(None, "%s.M_PK_FanVAT:MaterialExpressionCustom_%d" % (path, k))
    if x and "club" in x.get_editor_property("code"):
        node = x
ins = [i for i in node.get_editor_property("inputs") if str(i.get_editor_property("input_name")) in KEEP]
node.set_editor_property("inputs", ins)
# (the stray collection-parameter nodes stay in the graph, unconnected: deleting expressions asserts in 5.8)
# re-bind the club colour inputs by name (their ids go stale if the collection is ever rebuilt)
mpc = unreal.load_asset("/Game/PitchKings/Materials/MPC_PK_Stadium")
for k, nm in enumerate(("HomeA", "AwayA")):
    x = unreal.load_object(None, "%s.M_PK_FanVAT:MaterialExpressionCollectionParameter_%d" % (path, k))
    x.set_editor_property("collection", mpc)
    x.set_editor_property("parameter_name", nm)
node.set_editor_property("code", FAN_COLOR)
MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
unreal.log("[FanColour] patched %s inputs %s" % (path, [str(i.get_editor_property("input_name")) for i in node.get_editor_property("inputs")]))
