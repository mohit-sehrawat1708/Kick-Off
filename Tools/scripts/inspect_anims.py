import json
OT = "editor_toolset.toolsets.object.ObjectTools"
def props(path, names):
    try:
        return execute_tool(OT + ".get_properties", json.dumps({"instance": path, "properties": names}))["returnValue"]
    except Exception as e:
        return str(e)[:200]
def run():
    base = "/Game/Characters/Mannequins/Anims/Unarmed/"
    names = ["MM_Idle", "Walk/MF_Unarmed_Walk_Fwd", "Walk/MF_Unarmed_Walk_Left", "Jog/MF_Unarmed_Jog_Fwd", "Jog/MF_Unarmed_Jog_Bwd", "Jog/MF_Unarmed_Jog_Right", "Jump/MM_Dash", "Attack/MM_Attack_01", "Attack/MM_Attack_02", "Attack/MM_Attack_03", "Attack/MM_ChargedAttack", "Jump/MM_Land"]
    out = {}
    for n in names:
        leaf = n.split("/")[-1]
        out[n] = props(base + n + "." + leaf, ["SequenceLength", "bEnableRootMotion", "RateScale"])
    out["bones_leg"] = execute_tool("editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools.get_bone_children", json.dumps({"mesh": "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple", "bone_name": "pelvis"}))["returnValue"]
    return out
