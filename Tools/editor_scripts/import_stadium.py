"""Imports the generated stadium meshes (Tools/art/meshes/*.obj, made by Tools/art/gen_stadium.py) into
/Game/PitchKings/Stadium. Run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/import_stadium.py
"""
import os
import unreal

DEST = "/Game/PitchKings/Stadium"
SRC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "art", "meshes")

tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".obj"):
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, f))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.get_editor_property("imported_object_paths"):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            b = a.get_bounding_box()
            unreal.log("[Stadium] %s min %s max %s mats %d" % (p, b.min, b.max, len(a.static_materials)))
            for m in a.static_materials:
                unreal.log("[Stadium]    slot %s" % m.material_slot_name)
        else:
            unreal.log("[Stadium] imported %s (%s)" % (p, type(a).__name__))
