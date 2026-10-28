import unreal
def log(s): unreal.log("[Probe] " + str(s))
for g in ("Hair_S_Casual", "Beard_M_MuttonChops", "Mustache_S_Horseshoe", "Eyebrows_M_FlatThick"):
    a = unreal.load_asset("/Game/MHHeads/Build/Bruce/Grooms/" + g)
    log("%s %s" % (g, type(a).__name__))
    try:
        cards = a.get_editor_property("hair_groups_cards")
    except Exception as ex:
        log("  no cards prop %s" % ex); continue
    for i, c in enumerate(cards):
        try:
            t = c.get_editor_property("textures")
            log("  card %d lod %s layout %s" % (i, c.get_editor_property("lod_index"), t.get_editor_property("layout")))
            for j, tx in enumerate(t.get_editor_property("textures")):
                log("    tex %d %s" % (j, tx.get_path_name() if tx else None))
        except Exception as ex:
            log("  card %d %s / %s" % (i, ex, [p for p in dir(c) if not p.startswith("_")][:40]))
    for mi_name in ("MI_WI_%s_Hair_Cards" % g, "MI_WI_%s_Facial_Hair" % g):
        p = "/Game/MHHeads/Build/Bruce/Grooms/" + mi_name
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            mi = unreal.load_asset(p)
            log("  %s parent %s" % (mi_name, mi.get_editor_property("parent").get_path_name()))
            for tp in mi.get_editor_property("texture_parameter_values"):
                log("    tex %s = %s" % (tp.parameter_info.name, tp.parameter_value.get_path_name() if tp.parameter_value else None))
m = unreal.load_asset("/MetaHumanCharacter/Materials/M_hair_v4")
log("M_hair_v4 tex params %s" % unreal.MaterialEditingLibrary.get_texture_parameter_names(m))
m = unreal.load_asset("/MetaHumanCharacter/Materials/M_Facial_Hair")
log("M_Facial_Hair tex params %s" % unreal.MaterialEditingLibrary.get_texture_parameter_names(m))
