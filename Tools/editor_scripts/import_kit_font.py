"""Imports Tools/art/T_PK_KitFont.png (signed-distance kit lettering, make_kit_font.py) into /Game/PitchKings/Materials/Player.
Run headless before make_player_mh.py."""
import os
import unreal

art = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "art")
t = unreal.AssetImportTask()
t.filename = os.path.join(art, "T_PK_KitFont.png")
t.destination_path = "/Game/PitchKings/Materials/Player"
t.destination_name = "T_PK_KitFont"
t.replace_existing = True
t.automated = True
t.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
for path in t.imported_object_paths:
    tex = unreal.load_asset(path)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)  # uncompressed G8: clean edges
    tex.set_editor_property("virtual_texture_streaming", False)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    unreal.log("[KitFont] imported " + path)
