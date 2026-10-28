"""Imports the Mixamo soccer pack, retargets it onto the UE5 mannequin and measures each clip.

Run headless (editor closed):
  UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script="<this file>"

Outputs:
  /Game/Characters/Mixamo/...                 source mesh, skeleton, raw clips, IK rigs, retargeter
  /Game/Characters/Mannequins/Anims/Soccer/   PK_* clips on the mannequin skeleton
  Tools/anim_report.json                      length, kicking foot and contact time per clip
"""
import json
import os
import re

import unreal

SRC_DIR = r"C:\Users\ROHIT KUSHWAHA\Downloads\Soccer Game Pack"
MIX = "/Game/Characters/Mixamo"
MIX_ANIMS = MIX + "/Anims"
OUT = "/Game/Characters/Mannequins/Anims/Soccer"
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
REPORT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "anim_report.json")

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[PKImport] " + str(msg))


def clean_name(file_name):
    base = os.path.splitext(file_name)[0]
    m = re.match(r"^(.*?)\s*\((\d+)\)$", base)
    suffix = ""
    if m:
        base, suffix = m.group(1), "_" + m.group(2)
    return "".join(w.capitalize() for w in re.split(r"[^A-Za-z0-9]+", base) if w) + suffix


def fbx_options(skeleton=None):
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_materials = skeleton is None
    ui.import_textures = skeleton is None
    if skeleton is None:
        ui.import_mesh = True
        ui.import_as_skeletal = True
        ui.import_animations = False
        ui.create_physics_asset = False
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    else:
        ui.import_mesh = False
        ui.import_as_skeletal = True
        ui.import_animations = True
        ui.skeleton = skeleton
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
        ui.anim_sequence_import_data.set_editor_property("import_bone_tracks", True)
        ui.anim_sequence_import_data.set_editor_property("remove_redundant_keys", False)
    return ui


def import_file(path, dest, name, skeleton=None):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = fbx_options(skeleton)
    tools.import_asset_tasks([task])
    return list(task.imported_object_paths)


def load_first(paths, cls):
    for p in paths:
        a = unreal.load_asset(p)
        if isinstance(a, cls):
            return a
    return None


def make_ik_rig(name, mesh):
    path = MIX + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    rig = tools.create_asset(name, MIX, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    ok = ctrl.apply_auto_generated_retarget_definition()
    log("%s auto retarget definition: %s, root=%s" % (name, ok, ctrl.get_retarget_root()))
    try:
        ctrl.apply_auto_fbik()
    except Exception as e:  # FBIK is optional for plain FK retargeting
        log("auto FBIK skipped: %s" % e)
    eal.save_loaded_asset(rig)
    return rig


def make_retargeter(src_rig, tgt_rig, src_mesh, tgt_mesh):
    name = "RTG_Mixamo_To_Manny"
    path = MIX + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    rtg = tools.create_asset(name, MIX, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, src_mesh)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, tgt_mesh)
    if ctrl.get_num_retarget_ops() == 0:
        ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    ctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    for i in range(ctrl.get_num_retarget_ops()):
        ctrl.run_op_initial_setup(i)
    ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # Mixamo is a T-pose, Manny an A-pose: pose the target to match the source before transferring.
    ctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    ops = [str(ctrl.get_op_name(i)) for i in range(ctrl.get_num_retarget_ops())]
    log("retarget ops: %s" % ops)
    eal.save_loaded_asset(rtg)
    return rtg


def component_positions(anim, bones, chains, t):
    """Component-space positions of the given end bones at time t (chains: bone -> list root..bone)."""
    needed = sorted({b for chain in chains.values() for b in chain})
    poses = unreal.AnimationLibrary.get_bone_poses_for_time(anim, needed, t, False)
    local = dict(zip(needed, poses))
    out = {}
    for bone in bones:
        cs = unreal.Transform()
        for b in chains[bone]:  # root first
            cs = unreal.MathLibrary.compose_transforms(local[b], cs)
        out[bone] = cs.translation
    return out


def analyse(anim):
    chains = {
        "ball_r": ["root", "pelvis", "thigh_r", "calf_r", "foot_r", "ball_r"],
        "ball_l": ["root", "pelvis", "thigh_l", "calf_l", "foot_l", "ball_l"],
        "pelvis": ["root", "pelvis"],
        "head": ["root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "neck_02", "head"],
        "hand_r": ["root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"],
    }
    length = anim.get_play_length()
    steps = max(8, int(length * 60))
    samples = []
    for i in range(steps + 1):
        t = length * i / steps
        samples.append((t, component_positions(anim, list(chains.keys()), chains, t)))

    def peak(bone):
        best_t, best_v = 0.0, 0.0
        for (t0, a), (t1, b) in zip(samples, samples[1:]):
            v = (b[bone] - a[bone]).length() / max(t1 - t0, 1e-4)
            if v > best_v:
                best_t, best_v = (t0 + t1) * 0.5, v
        return best_t, best_v

    tr, vr = peak("ball_r")
    tl, vl = peak("ball_l")
    th, vh = peak("head")
    first, last = samples[0][1], samples[-1][1]
    min_head = min(s[1]["head"].z for s in samples)
    return {
        "length": round(length, 3),
        "foot": "r" if vr >= vl else "l",
        "contact": round(tr if vr >= vl else tl, 3),
        "foot_peak_speed": round(max(vr, vl), 1),
        "head_peak": round(th, 3),
        "head_peak_speed": round(vh, 1),
        "pelvis_travel": [round(last["pelvis"].x - first["pelvis"].x, 1), round(last["pelvis"].y - first["pelvis"].y, 1), round(last["pelvis"].z - first["pelvis"].z, 1)],
        "pelvis_start": [round(first["pelvis"].x, 1), round(first["pelvis"].y, 1), round(first["pelvis"].z, 1)],
        "min_head_z": round(min_head, 1),
    }


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    for folder in (MIX, MIX_ANIMS, OUT):
        eal.make_directory(folder)

    files = sorted(f for f in os.listdir(SRC_DIR) if f.lower().endswith(".fbx"))
    char_file = next(f for f in files if f.lower().startswith("character"))

    imported = import_file(os.path.join(SRC_DIR, char_file), MIX, "SK_Mixamo")
    mesh = load_first(imported, unreal.SkeletalMesh)
    if mesh is None:
        raise RuntimeError("character.fbx did not import as a skeletal mesh: %s" % imported)
    skeleton = mesh.skeleton
    log("mixamo mesh %s skeleton %s" % (mesh.get_path_name(), skeleton.get_path_name()))

    raw = []
    for f in files:
        if f == char_file:
            continue
        name = "MX_" + clean_name(f)
        paths = import_file(os.path.join(SRC_DIR, f), MIX_ANIMS, name, skeleton)
        anim = load_first(paths, unreal.AnimSequence)
        if anim is None:
            log("FAILED to import %s -> %s" % (f, paths))
            continue
        if anim.get_name() != name:
            eal.rename_asset(anim.get_path_name(), MIX_ANIMS + "/" + name)
            anim = unreal.load_asset(MIX_ANIMS + "/" + name)
        raw.append(anim)
        log("imported %s (%.2fs)" % (anim.get_name(), anim.get_play_length()))

    manny = unreal.load_asset(MANNY)
    src_rig = make_ik_rig("IK_Mixamo", mesh)
    tgt_rig = make_ik_rig("IK_PKManny", manny)
    rtg = make_retargeter(src_rig, tgt_rig, mesh, manny)

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [eal.find_asset_data(a.get_path_name()) for a in raw]
    inputs.source_mesh = mesh
    inputs.target_mesh = manny
    inputs.ik_retarget_asset = rtg
    inputs.search = "MX_"
    inputs.replace = "PK_"
    inputs.target_path = OUT
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    log("retargeted %d assets" % len(results))

    report = {}
    for data in results:
        anim = unreal.load_asset(str(data.package_name))
        if not isinstance(anim, unreal.AnimSequence):
            continue
        try:
            report[anim.get_name()] = analyse(anim)
        except Exception as e:
            report[anim.get_name()] = {"error": str(e)}
        eal.save_loaded_asset(anim)

    with open(REPORT, "w", encoding="utf-8") as fp:
        json.dump(report, fp, indent=1, sort_keys=True)
    log("report written: %s (%d clips)" % (REPORT, len(report)))


main()
