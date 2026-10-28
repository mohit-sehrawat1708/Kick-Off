"""Imports the stadium textures made by Tools/art (sponsor wordmark atlas) into /Game/PitchKings/Stadium. Run headless
before make_stadium_materials.py."""
import os
import unreal

art = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "art")
t = unreal.AssetImportTask()
t.filename = os.path.join(art, "T_PK_Sponsors.png")
t.destination_path = "/Game/PitchKings/Stadium"
t.destination_name = "T_PK_Sponsors"
t.replace_existing = True
t.automated = True
t.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
for path in t.imported_object_paths:
    tex = unreal.load_asset(path)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    unreal.log("[StadiumTex] imported " + path)
