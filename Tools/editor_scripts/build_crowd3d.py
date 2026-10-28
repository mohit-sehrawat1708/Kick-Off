"""Builds the 3D fans for the front rows from the City Sample Crowds pack (Content/CitySampleCrowd, free on Fab):

  1. 12 fan variants: body + top + bottom + shoes + MetaHuman head, merged into one skeletal mesh at crowd LODs
  2. two authored cheer animations (arms up / fist pump) on the crowd skeleton, next to the pack's idle
  3. each variant converted to a static mesh and its animations baked to bone textures (AnimToTexture plugin),
     so thousands of fans play animation as plain instanced static meshes (one draw per variant)
  4. M_PK_FanVAT: club-colour tops, skin tones, painted hair / brows from the head's baked groom maps, all driven
     by per-instance custom data (0-3 autoplay frames, 4 team, 5 seed, 6 skin tone)

Output in /Game/PitchKings/Crowd3D. Run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/build_crowd3d.py
"""
import math
import unreal

OUT = "/Game/PitchKings/Crowd3D"
CS = "/Game/CitySampleCrowd/Character"
SK_BASE = CS + "/Shared/Rig/SK_Base"
IDLE = CS + "/Anims/Loco/MTN_Set/MTN_N_Idle_B"
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
APE = unreal.AnimPoseExtensions

# (head, body weight folder, top, bottom, shoes)
VARIANTS = [
    ("Male/m_001", "Male/NormalWeight/Meshes/m_tal_nrw", "crewneck", "jeans", "loafers"),
    ("Male/m_002", "Male/NormalWeight/Meshes/m_tal_nrw", "turtleneck", "slacks", "oxfords"),
    ("Male/m_003", "Male/OverWeight/Meshes/m_tal_ovw", "crewneck", "jeans", "loafers"),
    ("Male/m_004", "Male/NormalWeight/Meshes/m_tal_nrw", "buttonDown", "jeansLong", "oxfords"),
    ("Male/m_005", "Male/NormalWeight/Meshes/m_tal_nrw", "crewneck", "slacks", "loafers"),
    ("Male/m_006", "Male/OverWeight/Meshes/m_tal_ovw", "buttonOpen", "jeans", "oxfords"),
    ("Female/f_001", "Female/NormalWeight/Meshes/f_tal_nrw", "scoopneck", "jeans", "loafers"),
    ("Female/f_002", "Female/NormalWeight/Meshes/f_tal_nrw", "turtleneck", "slacks", "oxfords"),
    ("Female/f_003", "Female/NormalWeight/Meshes/f_tal_nrw", "scoopneck", "jeans", "oxfords"),
    ("Female/f_004", "Female/NormalWeight/Meshes/f_tal_nrw", "buttonDown", "slacks", "loafers"),
    ("Female/f_005", "Female/NormalWeight/Meshes/f_tal_nrw", "scoopneck", "jeans", "loafers"),
    ("Female/f_006", "Female/NormalWeight/Meshes/f_tal_nrw", "turtleneck", "jeans", "oxfords"),
]


def log(s):
    unreal.log("[Crowd3D] " + s)


def fresh_path(name):
    full = OUT + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return full


def save_copy(obj, name, folder=OUT):
    full = folder + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return AT.duplicate_asset(name, folder, obj)


# ---------------------------------------------------------------------------------------------------------------
# 1. merged variants
# ---------------------------------------------------------------------------------------------------------------
def merge(meshes, strip, skeleton):
    p = unreal.SkeletalMeshMergeParams()
    p.set_editor_property("meshes_to_merge", meshes)
    p.set_editor_property("strip_top_lods", strip)
    p.set_editor_property("skeleton", skeleton)
    p.set_editor_property("needs_cpu_access", True)
    return unreal.SkeletalMergingLibrary.merge_meshes(p)


def build_variant(i, v, skeleton):
    head, base, top, bottom, shoes = v
    parts = [CS + "/" + base + "_" + x for x in ("body", top, bottom, shoes)]
    clothes = [unreal.load_asset(p) for p in parts]
    if None in clothes:
        log("variant %d: missing part %s" % (i, [p for p, m in zip(parts, clothes) if m is None]))
        return None
    face_path = "%s/%s/Face/%s_nrw_FaceMesh" % (CS, head, head.split("/")[-1])
    face = unreal.load_asset(face_path)
    # Crowd LODs: copies of the parts with their top LODs removed (head from LOD5, ~950 verts; body and clothes from
    # LOD3), then one merge of the copies.
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    low = []
    for k, (src, first) in enumerate([(face, 5)] + [(c, 3) for c in clothes]):
        n = sub.get_lod_count(src)
        first = min(first, n - 1)
        dst = "%s/Tmp/TMP_Part_%02d_%d" % (OUT, i, k)
        if EAL.does_asset_exist(dst):
            EAL.delete_asset(dst)
        copy = EAL.duplicate_asset(src.get_path_name().split(".")[0], dst)
        if first > 0:
            sub.remove_lods(copy, list(range(first)))
        low.append(copy)
    fan = merge(low, 0, skeleton)
    if not fan:
        log("variant %d: merge failed" % i)
        return None
    # The merged mesh stays in memory (saving a merged mesh asserts in the editor); only the baked static mesh and
    # textures made from it are saved.
    saved = fan
    log("variant %d merged" % i)
    log("variant %d: %d verts, slots %s" % (i, sub.get_num_verts(saved, 0), [str(m.material_slot_name) for m in saved.materials]))
    # remember the head's baked groom maps (hair / brows) for the material
    head_mat = None
    for sm in face.materials:
        if "LOD57" in str(sm.material_slot_name) or "LOD5" in str(sm.material_slot_name):
            head_mat = sm.material_interface
    maps = {}
    if isinstance(head_mat, unreal.MaterialInstanceConstant):
        for t in head_mat.get_editor_property("texture_parameter_values"):
            maps[str(t.parameter_info.name)] = t.parameter_value
    return saved, maps, [(face, 5)] + [(c, 3) for c in clothes]


# ---------------------------------------------------------------------------------------------------------------
# 2. cheer animations (authored from the idle pose)
# ---------------------------------------------------------------------------------------------------------------
def qmul(a, b):
    return unreal.Quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                       a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                       a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                       a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z)


def qbetween(u, v):
    u = u.normal()
    v = v.normal()
    d = u.dot(v)
    if d < -0.9999:
        return unreal.Quat(0, 0, 1, 0)
    c = u.cross(v)
    q = unreal.Quat(c.x, c.y, c.z, 1.0 + d)
    n = math.sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w)
    return unreal.Quat(q.x / n, q.y / n, q.z / n, q.w / n)


PARENT = {"upperarm_l": "clavicle_l", "upperarm_r": "clavicle_r", "lowerarm_l": "upperarm_l", "lowerarm_r": "upperarm_r"}


def qinv(q):
    return unreal.Quat(-q.x, -q.y, -q.z, q.w)


def aim(pose, bone, child, target_dir):
    """Rotate `bone` so the bone -> child direction points along target_dir (component space). Written as a local
    rotation against the parent (setting a world-space pose does not carry the children along)."""
    W, L = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
    tb = APE.get_bone_pose(pose, bone, W)
    tc = APE.get_bone_pose(pose, child, W)
    dq = qbetween(tc.translation - tb.translation, target_dir)
    new_world = qmul(dq, tb.rotation)
    parent_world = APE.get_bone_pose(pose, PARENT[bone], W).rotation
    local = qmul(qinv(parent_world), new_world)
    tl = APE.get_bone_pose(pose, bone, L)
    r = APE.set_bone_pose(pose, unreal.Transform(tl.translation, local.rotator(), tl.scale3d), bone, L)
    # struct "ref" parameters come back as return values in Python
    return r if isinstance(r, unreal.AnimPose) else pose


def author_cheer(name, skeleton, idle, kind):
    seq_path = fresh_path(name)
    fac = unreal.AnimSequenceFactory()
    fac.set_editor_property("target_skeleton", skeleton)
    seq = AT.create_asset(name, OUT, unreal.AnimSequence, fac)
    frames = 40  # 1.33 s loop at 30 fps
    opts = unreal.AnimPoseEvaluationOptions()
    base = APE.get_anim_pose_at_frame(idle, 0, opts)
    bones = list(APE.get_bone_names(base))
    keys = {b: ([], [], []) for b in bones}
    for f in range(frames + 1):
        t = f / frames
        pump = 0.5 - 0.5 * math.cos(t * 2.0 * math.pi)            # 0 .. 1 .. 0
        pose = APE.get_anim_pose_at_frame(idle, 0, opts)
        L = unreal.AnimPoseSpaces.LOCAL
        # mesh space: +Y forward, +X the character's left, Z up
        if kind == "armsup":
            for side, sx in (("l", 1.0), ("r", -1.0)):
                pose = aim(pose, "upperarm_" + side, "lowerarm_" + side, unreal.Vector(0.35 * sx, 0.15, 1.0))
                pose = aim(pose, "lowerarm_" + side, "hand_" + side, unreal.Vector(0.15 * sx, 0.1 + 0.25 * pump, 1.0))
        else:  # fist pump with the right arm, left arm down and relaxed
            pose = aim(pose, "upperarm_r", "lowerarm_r", unreal.Vector(-0.45, 0.35, 0.8 + 0.4 * pump))
            pose = aim(pose, "lowerarm_r", "hand_r", unreal.Vector(-0.1, 0.2, 1.0))
        # small bounce on the pelvis (knees take it)
        pt = APE.get_bone_pose(pose, "pelvis", L)
        pt.translation = pt.translation + unreal.Vector(0.0, 0.0, 0.0)
        if f == 0:
            log("  %s: hand_l z %.1f hand_r z %.1f (idle 92)" % (name, APE.get_bone_pose(pose, "hand_l", unreal.AnimPoseSpaces.WORLD).translation.z, APE.get_bone_pose(pose, "hand_r", unreal.AnimPoseSpaces.WORLD).translation.z))
        for b in bones:
            tr = APE.get_bone_pose(pose, b, L)
            keys[b][0].append(tr.translation)
            keys[b][1].append(tr.rotation)
            keys[b][2].append(tr.scale3d)
    ctrl = seq.controller
    ctrl.open_bracket(unreal.Text("author cheer"))
    ctrl.set_frame_rate(unreal.FrameRate(30, 1))
    ctrl.set_number_of_frames(unreal.FrameNumber(frames))
    for b in bones:
        ctrl.add_bone_curve(b)
        ctrl.set_bone_track_keys(b, keys[b][0], keys[b][1], keys[b][2])
    ctrl.close_bracket()
    EAL.save_asset(seq_path)
    log("authored %s (%d bones, %d frames)" % (name, len(bones), frames))
    return seq


# ---------------------------------------------------------------------------------------------------------------
# 3. bake
# ---------------------------------------------------------------------------------------------------------------
SAMPLE_TEX = "/AnimToTexture/Characters/Mannequin/Textures"
SAMPLE_NAMES = {"BonePosition": "TX_BonePosition", "BoneRotation": "TX_BoneRotation", "BoneWeight": "TX_BoneWeight"}


def bake(i, skel_mesh, anims, parts):
    sm_name = "SM_Fan_%02d" % i
    fresh_path(sm_name)
    log("converting %s" % sm_name)
    # Static mesh in the reference pose, built from the same part LODs as the merged skeletal mesh (Geometry Script;
    # the merged mesh has no mesh description and the plugin's own converter asserts in 5.8).
    AU = unreal.GeometryScript_AssetUtils
    target = unreal.DynamicMesh()
    mats = []
    for asset, lod in parts:
        n = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(asset)
        dm = None
        for lt in (unreal.GeometryScriptLODType.SOURCE_MODEL, unreal.GeometryScriptLODType.RENDER_DATA):
            read = unreal.GeometryScriptMeshReadLOD()
            read.set_editor_property("lod_type", lt)
            read.set_editor_property("lod_index", min(lod, n - 1))
            res = AU.copy_mesh_from_skeletal_mesh(asset, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), read)
            cand = [x for x in (res if isinstance(res, tuple) else (res,)) if isinstance(x, unreal.DynamicMesh)]
            if cand and cand[0].get_triangle_count() > 0:
                dm = cand[0]
                break
        if dm is None:
            log("  no geometry from %s" % asset.get_name())
            continue
        if asset.get_name().endswith("_FaceMesh"):
            # The head mesh carries a skirt of neck and shoulder skin that pokes through the low LOD shirts: keep the
            # head and neck above the collar (and the upper chest under a scoop neck).
            scoop = any(p[0].get_name().endswith("scoopneck") for p in parts)
            pos = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)
            pos = [x for x in (pos if isinstance(pos, tuple) else (pos,)) if isinstance(x, unreal.GeometryScriptVectorList)][0]
            pos = unreal.GeometryScript_List.convert_vector_list_to_array(pos)
            pos = [x for x in (pos if isinstance(pos, tuple) else (pos,)) if not isinstance(x, unreal.GeometryScriptVectorList)][-1]
            tl = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False)
            tl = [x for x in (tl if isinstance(tl, tuple) else (tl,)) if isinstance(x, unreal.GeometryScriptTriangleList)][0]
            tris = unreal.GeometryScript_List.convert_triangle_list_to_array(tl)
            tris = [x for x in (tris if isinstance(tris, tuple) else (tris,)) if not isinstance(x, unreal.GeometryScriptTriangleList)][-1]
            def visible(v):
                q = pos[v]
                return q.z > 149.0 or (scoop and q.y > 3.0 and abs(q.x) < 11.0 and q.z > 136.0)
            xs = [q.x for q in pos]; ys = [q.y for q in pos]; zs = [q.z for q in pos]
            log("  head bounds x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f (%d verts, %d tris)" % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs), len(pos), len(tris)))
            drop = [ti for ti, t in enumerate(tris) if not (visible(t.x) and visible(t.y) and visible(t.z))]
            il = unreal.GeometryScript_List.convert_array_to_index_list(drop)
            il = [x for x in (il if isinstance(il, tuple) else (il,)) if isinstance(x, unreal.GeometryScriptIndexList)][0]
            unreal.GeometryScript_MeshEdits.delete_triangles_from_mesh(dm, il)
            log("  head: removed %d hidden triangles" % len(drop))
        ml = AU.get_lod_material_list_from_skeletal_mesh(asset, read)
        ml = [x for x in (ml if isinstance(ml, tuple) else (ml,)) if isinstance(x, (list, unreal.Array))]
        ml = list(ml[0]) if ml else [m.material_interface for m in asset.materials]
        r = unreal.GeometryScript_MeshEdits.append_mesh_with_materials(target, mats, dm, ml, unreal.Transform())
        rl = [x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, (list, unreal.Array))]
        if rl:
            mats = list(rl[0])
        log("  part %s lod %d: %d tris, %d materials" % (asset.get_name(), lod, dm.get_triangle_count(), len(mats)))
    nopts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    for k, v in (("enable_recompute_normals", False), ("enable_recompute_tangents", True), ("enable_nanite", False), ("enable_collision", False)):
        try:
            nopts.set_editor_property(k, v)
        except Exception:
            pass
    r = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(target, "%s/%s" % (OUT, sm_name), nopts)
    sm = [x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, unreal.StaticMesh)]
    sm = sm[0] if sm else None
    if sm:
        slots = []
        for m in mats:
            st = unreal.StaticMaterial()
            st.set_editor_property("material_interface", m)
            st.set_editor_property("material_slot_name", m.get_name() if m else "None")
            slots.append(st)
        sm.set_editor_property("static_materials", slots)
    log("converted: %s" % sm)
    da_path = fresh_path("DA_Fan_%02d" % i)
    da = AT.create_asset("DA_Fan_%02d" % i, OUT, unreal.AnimToTextureDataAsset, unreal.DataAssetFactory())
    texs = {}
    for key in ("BonePosition", "BoneRotation", "BoneWeight"):
        tname = "T_Fan_%02d_%s" % (i, key)
        tpath = OUT + "/" + tname
        if EAL.does_asset_exist(tpath):
            texs[key] = unreal.load_asset(tpath)  # rewritten by the bake
        else:
            texs[key] = EAL.duplicate_asset(SAMPLE_TEX + "/BoneAnimation/" + SAMPLE_NAMES[key], tpath)
    da.set_editor_property("skeletal_mesh", skel_mesh)
    da.set_editor_property("static_mesh", sm)
    da.set_editor_property("mode", unreal.AnimToTextureMode.BONE)
    da.set_editor_property("precision", unreal.AnimToTexturePrecision.SIXTEEN_BITS)
    da.set_editor_property("bone_position_texture", texs["BonePosition"])
    da.set_editor_property("bone_rotation_texture", texs["BoneRotation"])
    da.set_editor_property("bone_weight_texture", texs["BoneWeight"])
    da.set_editor_property("sample_rate", 30.0)
    seqs = []
    for a, rng in anims:
        info = unreal.AnimToTextureAnimSequenceInfo()
        info.set_editor_property("anim_sequence", a)
        if rng:
            info.set_editor_property("use_custom_range", True)
            info.set_editor_property("start_frame", rng[0])
            info.set_editor_property("end_frame", rng[1])
        seqs.append(info)
    da.set_editor_property("anim_sequences", seqs)
    log("baking")
    ok = unreal.AnimToTextureBPLibrary.animation_to_texture(da)
    EAL.save_asset(da_path)
    for t in texs.values():
        EAL.save_loaded_asset(t)
    EAL.save_loaded_asset(sm)
    info = [(x.get_editor_property("start_frame"), x.get_editor_property("end_frame")) for x in da.get_editor_property("animations")] if hasattr(da, "animations") else []
    log("baked %s: ok=%s frames=%s" % (sm_name, ok, info))
    return sm, da


# ---------------------------------------------------------------------------------------------------------------
# 4. material
# ---------------------------------------------------------------------------------------------------------------
FAN_COLOR = r"""
float team = Team, seed = Seed, tone = Tone;
float h1 = frac(seed * 13.17 + 0.11), h2 = frac(seed * 71.31 + 0.37), h3 = frac(seed * 331.7 + 0.59);
float3 skins[5] = { float3(0.48, 0.33, 0.24), float3(0.5, 0.33, 0.22), float3(0.3, 0.18, 0.11), float3(0.16, 0.09, 0.055), float3(0.5, 0.36, 0.27) };  // no near-white tones: pale fans read as chalk mannequins
float3 skin = skins[(int)(tone * 4.99)];
float3 hairc = lerp(float3(0.015, 0.01, 0.008), float3(0.22, 0.13, 0.06), step(0.8, h2));
float3 casual[8] = { float3(0.02, 0.02, 0.025), float3(0.1, 0.1, 0.11), float3(0.04, 0.06, 0.14), float3(0.35, 0.34, 0.32), float3(0.16, 0.09, 0.05), float3(0.05, 0.1, 0.05),
                     float3(0.22, 0.2, 0.15), float3(0.6, 0.6, 0.58) };  // + stone jacket, white tee
float3 club = team < 0.5 ? HomeA.rgb : (team < 1.5 ? AwayA.rgb : casual[(int)(h1 * 7.99)]);
float3 c = skin;
if (Kind < 0.5) c = skin;                                                  // body (hands, neck)
else if (Kind < 1.5)
{
	// top: half in club colours (faded replica shades, some in the second colour), the rest everyday clothes
	float3 club2 = lerp(club, float3(0.9, 0.9, 0.88), 0.55);   // away-shirt / training-top shade of the club colour
	// wide value / saturation spread: one flat club colour merged neighbouring rows into a single blue mass (heads of
	// the row in front seemed to float on the shirts behind)
	float3 shade = lerp(club, dot(club, float3(0.3, 0.59, 0.11)).xxx, lerp(0.1, 0.45, h1)) * lerp(0.42, 1.05, h2);
	c = h3 < 0.4 ? shade : (h3 < 0.52 ? club2 * lerp(0.7, 1.0, h2) : casual[(int)(frac(h1 * 3.7 + h2) * 7.99)]);
}
else if (Kind < 2.5) c = lerp(float3(0.04, 0.06, 0.12), float3(0.07, 0.07, 0.075), step(0.5, h2)); // trousers
else if (Kind < 3.5) c = float3(0.03, 0.025, 0.02);                        // shoes, socks
else if (Kind < 4.5)
{
	// head: skin with the baked groom maps painted on (hair, brows, stubble)
	// baked groom maps: coverage in alpha, strand detail in red
	float hair = saturate(HairMap.a * 3.0);  // denser coverage (bald fans read as mannequins)
	float brow = saturate(BrowMap.a);
	float beard = saturate(BeardMap.a) * step(0.5, h1);
	float strands = lerp(0.75, 1.25, HairMap.r);
	c = lerp(skin, hairc * strands, saturate(max(hair, max(brow, beard * 0.7))));
}
else c = float3(0.1, 0.075, 0.06);                                         // eyes, teeth: shaded, not black holes
return c;
"""


SAMPLE_GROOM = {}


def sampler_for(tex):
    T = unreal.MaterialSamplerType
    if not tex:
        return T.SAMPLERTYPE_COLOR
    comp = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    C = unreal.TextureCompressionSettings
    if comp in (C.TC_GRAYSCALE, C.TC_ALPHA):
        return T.SAMPLERTYPE_GRAYSCALE if srgb else T.SAMPLERTYPE_LINEAR_GRAYSCALE
    if comp == C.TC_MASKS:
        return T.SAMPLERTYPE_MASKS
    if comp == C.TC_NORMALMAP:
        return T.SAMPLERTYPE_NORMAL
    vt = False
    try:
        vt = tex.get_editor_property("virtual_texture_streaming")
    except Exception:
        pass
    if vt:
        return T.SAMPLERTYPE_VIRTUAL_COLOR if srgb else T.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR
    return T.SAMPLERTYPE_COLOR if srgb else T.SAMPLERTYPE_LINEAR_COLOR


def make_material(mpc):
    full = fresh_path("M_PK_FanVAT")
    mat = AT.create_asset("M_PK_FanVAT", OUT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("use_material_attributes", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mat.set_editor_property("used_with_static_lighting", False)

    def e(cls, x, y, **props):
        n = MEL.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    kind = e(unreal.MaterialExpressionScalarParameter, -1400, 0, parameter_name="Kind", default_value=0.0)
    cd = [e(unreal.MaterialExpressionPerInstanceCustomData, -1400, 100 + 80 * k, data_index=4 + k) for k in range(3)]
    home = e(unreal.MaterialExpressionCollectionParameter, -1400, 400, collection=mpc, parameter_name="HomeA")
    away = e(unreal.MaterialExpressionCollectionParameter, -1400, 480, collection=mpc, parameter_name="AwayA")
    uv = e(unreal.MaterialExpressionTextureCoordinate, -1700, 600)
    samples = {}
    for k, (pname, y) in enumerate((("HairAttributeMap", 600), ("EyebrowsAttributeMap", 720), ("BeardAttributeMap", 840))):
        s = e(unreal.MaterialExpressionTextureSampleParameter2D, -1400, y, parameter_name=pname)
        # default texture = a real baked groom map, sampler type matched to its format (the instances swap textures
        # of the same kind)
        tex = SAMPLE_GROOM.get(pname)
        if tex:
            s.set_editor_property("texture", tex)
        s.set_editor_property("sampler_type", sampler_for(tex))
        MEL.connect_material_expressions(uv, "", s, "UVs")
        samples[pname] = s
    node = e(unreal.MaterialExpressionCustom, -1000, 200)
    node.set_editor_property("code", FAN_COLOR)
    node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm in ("Kind", "Team", "Seed", "Tone", "HomeA", "AwayA", "HairMap", "BrowMap", "BeardMap"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    for nm, src, pin in (("Kind", kind, ""), ("Team", cd[0], ""), ("Seed", cd[1], ""), ("Tone", cd[2], ""), ("HomeA", home, ""),
                         ("AwayA", away, ""), ("HairMap", samples["HairAttributeMap"], "RGBA"), ("BrowMap", samples["EyebrowsAttributeMap"], "RGBA"),
                         ("BeardMap", samples["BeardAttributeMap"], "RGBA")):
        MEL.connect_material_expressions(src, pin, node, nm)
    make = e(unreal.MaterialExpressionMakeMaterialAttributes, -600, 200)
    MEL.connect_material_expressions(node, "", make, "BaseColor")
    rough = e(unreal.MaterialExpressionConstant, -800, 400, r=0.75)
    MEL.connect_material_expressions(rough, "", make, "Roughness")
    fn = e(unreal.MaterialExpressionMaterialFunctionCall, -300, 200)
    fn.set_editor_property("material_function", unreal.load_asset("/AnimToTexture/Materials/MaterialFunctions/MF_BoneAnimation"))
    MEL.connect_material_expressions(make, "", fn, "Material Attributes")
    MEL.connect_material_property(fn, "Result", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    log("material built")
    return mat


def classify(slot):
    s = slot.lower()
    if s == "lambert1" or "bodysynthesized" in s or "body" in s:
        return 0
    if "headsynthesized" in s:
        return 4
    if "eye" in s or "teeth" in s or "lacrimal" in s or "cartilage" in s:
        return 5
    if any(k in s for k in ("crewneck", "turtleneck", "button", "scoopneck", "shirt", "top")):
        return 1
    if any(k in s for k in ("jeans", "slacks", "skirt", "pants")):
        return 2
    if any(k in s for k in ("socks", "loafers", "oxfords", "flats", "shoe")):
        return 3
    if "head" in s:
        return 4
    return 5


HAIR_TEX = CS + "/Shared/Textures/Hair/%s/%s"
HAIRS = ["Hair_S_BuzzCut_baked", "Hair_S_AfroFade_baked", "Hair_S_HairLoss_baked", "Hair_S_Coil_baked", "Hair_S_BuzzCut_baked", "Hair_S_AfroFade_baked",
         "Hair_S_PulledBack_baked", "Hair_S_Updo_baked", "Hair_S_Coil_baked", "Hair_S_PulledBack_baked", "Hair_S_Updo_baked", "Hair_S_PulledBack_baked"]
BROWS = ["Eyebrows_M_Full_baked", "Eyebrows_M_Dense_baked", "Eyebrows_M_Thick_baked", "Eyebrows_M_Natural_baked", "Eyebrows_M_Wide_baked", "Eyebrows_M_Unruly_baked",
         "Eyebrows_M_Thin_baked", "Eyebrows_M_SlightArch_baked", "Eyebrows_M_Natural_baked", "Eyebrows_M_Thin_baked", "Eyebrows_M_SlightArch_baked", "Eyebrows_M_Natural_baked"]


def material_instances(i, sm, da, master, maps):
    # every MetaHuman head shares one UV layout, so any baked groom map fits: give each variant its own hair and brows
    maps = dict(maps)
    for key, name in (("HairAttributeMap", HAIRS[i % len(HAIRS)]), ("EyebrowsAttributeMap", BROWS[i % len(BROWS)])):
        t = unreal.load_asset(HAIR_TEX % (name, name))
        if t:
            maps[key] = t
    for k, sm_slot in enumerate(sm.static_materials):
        kind = classify(sm_slot.material_interface.get_name() if sm_slot.material_interface else str(sm_slot.material_slot_name))
        name = "MI_Fan_%02d_%d" % (i, k)
        full = fresh_path(name)
        mi = AT.create_asset(name, OUT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        MEL.set_material_instance_scalar_parameter_value(mi, "Kind", float(kind))
        for pname in ("HairAttributeMap", "EyebrowsAttributeMap", "BeardAttributeMap"):
            if maps.get(pname) and sampler_for(maps[pname]) == sampler_for(SAMPLE_GROOM.get(pname)):
                MEL.set_material_instance_texture_parameter_value(mi, pname, maps[pname])
        unreal.AnimToTextureBPLibrary.update_material_instance_from_data_asset(da, mi)
        # per-instance playback (custom data 0-3: time offset, play rate, start / end frame) instead of the
        # instance-wide frame range, so every fan has its own phase and the game can switch fans to a cheer
        MEL.set_material_instance_static_switch_parameter_value(mi, "UseDynamicParameters", False)  # False = per-instance custom data (True = particle dynamic parameters)
        MEL.set_material_instance_static_switch_parameter_value(mi, "AutoPlay", True)
        MEL.update_material_instance(mi)
        EAL.save_asset(full)
        sm.set_material(k, mi)
    EAL.save_loaded_asset(sm)


# ---------------------------------------------------------------------------------------------------------------
def main():
    if not EAL.does_directory_exist(OUT):
        EAL.make_directory(OUT)
    skeleton = unreal.load_asset(SK_BASE)
    idle = unreal.load_asset(IDLE)
    log("idle %s frames=%d" % (idle.get_name(), idle.get_editor_property("number_of_sampled_frames") if hasattr(idle, "number_of_sampled_frames") else -1))
    cheer_a = author_cheer("A_Fan_ArmsUp", skeleton, idle, "armsup")
    cheer_b = author_cheer("A_Fan_FistPump", skeleton, idle, "pump")
    mpc = unreal.load_asset("/Game/PitchKings/Materials/MPC_PK_Stadium")
    face0 = unreal.load_asset("%s/%s/Face/%s_nrw_FaceMesh" % (CS, VARIANTS[0][0], VARIANTS[0][0].split("/")[-1]))
    for sm in face0.materials:
        mi = sm.material_interface
        if isinstance(mi, unreal.MaterialInstanceConstant) and "LOD5" in mi.get_name():
            for t in mi.get_editor_property("texture_parameter_values"):
                if str(t.parameter_info.name) in ("HairAttributeMap", "EyebrowsAttributeMap", "BeardAttributeMap") and t.parameter_value:
                    SAMPLE_GROOM[str(t.parameter_info.name)] = t.parameter_value
    log("groom maps: %s" % {k: (v.get_name(), str(v.get_editor_property("compression_settings")), v.get_editor_property("srgb")) for k, v in SAMPLE_GROOM.items()})
    master = make_material(mpc)
    anims = [(idle, (0, 179)), (cheer_a, None), (cheer_b, None)]
    count = 1 if "-PKOne" in unreal.SystemLibrary.get_command_line() else len(VARIANTS)
    for i, v in enumerate(VARIANTS[:count]):
        r = build_variant(i, v, skeleton)
        if not r:
            continue
        skel_mesh, maps, parts = r
        sm, da = bake(i, skel_mesh, anims, parts)
        material_instances(i, sm, da, master, maps)
    log("done")


main()
