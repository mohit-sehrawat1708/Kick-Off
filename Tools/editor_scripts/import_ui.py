"""Imports Tools/art/*.png into /Game/PitchKings/UI as UI textures (run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/import_ui.py)."""
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
art = os.path.join(project, "Tools", "art")
tasks = []
for name in ("PK_Logo.png", "PK_Crest.png"):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(art, name)
    t.destination_path = "/Game/PitchKings/UI"
    t.destination_name = "T_" + os.path.splitext(name)[0]
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for path in t.imported_object_paths:
        tex = unreal.load_asset(path)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        unreal.log("PK UI imported " + path)
