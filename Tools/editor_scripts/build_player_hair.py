"""
Player hair and beards from City Sample Crowds' groom cards: one static mesh per style with a LOD chain
(cards LOD1 -> cards LOD3 -> helmet LOD5), so the close-ups get real hair volume and the broadcast view stays cheap.

The pack's card meshes are modelled in the reference pose around the head (head bone at 162.6 cm); here they are moved
into the head bone's own space, so PKFootballer attaches them to the "head" socket with no offset. (With the offset,
the players' non-uniform build scale swung the hair ~10 cm off the skull.) Stray cards hanging below the jaw are cut.

  UnrealEditor PitchKings.uproject -ExecutePythonScript=Tools/editor_scripts/build_player_hair.py  (full editor: the static
  mesh subsystem is missing in the commandlet; the editor quits when done)
"""
import unreal

EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
GA = unreal.GeometryScript_AssetUtils
GN = unreal.GeometryScript_NewAssetUtils
GE = unreal.GeometryScript_MeshEdits
GT = unreal.GeometryScript_MeshTransforms
GQ = unreal.GeometryScript_MeshQueries
M = "/Game/CitySampleCrowd/Character/Male"
CARDS_MAT = "/Game/CitySampleCrowd/Character/Shared/Materials/MetaHuman/HairCards"
OUT = "/Game/PitchKings/Characters/Hair"
TMP = OUT + "/Tmp"
# head bone in the reference pose (component space), as dumped by PKFootballer -PKBoneDump
HEAD = unreal.Transform(location=unreal.Vector(0.0, 0.663, 162.575), rotation=unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0), scale=unreal.Vector(1, 1, 1))

# name: (cards mesh path with %d for the LOD, helmet mesh, lowest z kept)
STYLES = {
    "AfroFade": (M + "/m_001/Hair/Hair/Hair_S_AfroFade_CardsMesh_Group0_LOD%d", M + "/m_001/Hair/Helmets/Hair_S_AfroFade_Helmet_LOD5", 150.0),
    "CurlyFade": (M + "/m_002/Hair/Hair/Hair_S_CurlyFade_Cards/Hair_S_CurlyFade_CardsMesh_Group0_LOD%d", M + "/m_002/Hair/Helmets/Hair_S_CurlyFade_Helmet_LOD5", 150.0),
    "SideSweptFringe": (M + "/m_003/Hair/Hair/Hair_M_SideSweptFringe_CardsMesh_Group0_LOD%d", M + "/m_003/Hair/Helmets/Hair_M_SideSweptFringe_Mesh_LOD5", 145.0),
    "PulledBack": (M + "/m_004/Hair/Hair/Hair_S_PulledBack_CardsMesh_Group0_LOD%d", M + "/m_004/Hair/Helmets/Hair_S_PulledBack_Helmet_LOD5", 150.0),
    "HairLoss": (M + "/m_006/Hair/Hair/Hair_S_HairLoss_Modified/Hair_S_HairLoss_CardsMesh_Group0_LOD%d", M + "/m_006/Hair/Helmets/Hair_S_HairLoss_Helmet_LOD5", 150.0),
    "Coil": ("/Game/CitySampleCrowd/Character/Female/f_001/Hair/Hair/Hair_S_Coil_CardsMesh_Group0_LOD%d", "/Game/CitySampleCrowd/Character/Female/f_001/Hair/Helmets/Hair_S_Coil_Helmet_LOD5", 150.0),
    "BeardFull": (M + "/m_002/Hair/Hair/Beard_S_Full_CardsMesh_Group0_LOD%d", M + "/m_002/Hair/Helmets/Beard_S_Full_Helmet_LOD5", 145.0),
    "ChinStrap": (M + "/m_003/Hair/Hair/Goatee_S_ChinStrap_CardsMesh_Group0_LOD%d", None, 145.0),
}
# grooms from other heads: (lift along the head bone in cm, scale) to fit the male heads (f_001's head sits ~11 cm lower)
FIT = {"Coil": (11.5, 1.06)}
CARD_LODS = (0, 2)          # cards LODs used for mesh LOD0, LOD1 (LOD0: the finest cards, for close-ups)
SCREEN = (1.0, 0.3, 0.08)  # screen sizes of LOD0, LOD1, LOD2 (helmet)


def log(s):
    unreal.log("[PlayerHair] " + s)


def load(path):
    return unreal.load_asset(path) if path and EAL.does_asset_exist(path) else None


def pick(result, cls):
    if isinstance(result, cls):
        return result
    for x in (result if isinstance(result, tuple) else ()):
        if isinstance(x, cls):
            return x
    return None


def to_head_space(src, min_z, tmp_name, fit=(0.0, 1.0)):
    """Copy a card mesh, drop the triangles below min_z (stray cards), move it into head-bone space."""
    dm = unreal.DynamicMesh()
    read = unreal.GeometryScriptMeshReadLOD()
    read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    read.lod_index = 0
    dm = pick(GA.copy_mesh_from_static_mesh(src, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read), unreal.DynamicMesh) or dm
    low = []
    for t in range(GQ.get_num_triangle_i_ds(dm)):
        if not GQ.is_valid_triangle_id(dm, t):
            continue
        r = GQ.get_triangle_positions(dm, t)
        vs = [v for v in (r if isinstance(r, tuple) else ()) if isinstance(v, unreal.Vector)]
        if vs and min(v.z for v in vs) < min_z:
            low.append(t)
    if low:
        lst = pick(unreal.GeometryScript_List.convert_array_to_index_list(low, unreal.GeometryScriptIndexType.TRIANGLE), unreal.GeometryScriptIndexList)
        GE.delete_triangles_from_mesh(dm, lst)
    GT.transform_mesh(dm, HEAD.inverse())
    if fit != (0.0, 1.0):
        GT.scale_mesh(dm, unreal.Vector(fit[1], fit[1], fit[1]), unreal.Vector(0, 0, 0))
        GT.translate_mesh(dm, unreal.Vector(fit[0], 0, 0))
    path = "%s/%s" % (TMP, tmp_name)
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.set_editor_property("enable_recompute_normals", False)
    opts.set_editor_property("enable_recompute_tangents", False)
    opts.set_editor_property("enable_nanite", False)
    opts.set_editor_property("enable_collision", False)
    mesh = pick(GN.create_new_static_mesh_asset_from_mesh(dm, path, opts), unreal.StaticMesh)
    if mesh:
        mesh.set_material(0, src.static_materials[0].material_interface)
    return mesh, len(low)


def build(name, cards, helmet, min_z):
    srcs = [load(cards % i) for i in CARD_LODS]
    srcs = [s for s in srcs if s] + ([load(helmet)] if load(helmet) else [])
    if not srcs:
        log("%s: no card meshes" % name)
        return
    parts = []
    for i, s in enumerate(srcs):
        m, cut = to_head_space(s, min_z, "SM_%s_L%d" % (name, i), FIT.get(name, (0.0, 1.0)))
        if m:
            parts.append(m)
            if cut:
                log("%s lod %d: cut %d stray triangles" % (name, i, cut))
    dst = "%s/SM_PK_Hair_%s" % (OUT, name)
    if EAL.does_asset_exist(dst):
        EAL.delete_asset(dst)
    mesh = EAL.duplicate_asset(parts[0].get_path_name().split(".")[0], dst)
    for i, p in enumerate(parts[1:], start=1):
        SMS.set_lod_from_static_mesh(mesh, i, p, 0, True)
    # the pulled-back cards ship with the grid material: use the helmet's material of the same style
    for i, slot in enumerate(mesh.static_materials):
        mi = slot.material_interface
        if mi is None or mi.get_name() == "WorldGridMaterial":
            fix = load("%s/MI_Hair_Cards_S_%s" % (CARDS_MAT, name))
            if not fix and load(helmet):
                fix = load(helmet).static_materials[0].material_interface
            mesh.set_material(i, fix)
    n = SMS.get_lod_count(mesh)
    try:
        SMS.set_lod_screen_sizes(mesh, [SCREEN[min(i, len(SCREEN) - 1)] for i in range(n)])
    except Exception as ex:
        log("screen sizes: %s" % ex)
    EAL.save_asset(dst)
    b = mesh.get_bounding_box()
    tris = [mesh.get_num_triangles(i) for i in range(n)]
    log("%s: lods %d tris %s box (%.1f %.1f %.1f)-(%.1f %.1f %.1f) mats %s" % (name, n, tris, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z,
                                                                       [m.material_interface.get_name() if m.material_interface else None for m in mesh.static_materials]))


try:
    only = [a.split("=", 1)[1].split(",") for a in unreal.SystemLibrary.get_command_line().split() if a.startswith("-PKHair=")]
    for k, (c, h, z) in STYLES.items():
        if not only or k in only[0]:
            build(k, c, h, z)
    EAL.delete_directory(TMP)
    log("done")
except Exception as ex:
    import traceback
    log("FAILED %s %s" % (ex, traceback.format_exc()))
unreal.SystemLibrary.quit_editor()
