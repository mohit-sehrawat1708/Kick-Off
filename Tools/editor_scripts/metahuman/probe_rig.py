"""Probe: copy a preset, cloud auto-rig (joints only), download textures, build (optimized, low). Full editor; quits."""
import unreal

EAL = unreal.EditorAssetLibrary
PRESET = "/MetaHumanCharacter/Optional/Presets/Bruce"
DST = "/Game/PitchKings/MetaHumans/Probe/Bruce"
OUT = "/Game/PitchKings/MetaHumans/Probe/Build"


def log(s):
    unreal.log("[MHRig] " + str(s))


try:
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    ch = EAL.duplicate_asset(PRESET, DST)
    EAL.save_asset(DST)
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if not sub.try_add_object_to_edit(ch):
        raise RuntimeError("cannot edit")
    try:
        rp = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rp.blocking = True
        rp.report_progress = False
        rp.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        r = sub.request_auto_rigging(ch, rp)
        log("auto rig returned %s; can build %s" % (r, sub.can_build_meta_human(ch)))
        tp = unreal.MetaHumanCharacterTextureRequestParams()
        tp.blocking = True
        tp.report_progress = False
        r = sub.request_texture_sources(ch, tp)
        log("textures returned %s, high res %s" % (r, ch.get_editor_property("has_high_resolution_textures")))
        EAL.save_asset(DST)
        if sub.can_build_meta_human(ch):
            bp = unreal.MetaHumanCharacterEditorBuildParameters()
            bp.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
            bp.pipeline_quality = unreal.MetaHumanQualityLevel.LOW
            bp.absolute_build_path = OUT
            bp.common_folder_path = OUT + "/Common"
            bp.enable_wardrobe_item_validation = False
            sub.build_meta_human(character=ch, params=bp)
            assets = EAL.list_assets(OUT, recursive=True)
            log("built %d assets" % len(assets))
            ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
            for a in assets[:60]:
                o = unreal.load_asset(a)
                extra = ""
                if isinstance(o, unreal.SkeletalMesh):
                    extra = " lods=%d verts0=%d skel=%s" % (ss.get_lod_count(o), ss.get_num_verts(o, 0), o.skeleton.get_name())
                log("  %s %s%s" % (a.split(".")[0], o.get_class().get_name() if o else None, extra))
    finally:
        if sub.is_object_added_for_editing(ch):
            sub.remove_object_to_edit(ch)
except Exception as ex:
    import traceback
    unreal.log_error("[MHRig] FAILED %s %s" % (ex, traceback.format_exc()))
unreal.SystemLibrary.quit_editor()
