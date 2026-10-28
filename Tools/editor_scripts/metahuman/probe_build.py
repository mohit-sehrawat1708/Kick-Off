"""Probe: build the already-rigged preset copy (optimized, low), tolerate the control-rig link warnings, save outputs."""
import unreal

EAL = unreal.EditorAssetLibrary
DST = "/Game/PitchKings/MetaHumans/Probe/Bruce"
OUT = "/Game/PitchKings/MetaHumans/Probe/Build"


def log(s):
    unreal.log("[MHBuild] " + str(s))


try:
    ch = unreal.load_asset(DST)
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if not sub.try_add_object_to_edit(ch):
        raise RuntimeError("cannot edit")
    log("can build: %s" % (sub.can_build_meta_human(ch),))
    try:
        bp = unreal.MetaHumanCharacterEditorBuildParameters()
        bp.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        bp.pipeline_quality = unreal.MetaHumanQualityLevel.LOW
        bp.absolute_build_path = OUT
        bp.enable_wardrobe_item_validation = False
        try:
            sub.build_meta_human(character=ch, params=bp)
        except Exception as ex:
            log("build raised (continuing): %s" % str(ex)[:200])
    finally:
        if sub.is_object_added_for_editing(ch):
            sub.remove_object_to_edit(ch)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    assets = EAL.list_assets(OUT, recursive=True)
    log("built %d assets under %s" % (len(assets), OUT))
    ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for a in assets:
        if "/Common/" in a:
            continue
        o = unreal.load_asset(a)
        extra = ""
        if isinstance(o, unreal.SkeletalMesh):
            extra = " lods=%d verts0=%d skel=%s mats=%d" % (ss.get_lod_count(o), ss.get_num_verts(o, 0), o.skeleton.get_name(), len(o.materials))
        log("  %s %s%s" % (a.split(".")[0], o.get_class().get_name() if o else None, extra))
except Exception as ex:
    import traceback
    unreal.log_error("[MHBuild] FAILED %s %s" % (ex, traceback.format_exc()))
unreal.SystemLibrary.quit_editor()
