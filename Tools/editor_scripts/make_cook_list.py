"""
DA_PK_CookList: hard references to the City Sample Crowds assets PKFootballer loads by path (faces, clothes, scalp maps),
so the packaged build cooks them. Keep in step with the paths in PKFootballer.cpp.

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_cook_list.py
"""
import unreal

EAL = unreal.EditorAssetLibrary
C = "/Game/CitySampleCrowd/Character"
PATH = "/Game/PitchKings/Data/DA_PK_CookList"

assets = ["%s/Male/m_00%d/Face/m_00%d_nrw_FaceMesh" % (C, i, i) for i in range(1, 7)]
assets += ["%s/Male/m_00%d/Textures/m_00%d_nrw_LODnormals" % (C, i, i) for i in range(1, 7)]
assets += [C + "/Male/NormalWeight/Meshes/m_tal_nrw_crewneck", C + "/Male/NormalWeight/Meshes/m_tal_nrw_slacks"]
assets += ["%s/Shared/Textures/Hair/%s/%s" % (C, n, n) for n in (
    "Hair_S_BuzzCut_baked", "Hair_S_AfroFade_baked", "Hair_S_Coil_baked", "Hair_S_PulledBack_baked", "Hair_S_HairLoss_baked",
    "Eyebrows_M_Full_baked", "Eyebrows_M_Dense_baked", "Eyebrows_M_Thick_baked", "Eyebrows_M_Natural_baked", "Eyebrows_M_Wide_baked",
    "Eyebrows_M_Unruly_baked", "Beard_S_Stubble_baked", "Goatee_S_ChinStrap_baked")]

objs = []
for p in assets:
    o = unreal.load_asset(p)
    if o:
        objs.append(o)
    else:
        unreal.log_warning("[CookList] missing %s" % p)
if EAL.does_asset_exist(PATH):
    da = unreal.load_asset(PATH)
else:
    cls = unreal.load_class(None, "/Script/PitchKings.PKCookList")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    da = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_PK_CookList", "/Game/PitchKings/Data", cls, factory)
da.set_editor_property("assets", objs)
EAL.save_asset(PATH)
unreal.log("[CookList] %d assets" % len(objs))
