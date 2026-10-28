import os, tempfile, unreal
for p in ("/MetaHumanCharacter/Optional/Grooms/GroomAssets/Hair/Hair_S_Casual/Hair_S_Casual_CardsAtlas_Tangent",
          "/Game/MHHeads/Build/Bruce/Grooms/Textures/Hair_S_Casual_CardsAtlas_Attribute"):
    t = unreal.load_asset(p)
    unreal.log("[Probe] %s comp %s srgb %s vt %s size %s" % (t.get_name(), t.get_editor_property("compression_settings"), t.get_editor_property("srgb"),
               t.get_editor_property("virtual_texture_streaming"), (t.blueprint_get_size_x(), t.blueprint_get_size_y())))
    task = unreal.AssetExportTask()
    task.object = t
    task.filename = os.path.join(tempfile.gettempdir(), t.get_name() + ".tga")
    task.automated = True; task.replace_identical = True; task.prompt = False
    unreal.Exporter.run_asset_export_task(task)
    unreal.log("[Probe] exported " + task.filename)
