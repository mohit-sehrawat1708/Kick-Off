"""
Player heads from MetaHuman Creator presets (MetaHuman Creator Core Data, UE 5.8).

For each preset in HEADS (resumable: finished steps are skipped):
  1. copy the preset character to /Game/MHHeads/Chars/<Name>
  2. rig it (Epic's cloud auto-rigging, joints only, ~8 min) and download its texture sources
  3. build it with the optimized pipeline at Medium quality into /Game/MHHeads/Build (outside /Game/PitchKings, which is
     always cooked: the unused body / clothing / blueprint must not ship)
  4. bake the hair, beard and moustache cards (LOD0, LOD2, helmet LOD5) into the face's head-bone space with their own
     MetaHuman hair materials -> /Game/PitchKings/Characters/Heads/SM_PK_<Name>_<Groom>
  5. read the face's average skin colour from its baked albedo
Then writes DA_PK_Heads (UPKHeadSet) for PKFootballer.

Full editor only (cloud requests and the build need it):
  UnrealEditor PitchKings.uproject -ExecutePythonScript=Tools/editor_scripts/metahuman/build_player_heads.py [-PKHeads=Bruce,Omari]
"""
import os
import tempfile

import unreal

EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
GA = unreal.GeometryScript_AssetUtils
GN = unreal.GeometryScript_NewAssetUtils
GT = unreal.GeometryScript_MeshTransforms
PRESETS = "/MetaHumanCharacter/Optional/Presets"
CHARS = "/Game/MHHeads/Chars"
BUILD = "/Game/MHHeads/Build3"   # (Build, Build2: faces trimmed by later recipes; trims only ever cut more)
OUT = "/Game/PitchKings/Characters/Heads"
SET = "/Game/PitchKings/Data/DA_PK_HeadSet"
PROBE = "/Game/PitchKings/MetaHumans/Probe/Bruce"   # already rigged during the first test: reused, then removed
NECK_CUT = 3.0     # cm below neck_01 in the face's reference pose
SIDE_CUT = 4.0     # either side of the neck: cut up to this far above neck_01
NECK_HALF_W = 7.0  # cm from the neck axis
TRIM_VERSION = 4
CHEST_CUT = 9.0    # the front keeps its neck down into the collar notch (a higher cut left a hole there)
HEADS = ["Omari", "Mateo", "Kelvin", "Lorenzo", "Isaiah", "Jorge", "Trey", "Mikel", "Dominic", "Victor", "Orlando", "Bruce"]


def log(s):
    unreal.log("[Heads] " + str(s))


def pick(result, cls):
    if isinstance(result, cls):
        return result
    for x in (result if isinstance(result, tuple) else ()):
        if isinstance(x, cls):
            return x
    return None


def rigged_character(name, sub):
    path = "%s/%s" % (CHARS, name)
    if not EAL.does_asset_exist(path):
        src = PROBE if name == "Bruce" and EAL.does_asset_exist(PROBE) else "%s/%s" % (PRESETS, name)
        EAL.duplicate_asset(src, path)
        EAL.save_asset(path)
        log("%s: copied from %s" % (name, src))
    ch = unreal.load_asset(path)
    if sub.can_build_meta_human(ch):
        return ch
    if not sub.try_add_object_to_edit(ch):
        raise RuntimeError("cannot edit " + name)
    try:
        rp = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rp.blocking = True
        rp.report_progress = False
        rp.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        sub.request_auto_rigging(ch, rp)
        tp = unreal.MetaHumanCharacterTextureRequestParams()
        tp.blocking = True
        tp.report_progress = False
        sub.request_texture_sources(ch, tp)
    finally:
        if sub.is_object_added_for_editing(ch):
            sub.remove_object_to_edit(ch)
    EAL.save_asset(path)
    log("%s: rigged, textures downloaded, can build %s" % (name, sub.can_build_meta_human(ch)))
    return ch


def built(name, ch, sub):
    face = "%s/%s/Face/SKM_%s_FaceMesh" % (BUILD, name, name)
    if EAL.does_asset_exist(face):
        return unreal.load_asset(face)
    if not sub.try_add_object_to_edit(ch):
        raise RuntimeError("cannot edit " + name)
    try:
        bp = unreal.MetaHumanCharacterEditorBuildParameters()
        bp.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        bp.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        bp.absolute_build_path = BUILD
        bp.enable_wardrobe_item_validation = False
        try:
            sub.build_meta_human(character=ch, params=bp)
        except Exception as ex:   # the shared control rigs log link errors that Python raises; the build itself completes
            log("%s: build warnings (%s)" % (name, str(ex)[:120]))
    finally:
        if sub.is_object_added_for_editing(ch):
            sub.remove_object_to_edit(ch)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("%s: built" % name)
    return unreal.load_asset(face)


def gs_func(fn):
    """A Geometry Script library function by name (the Python class names of the libraries vary between versions)."""
    for cls in dir(unreal):
        if cls.startswith("GeometryScript"):
            f = getattr(getattr(unreal, cls), fn, None)
            if f:
                return f
    raise RuntimeError("no geometry script function " + fn)


def trim_neck(name, face):
    """The MetaHuman face mesh carries a chunk of neck, shoulders and upper chest in the preset's own body shape: on the
    shared shirt it pokes through the collar and over the shoulders. Delete the triangles below the base of the neck
    and, either side of the neck, up to a little above it (the trapezius), in every LOD: the shirt covers the cut."""
    tag = "PKNeckTrimmed"
    done = int(EAL.get_metadata_tag(face, tag) or 0)
    if done >= TRIM_VERSION:
        return
    BS = unreal.GeometryScript_BoneWeights
    nlod = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(face)
    for lod in range(nlod):
        rl = unreal.GeometryScriptMeshReadLOD()
        rl.set_editor_property("lod_index", lod)
        dm = pick(GA.copy_mesh_from_skeletal_mesh(face, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), rl), unreal.DynamicMesh)
        neck = pick(BS.get_bone_info(dm, "neck_01"), unreal.GeometryScriptBoneInfo).get_editor_property("world_transform").translation
        cut, side = neck.z - NECK_CUT, neck.z + SIDE_CUT
        boxes = [((-200, -200, -50), (200, 200, neck.z - CHEST_CUT)),   # chest
                 ((-200, -200, -50), (200, -2.0, cut)),                # back, below the collar
                 ((-200, -200, -50), (-NECK_HALF_W, 200, side)),       # right shoulder
                 ((NECK_HALF_W, -200, -50), (200, 200, side))]         # left shoulder
        before = dm.get_triangle_count() if hasattr(dm, "get_triangle_count") else -1
        for lo, hi in boxes:
            box = unreal.Box(unreal.Vector(*lo), unreal.Vector(*hi))
            sel = pick(gs_func("select_mesh_elements_in_box")(dm, box, unreal.GeometryScriptMeshSelectionType.TRIANGLES, False, 3),
                       unreal.GeometryScriptMeshSelection)
            gs_func("delete_selected_triangles_from_mesh")(dm, sel)
        wl = unreal.GeometryScriptMeshWriteLOD()
        wl.set_editor_property("lod_index", lod)
        GA.copy_mesh_to_skeletal_mesh(dm, face, unreal.GeometryScriptCopyMeshToAssetOptions(), wl)
        log("%s: LOD%d neck cut at z %.1f, sides |x| > %.1f below z %.1f, tris %s -> %s" % (name, lod, cut, NECK_HALF_W, side, before,
            dm.get_triangle_count() if hasattr(dm, "get_triangle_count") else -1))
    EAL.set_metadata_tag(face, tag, str(TRIM_VERSION))
    EAL.save_loaded_asset(face)


def head_transform(face):
    dm = pick(GA.copy_mesh_from_skeletal_mesh(face, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                              unreal.GeometryScriptMeshReadLOD()), unreal.DynamicMesh)
    info = pick(unreal.GeometryScript_BoneWeights.get_bone_info(dm, "head"), unreal.GeometryScriptBoneInfo)
    return info.get_editor_property("world_transform")


MEL = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
HAIR_MASTER = OUT + "/M_PK_HairCardsMH"

# MetaHuman groom atlases (texture layout 2): Tangent = tangent RGB + root-to-tip U in A; Attribute = coverage in R,
# root UV in GB. The pack's hair material reads them through the groom component, which a static mesh does not have,
# so the cards use this material instead (the M_PK_HairCards look: dithered coverage, thinned tips and edge-on cards,
# the hair lit as one volume around the head).
MH_COLOR = r"""
float t = saturate(Tan.a);
float seed = frac(Att.g * 7.31 + Att.b * 3.17);
float3 c = HairColor.rgb * lerp(0.8, 1.0, t) * lerp(0.88, 1.12, seed);  // tips no lighter: they read as a grey fringe
return c;
"""


def custom(mat, code, out_type, inputs, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property("code", code)
    n.set_editor_property("output_type", out_type)
    ins = []
    for nm in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    n.set_editor_property("inputs", ins)
    return n


def hair_master(tangent, attribute):
    if EAL.does_asset_exist(HAIR_MASTER):
        return unreal.load_asset(HAIR_MASTER)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_PK_HairCardsMH", OUT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("dither_opacity_mask", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.35)
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    tex = {}
    for k, (pname, t) in enumerate((("TangentTexture", tangent), ("AttributeTexture", attribute))):
        sm = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 220 * k)
        sm.set_editor_property("parameter_name", pname)
        sm.set_editor_property("texture", t)
        sm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        MEL.connect_material_expressions(uv, "", sm, "UVs")
        tex[pname] = sm
    col = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 700)
    col.set_editor_property("parameter_name", "HairColor")
    col.set_editor_property("default_value", unreal.LinearColor(0.03, 0.02, 0.015, 1))
    cn = custom(mat, MH_COLOR, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ("Tan", "Att", "HairColor"), -500, 100)
    MEL.connect_material_expressions(tex["TangentTexture"], "RGBA", cn, "Tan")
    MEL.connect_material_expressions(tex["AttributeTexture"], "RGBA", cn, "Att")
    MEL.connect_material_expressions(col, "", cn, "HairColor")
    MEL.connect_material_property(cn, "", MP.MP_BASE_COLOR)
    cov = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 800)
    cov.set_editor_property("parameter_name", "Coverage")
    cov.set_editor_property("default_value", 1.0)
    cam = MEL.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, 900)
    nrm = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 1000)
    op = custom(mat, "float facing = saturate(abs(dot(normalize(Nrm), normalize(Cam))) * 2.5 + 0.05);"
                     " float tips = 1.0 - 0.7 * smoothstep(0.55, 1.0, Tan.a);"
                     " float a = Att.r * facing * tips * Cov;"
                     " return smoothstep(0.2, 0.65, a);",  # faint coverage drops out instead of dithering into stray dots
                unreal.CustomMaterialOutputType.CMOT_FLOAT1, ("Att", "Tan", "Nrm", "Cam", "Cov"), -500, 500)
    MEL.connect_material_expressions(tex["AttributeTexture"], "RGBA", op, "Att")
    MEL.connect_material_expressions(tex["TangentTexture"], "RGBA", op, "Tan")
    MEL.connect_material_expressions(nrm, "", op, "Nrm")
    MEL.connect_material_expressions(cam, "", op, "Cam")
    MEL.connect_material_expressions(cov, "", op, "Cov")
    MEL.connect_material_property(op, "", MP.MP_OPACITY_MASK)
    mat.set_editor_property("tangent_space_normal", False)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 1200)
    obj = MEL.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -900, 1300)
    fwd = MEL.create_material_expression(mat, unreal.MaterialExpressionTransform, -900, 1400)
    fwd.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    fwd.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    upc = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1100, 1400)
    upc.set_editor_property("constant", unreal.LinearColor(1, 0, 0, 0))  # head bone +X runs up the skull
    MEL.connect_material_expressions(upc, "", fwd, "")
    nrm2 = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 1500)
    vn = custom(mat, "float3 c = Obj + normalize(Up) * 9.0; float3 v = normalize(WP - c);"
                     " float3 n = normalize(Nrm); n *= sign(dot(n, v) + 1e-4);"
                     " return normalize(lerp(v, n, 0.2));",
                unreal.CustomMaterialOutputType.CMOT_FLOAT3, ("WP", "Obj", "Up", "Nrm"), -500, 1250)
    MEL.connect_material_expressions(wp, "", vn, "WP")
    MEL.connect_material_expressions(obj, "", vn, "Obj")
    MEL.connect_material_expressions(fwd, "", vn, "Up")
    MEL.connect_material_expressions(nrm2, "", vn, "Nrm")
    MEL.connect_material_property(vn, "", MP.MP_NORMAL)
    r = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    r.set_editor_property("r", 0.7)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    sp = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 500)
    sp.set_editor_property("r", 0.15)  # duller sheen: the sky lit sparse tips into a grey fringe
    MEL.connect_material_property(sp, "", MP.MP_SPECULAR)
    MEL.recompile_material(mat)
    EAL.save_asset(HAIR_MASTER)
    log("hair master built")
    return mat


def groom_material(name, kind, groom):
    """MI of the MH card master with this groom's atlases."""
    tx = groom.get_editor_property("hair_groups_cards")[0].get_editor_property("textures").get_editor_property("textures")
    tangent, attribute = tx[0], tx[1]
    master = hair_master(tangent, attribute)
    path = "%s/MI_PK_%s_%s" % (OUT, name, kind)
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)   # in use by the loaded game: updated in place
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_PK_%s_%s" % (name, kind), OUT, unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    MEL.set_material_instance_texture_parameter_value(mi, "TangentTexture", tangent)
    MEL.set_material_instance_texture_parameter_value(mi, "AttributeTexture", attribute)
    if kind == "Hair":
        MEL.set_material_instance_scalar_parameter_value(mi, "Coverage", 1.0)  # (1.25 hardened the cards into blocky chunks)
    if kind in ("Beard", "Mustache"):
        MEL.set_material_instance_scalar_parameter_value(mi, "Coverage", 1.3)  # short facial hair: solid, not a pixel cloud
    MEL.update_material_instance(mi)
    EAL.save_asset(path)
    return mi


def hair_colour(groom_dir, prefix):
    """Linear albedo from the groom's MetaHuman melanin / redness (rough fit of the pack's hair absorption)."""
    mel, red = 0.5, 0.1
    for s in ("Hair_Cards", "Facial_Hair", "Hair"):
        p = "%s/MI_WI_%s_%s" % (groom_dir, prefix, s)
        if EAL.does_asset_exist(p):
            mi = unreal.load_asset(p)
            for sp in mi.get_editor_property("scalar_parameter_values"):
                n = str(sp.parameter_info.name)
                if n == "hairMelanin":
                    mel = sp.parameter_value
                elif n == "hairRedness":
                    red = sp.parameter_value
            break
    k = mel ** 0.6
    base = [0.55 + (0.012 - 0.55) * k, 0.40 + (0.009 - 0.40) * k, 0.25 + (0.007 - 0.25) * k]
    base = [base[0] * (1 + red * 1.2), base[1] * (1 + red * 0.2), base[2] * (1 - red * 0.5)]
    return unreal.LinearColor(base[0], base[1], base[2], 1)


def bake_groom(name, groom_dir, prefix, head, kind, groom):
    """prefix e.g. Hair_S_Casual: card LODs 0 / 2 / 4 -> one static mesh in head-bone space with the MH card material."""
    def find(lod):
        for s in ("CardsMesh_Group0_LOD%d" % lod, "CardMesh_Group0_LOD%d" % lod):
            p = "%s/%s_%s" % (groom_dir, prefix, s)
            if EAL.does_asset_exist(p):
                return unreal.load_asset(p)
        return None
    srcs = [m for m in (find(0), find(2), find(4)) if m]
    if not srcs:
        return None
    mi = groom_material(name, kind, groom)
    parts = []
    tmp = OUT + "/Tmp"
    for i, s in enumerate(srcs):
        dm = pick(GA.copy_mesh_from_static_mesh(s, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                                unreal.GeometryScriptMeshReadLOD()), unreal.DynamicMesh)
        GT.transform_mesh(dm, head.inverse())
        path = "%s/%s_%s_L%d" % (tmp, name, kind, i)
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        opts.set_editor_property("enable_nanite", False)
        opts.set_editor_property("enable_collision", False)
        opts.set_editor_property("enable_recompute_normals", False)
        opts.set_editor_property("enable_recompute_tangents", False)
        parts.append(pick(GN.create_new_static_mesh_asset_from_mesh(dm, path, opts), unreal.StaticMesh))
    dst = "%s/SM_PK_%s_%s" % (OUT, name, kind)
    if EAL.does_asset_exist(dst):
        # rebuilt in place: the loaded game (DA_PK_HeadSet) references the mesh, so it cannot be deleted
        mesh = unreal.load_asset(dst)
        for i, p in enumerate(parts):
            SMS.set_lod_from_static_mesh(mesh, i, p, 0, True)
    else:
        mesh = EAL.duplicate_asset(parts[0].get_path_name().split(".")[0], dst)
        for i, p in enumerate(parts[1:], start=1):
            SMS.set_lod_from_static_mesh(mesh, i, p, 0, True)
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mi)
    try:
        SMS.set_lod_screen_sizes(mesh, [1.0, 0.25, 0.06][:SMS.get_lod_count(mesh)])
    except Exception:
        pass
    EAL.save_asset(dst)
    return mesh


def skin_tone(name):
    tex_path = "%s/%s/Face/Baked/T_Head_LOD3_BC_VT" % (BUILD, name)
    if not EAL.does_asset_exist(tex_path):
        return unreal.LinearColor(0.4, 0.27, 0.2, 1)
    tex = unreal.load_asset(tex_path)
    out = os.path.join(tempfile.gettempdir(), "pk_head_%s.tga" % name)
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = out
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = None
    unreal.Exporter.run_asset_export_task(task)
    try:
        r, g, b = tga_mean(out, (0.30, 0.38, 0.42, 0.55))
        lin = lambda c: c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
        return unreal.LinearColor(lin(r), lin(g), lin(b), 1)
    except Exception as ex:
        log("%s: skin sample failed (%s)" % (name, ex))
        return unreal.LinearColor(0.4, 0.27, 0.2, 1)


def tga_mean(path, box):
    """Mean sRGB colour (0-1) of the fractional box (x0, y0, x1, y1, from the top left) of an uncompressed or RLE TGA
    (UE's Python has no PIL)."""
    import struct
    data = open(path, "rb").read()
    id_len, cmap_type, img_type = data[0], data[1], data[2]
    w, h = struct.unpack("<HH", data[12:16])
    bpp, desc = data[16], data[17]
    px = bpp // 8
    off = 18 + id_len + (struct.unpack("<H", data[5:7])[0] * (data[7] // 8) if cmap_type else 0)
    if img_type == 10:   # RLE: unpack it
        raw = bytearray()
        need = w * h * px
        while len(raw) < need:
            hdr = data[off]
            off += 1
            n = (hdr & 0x7F) + 1
            if hdr & 0x80:
                raw += data[off:off + px] * n
                off += px
            else:
                raw += data[off:off + n * px]
                off += n * px
        pix = bytes(raw)
    else:
        pix = data[off:off + w * h * px]
    top_down = bool(desc & 0x20)
    x0, x1 = int(w * box[0]), int(w * box[2])
    sums = [0, 0, 0]
    n = 0
    step = max(1, (x1 - x0) // 64)
    for fy in range(int(h * box[1]), int(h * box[3]), step):
        row = fy if top_down else h - 1 - fy
        for x in range(x0, x1, step):
            i = (row * w + x) * px
            sums[0] += pix[i + 2]
            sums[1] += pix[i + 1]
            sums[2] += pix[i]
            n += 1
    return [c / (255.0 * n) for c in sums]


def main():
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    only = [a.split("=", 1)[1].split(",") for a in unreal.SystemLibrary.get_command_line().split() if a.startswith("-PKHeads=")]
    names = only[0] if only else HEADS
    entries = []
    for name in names:
        try:
            ch = rigged_character(name, sub)
            face = built(name, ch, sub)
            if not face:
                log("%s: no face mesh after build" % name)
                continue
            trim_neck(name, face)
            head = head_transform(face)
            groom_dir = "%s/%s/Grooms" % (BUILD, name)
            prefixes = sorted(set(a.split(".")[0].split("/")[-1] for a in EAL.list_assets(groom_dir, recursive=False)))
            def is_groom(p):
                return EAL.does_asset_exist("%s/%s" % (groom_dir, p)) and isinstance(unreal.load_asset("%s/%s" % (groom_dir, p)), unreal.GroomAsset)
            def has_cards(p):
                return any(EAL.does_asset_exist("%s/%s_%s_Group0_LOD0" % (groom_dir, p, c)) for c in ("CardsMesh", "CardMesh"))
            known = ("Hair_", "Beard_", "Goatee_", "Mustache_", "Eyebrows_")
            # some presets keep their hair cards under the style name but the groom asset under a generic one (Victor:
            # Hair_S_BrushCut_CardsMesh_* with GroomAsset_0): that groom supplies the hair atlases
            generic = [p for p in prefixes if not p.startswith(known) and not p.startswith("MI_") and is_groom(p)]
            def groom(starts):
                """(card prefix, groom asset) for the first groom of this kind that has card meshes."""
                for p in prefixes:
                    if p.startswith(starts) and has_cards(p):
                        if is_groom(p):
                            return p, unreal.load_asset("%s/%s" % (groom_dir, p))
                        if generic:
                            return p, unreal.load_asset("%s/%s" % (groom_dir, generic[0]))
                return None
            e = unreal.PKHead()
            e.set_editor_property("name", name)
            e.set_editor_property("face", face)
            hair_set = False
            for start, kind, prop in (("Hair_", "Hair", "hair"), (("Beard_", "Goatee_"), "Beard", "beard"), ("Mustache_", "Mustache", "mustache"),
                                     ("Eyebrows_", "Eyebrows", "eyebrows")):
                found = groom(start)
                if found:
                    g, asset = found
                    if not asset.get_editor_property("hair_groups_cards"):
                        log("%s: %s has no card atlas, skipped" % (name, g))
                        continue
                    m = bake_groom(name, groom_dir, g, head, kind, asset)
                    e.set_editor_property(prop, m)
                    if kind == "Hair" or (kind != "Eyebrows" and not hair_set):
                        e.set_editor_property("hair_color", hair_colour(groom_dir, g))
                        hair_set = True
            tone = skin_tone(name)
            e.set_editor_property("skin", tone)
            entries.append(e)
            log("%s: head at %s, grooms %s, skin (%.3f %.3f %.3f)" % (name, head.translation, [x for x in prefixes if unreal.load_asset("%s/%s" % (groom_dir, x)).__class__.__name__ == "GroomAsset"], tone.r, tone.g, tone.b))
        except Exception as ex:
            import traceback
            unreal.log_error("[Heads] %s FAILED %s %s" % (name, ex, traceback.format_exc()))
    # merge with the existing set (a partial run keeps the heads it did not rebuild)
    if EAL.does_asset_exist(SET):
        hs = unreal.load_asset(SET)
        built_names = [str(x.get_editor_property("name")) for x in entries]
        keep = [h for h in hs.get_editor_property("heads") if str(h.get_editor_property("name")) not in built_names]  # a failed rebuild keeps its old entry
    else:
        cls = unreal.load_class(None, "/Script/PitchKings.PKHeadSet")
        f = unreal.DataAssetFactory()
        f.set_editor_property("data_asset_class", cls)
        hs = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_PK_HeadSet", "/Game/PitchKings/Data", cls, f)
        keep = []
    hs.set_editor_property("heads", keep + entries)
    EAL.save_asset(SET)
    if EAL.does_directory_exist(OUT + "/Tmp"):
        EAL.delete_directory(OUT + "/Tmp")
    log("done: %d heads in the set" % len(hs.get_editor_property("heads")))


main()
unreal.SystemLibrary.quit_editor()
