import unreal
EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
def log(s): unreal.log("[Probe] " + str(s))
hs = unreal.load_asset("/Game/PitchKings/Data/DA_PK_Heads")
for h in hs.get_editor_property("heads"):
    log("head %s face %s" % (h.get_editor_property("name"), h.get_editor_property("face")))
    for prop in ("hair", "beard", "mustache", "eyebrows"):
        m = h.get_editor_property(prop)
        if not m:
            log("  %s none" % prop); continue
        b = m.get_bounds()
        log("  %s %s lods %d bounds origin %s ext %s" % (prop, m.get_name(), m.get_num_lods(), b.origin, b.box_extent))
        for i, sm in enumerate(m.static_materials):
            mi = sm.material_interface
            base = mi.get_base_material() if mi else None
            log("    slot %d %s base %s blend %s shading %s" % (i, mi.get_path_name() if mi else None, base.get_path_name() if base else None,
                base.get_editor_property("blend_mode") if base else None, base.get_editor_property("shading_model") if base else None))
            if mi and isinstance(mi, unreal.MaterialInstance):
                for tp in mi.get_editor_property("texture_parameter_values"):
                    log("      tex %s = %s" % (tp.parameter_info.name, tp.parameter_value.get_name() if tp.parameter_value else None))
                for sp in mi.get_editor_property("scalar_parameter_values"):
                    log("      scalar %s = %s" % (sp.parameter_info.name, sp.parameter_value))
    face = h.get_editor_property("face")
    for i, sm in enumerate(face.materials):
        mi = sm.material_interface
        base = mi.get_base_material() if mi else None
        log("  face slot %s %s base %s blend %s" % (sm.material_slot_name, mi.get_name() if mi else None, base.get_path_name() if base else None, base.get_editor_property("blend_mode") if base else None))
    dm = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(face, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    dm = dm[0] if isinstance(dm, tuple) else dm
    for bone in ("spine_04", "spine_05", "clavicle_l", "neck_01", "neck_02", "head"):
        r = unreal.GeometryScript_BoneWeights.get_bone_info(dm, bone)
        info = [x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, unreal.GeometryScriptBoneInfo)]
        if info:
            log("  bone %s at %s" % (bone, info[0].get_editor_property("world_transform").translation))
    log("  morphs %d" % len(face.get_all_morph_target_names()) if hasattr(face, "get_all_morph_target_names") else "")
    bb = face.get_bounds()
    log("  face bounds %s %s" % (bb.origin, bb.box_extent))
