"""
M_PK_HairCards: the players' groom-card material. The pack's card material clips the coverage hard (cut-paper ends,
black blocks); this one dithers the coverage (TSR resolves it into soft strand ends), darkens the roots and the inner
cards from the attribute / depth maps and takes a plain HairColor from PKFootballer. One instance per style with that
groom's atlases, assigned to the card slots of /Game/PitchKings/Characters/Hair/SM_PK_Hair_* (run after
build_player_hair.py).

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_hair_material.py
"""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
OUT = "/Game/PitchKings/Characters/Hair"
CARDS = "/Game/CitySampleCrowd/Character/Shared/Materials/MetaHuman/HairCards/"
TEX = "/Game/CitySampleCrowd/Character/Shared/Textures/Hair/"
# mesh style -> the pack's card instance (its atlases), or explicit atlas paths
STYLES = {
    "AfroFade": CARDS + "MI_Hair_Cards_S_AfroFade",
    "CurlyFade": CARDS + "MI_Hair_Cards_M_CurlyFade",
    "SideSweptFringe": CARDS + "MI_Hair_Cards_M_SideSweptFringe",
    "PulledBack": {"OpacityTexture": TEX + "PulledBack/Hair_S_PulledBack_Opacity", "AttributeTexture": TEX + "PulledBack/Hair_S_PulledBack_Attribute",
                   "DepthTexture": TEX + "PulledBack/Hair_S_PulledBack_Depth"},
    "HairLoss": CARDS + "MI_Hair_Cards_S_HairLoss",
    "Coil": CARDS + "MI_Hair_Cards_S_Coil",
    "BeardFull": CARDS + "MI_Beard_Cards_S_Full",
    "ChinStrap": CARDS + "MI_Goatee_Cards_S_ChinStrap",
}

COLOR = r"""
// root -> tip from the attribute map (R), per-strand variation (G), inner cards darker (depth map)
float t = saturate(Attr.r);
float seed = Attr.g;
float3 c = HairColor.rgb * lerp(0.62, 1.3, t) * lerp(0.85, 1.15, seed);
c *= lerp(0.6, 1.0, saturate(Depth.r));
return c;
"""


def log(s):
    unreal.log("[HairMat] " + s)


def sampler_for(tex):
    T = unreal.MaterialSamplerType
    C = unreal.TextureCompressionSettings
    comp = tex.get_editor_property("compression_settings")
    vt = tex.get_editor_property("virtual_texture_streaming")
    srgb = tex.get_editor_property("srgb")
    if comp == C.TC_MASKS:
        return T.SAMPLERTYPE_VIRTUAL_MASKS if vt else T.SAMPLERTYPE_MASKS
    if comp in (C.TC_GRAYSCALE, C.TC_ALPHA):
        return (T.SAMPLERTYPE_VIRTUAL_LINEAR_GRAYSCALE if vt else T.SAMPLERTYPE_LINEAR_GRAYSCALE) if not srgb else (T.SAMPLERTYPE_VIRTUAL_GRAYSCALE if vt else T.SAMPLERTYPE_GRAYSCALE)
    if vt:
        return T.SAMPLERTYPE_VIRTUAL_COLOR if srgb else T.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR
    return T.SAMPLERTYPE_COLOR if srgb else T.SAMPLERTYPE_LINEAR_COLOR


def atlases(style):
    src = STYLES[style]
    if isinstance(src, dict):
        return {k: unreal.load_asset(v) for k, v in src.items()}
    mi = unreal.load_asset(src)
    return {str(t.parameter_info.name): t.parameter_value for t in mi.get_editor_property("texture_parameter_values")}


def make_master(sample):
    path = OUT + "/M_PK_HairCards"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = AT.create_asset("M_PK_HairCards", OUT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("dither_opacity_mask", True)   # stochastic coverage: soft strand ends under TSR
    mat.set_editor_property("opacity_mask_clip_value", 0.35)
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    nodes = {}
    for k, name in enumerate(("OpacityTexture", "AttributeTexture", "DepthTexture")):
        s = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 200 * k)
        s.set_editor_property("parameter_name", name)
        s.set_editor_property("texture", sample[name])
        s.set_editor_property("sampler_type", sampler_for(sample[name]))
        MEL.connect_material_expressions(uv, "", s, "UVs")
        nodes[name] = s
    col = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 700)
    col.set_editor_property("parameter_name", "HairColor")
    col.set_editor_property("default_value", unreal.LinearColor(0.03, 0.02, 0.015, 1))
    cn = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 100)
    cn.set_editor_property("code", COLOR)
    cn.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm in ("Attr", "Depth", "HairColor"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    cn.set_editor_property("inputs", ins)
    MEL.connect_material_expressions(nodes["AttributeTexture"], "RGBA", cn, "Attr")
    MEL.connect_material_expressions(nodes["DepthTexture"], "RGBA", cn, "Depth")
    MEL.connect_material_expressions(col, "", cn, "HairColor")
    MEL.connect_material_property(cn, "", MP.MP_BASE_COLOR)
    # coverage thinned toward the tips and on cards seen edge-on (those drew the faceted "glass shard" silhouettes)
    cam = MEL.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, 900)
    nrm = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 1000)
    op = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 500)
    op.set_editor_property("code", "float facing = saturate(abs(dot(normalize(Nrm), normalize(Cam))) * 2.5 + 0.15);"
                                   " float tips = 1.0 - 0.55 * smoothstep(0.6, 1.0, Attr.r);"
                                   " return Cov.r * facing * tips * 0.85;")  # the helmet base fills the gaps: thinner cards read finer
    op.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ins2 = []
    for nm in ("Cov", "Attr", "Nrm", "Cam"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins2.append(ci)
    op.set_editor_property("inputs", ins2)
    MEL.connect_material_expressions(nodes["OpacityTexture"], "RGBA", op, "Cov")
    MEL.connect_material_expressions(nodes["AttributeTexture"], "RGBA", op, "Attr")
    MEL.connect_material_expressions(nrm, "", op, "Nrm")
    MEL.connect_material_expressions(cam, "", op, "Cam")
    MEL.connect_material_property(op, "", MP.MP_OPACITY_MASK)
    # shading normal: mostly the direction from the head centre (the hair lit as one volume), a little of the card's
    # own normal. Per-card normals lit every card differently and read as shattered glass.
    mat.set_editor_property("tangent_space_normal", False)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 1200)
    obj = MEL.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -900, 1300)
    fwd = MEL.create_material_expression(mat, unreal.MaterialExpressionTransform, -900, 1400)
    fwd.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    fwd.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    upc = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1100, 1400)
    upc.set_editor_property("constant", unreal.LinearColor(1, 0, 0, 0))  # head bone +X runs up the skull
    MEL.connect_material_expressions(upc, "", fwd, "")
    vn = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 1250)
    vn.set_editor_property("code", "float3 c = Obj + normalize(Up) * 9.0; float3 v = normalize(WP - c);"
                                   " float3 n = normalize(Nrm); n *= sign(dot(n, v) + 1e-4);"
                                   " return normalize(lerp(v, n, 0.2));")
    vn.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins3 = []
    for nm in ("WP", "Obj", "Up", "Nrm"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins3.append(ci)
    vn.set_editor_property("inputs", ins3)
    nrm2 = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 1500)
    MEL.connect_material_expressions(wp, "", vn, "WP")
    MEL.connect_material_expressions(obj, "", vn, "Obj")
    MEL.connect_material_expressions(fwd, "", vn, "Up")
    MEL.connect_material_expressions(nrm2, "", vn, "Nrm")
    MEL.connect_material_property(vn, "", MP.MP_NORMAL)
    r = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    r.set_editor_property("r", 0.58)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    sp = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 500)
    sp.set_editor_property("r", 0.3)
    MEL.connect_material_property(sp, "", MP.MP_SPECULAR)
    MEL.recompile_material(mat)
    EAL.save_asset(path)
    log("master built")
    return mat


try:
    master = make_master(atlases("AfroFade"))
    for style in STYLES:
        tex = atlases(style)
        mi_path = "%s/MI_PK_Hair_%s" % (OUT, style)
        if EAL.does_asset_exist(mi_path):
            EAL.delete_asset(mi_path)
        mi = AT.create_asset("MI_PK_Hair_%s" % style, OUT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        for name in ("OpacityTexture", "AttributeTexture", "DepthTexture"):
            if tex.get(name):
                MEL.set_material_instance_texture_parameter_value(mi, name, tex[name])
        MEL.update_material_instance(mi)
        EAL.save_asset(mi_path)
        mesh = unreal.load_asset("%s/SM_PK_Hair_%s" % (OUT, style))
        if not mesh:
            continue
        done = []
        for i, slot in enumerate(mesh.static_materials):
            mat = slot.material_interface
            name = mat.get_name() if mat else ""
            # card slots (not the low-LOD helmet, which keeps the pack's helmet material)
            if "Helmet" not in name or style == "PulledBack" and i == 0:
                mesh.set_material(i, mi)
                done.append(i)
        EAL.save_asset(mesh.get_path_name().split(".")[0])
        log("%s: card slots %s -> %s" % (style, done, mi.get_name()))
except Exception as ex:
    import traceback
    unreal.log_error("[HairMat] FAILED %s %s" % (ex, traceback.format_exc()))
