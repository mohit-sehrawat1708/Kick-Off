"""Probe: copy one MetaHuman Creator preset into the project and build it with the optimized pipeline, then list
what the build produced (meshes, grooms, materials, LODs). Full editor: -ExecutePythonScript (quits when done)."""
import unreal

EAL = unreal.EditorAssetLibrary
PRESET = "/MetaHumanCharacter/Optional/Presets/Bruce"
DST = "/Game/PitchKings/MetaHumans/Probe/Bruce"
OUT = "/Game/PitchKings/MetaHumans/Probe/Build"


def log(s):
    unreal.log("[MHProbe] " + str(s))


try:
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    ch = EAL.duplicate_asset(PRESET, DST)
    log("duplicated %s -> %s (%s)" % (PRESET, DST, ch.get_class().get_name() if ch else None))
    for p in ("preview_material_type",):
        try:
            log("%s = %s" % (p, ch.get_editor_property(p)))
        except Exception as ex:
            log("no %s" % p)
    log("character props: %s" % [n for n in dir(ch) if not n.startswith("_") and ("rig" in n.lower() or "dna" in n.lower() or "texture" in n.lower() or "skin" in n.lower())][:40])
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    log("subsystem fns: %s" % [n for n in dir(sub) if not n.startswith("_")][:120])
    if not sub.try_add_object_to_edit(ch):
        raise RuntimeError("cannot edit")
    try:
        bp = unreal.MetaHumanCharacterEditorBuildParameters()
        bp.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        bp.pipeline_quality = unreal.MetaHumanQualityLevel.LOW
        bp.absolute_build_path = OUT
        bp.common_folder_path = OUT + "/Common"
        bp.enable_wardrobe_item_validation = False
        r = sub.build_meta_human(character=ch, params=bp)
        log("build result %s" % (r,))
    finally:
        if sub.is_object_added_for_editing(ch):
            sub.remove_object_to_edit(ch)
    assets = EAL.list_assets(OUT, recursive=True)
    log("built %d assets" % len(assets))
    for a in assets[:80]:
        o = unreal.load_asset(a)
        extra = ""
        if isinstance(o, unreal.SkeletalMesh):
            ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
            extra = " lods=%d verts0=%d skel=%s" % (ss.get_lod_count(o), ss.get_num_verts(o, 0), o.skeleton.get_name())
        log("  %s %s%s" % (a.split(".")[0], o.get_class().get_name() if o else None, extra))
except Exception as ex:
    import traceback
    unreal.log_error("[MHProbe] FAILED %s %s" % (ex, traceback.format_exc()))
unreal.SystemLibrary.quit_editor()
