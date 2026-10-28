"""Materials for the MetaHuman player parts (City Sample Crowds pack): the head, the shirt and the shorts. The parts
follow the animated body (UE5 mannequin) with UPKCopyPoseAnimInstance; the body itself keeps M_PK_Kit for the skin,
socks and boots and hides whatever the cloth covers. Run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_player_mh.py

Parameters (set per player by APKFootballer::ApplyKit):
  M_PK_PlayerCloth  Primary, Secondary, Trim, Shorts, NumberColor, KitA (pattern, number, sleeve, shorts), KitB (.., .., keeper, ..), Part (0 shirt, 1 shorts, 2 hidden)
  M_PK_PlayerSkin   Skin, Hair (colours), HairAttributeMap, EyebrowsAttributeMap, BeardAttributeMap, Beard (0..1)
  M_PK_PlayerEye    Iris (colour)
"""
import json
import os
import unreal

MAT = "/Game/PitchKings/Materials/Player"
CS = "/Game/CitySampleCrowd/Character"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

# KitLib (region classifier, block digits, shirt patterns) from the mannequin kit material, shared code
_kit = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_kit_material.py"), encoding="utf-8").read()
KITLIB = _kit[_kit.index('LIB = r"""') + len('LIB = r"""'):]
KITLIB = KITLIB[:KITLIB.index('"""')]


def log(s):
    unreal.log("[PlayerMH] " + s)


def fresh(name):
    full = MAT + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return AT.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew()), full


def e(mat, cls, x, y, **props):
    n = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def custom(mat, code, out, x, y, inputs, desc):
    node = e(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out)
    node.set_editor_property("description", desc)
    ins = []
    for nm, _src, _pin in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    for nm, src, pin in inputs:
        MEL.connect_material_expressions(src, pin, node, nm)
    return node


def vec(mat, name, val, x, y):
    return e(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*val))


def scal(mat, name, val, x, y):
    return e(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=val)


def tex_param(mat, name, tex, x, y, uv=None, sampler=None):
    s = e(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name)
    if tex:
        s.set_editor_property("texture", tex)
    if sampler is not None:
        s.set_editor_property("sampler_type", sampler)
    if uv is not None:
        MEL.connect_material_expressions(uv, "", s, "UVs")
    return s


def sampler_for(tex):
    T = unreal.MaterialSamplerType
    C = unreal.TextureCompressionSettings
    comp = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    vt = False
    try:
        vt = tex.get_editor_property("virtual_texture_streaming")
    except Exception:
        pass
    if comp == C.TC_NORMALMAP:
        return T.SAMPLERTYPE_VIRTUAL_NORMAL if vt else T.SAMPLERTYPE_NORMAL
    if comp == C.TC_ALPHA:  # single channel, read from .a
        return T.SAMPLERTYPE_VIRTUAL_ALPHA if vt else T.SAMPLERTYPE_ALPHA
    if comp == C.TC_GRAYSCALE:
        return (T.SAMPLERTYPE_VIRTUAL_GRAYSCALE if srgb else T.SAMPLERTYPE_VIRTUAL_LINEAR_GRAYSCALE) if vt else (T.SAMPLERTYPE_GRAYSCALE if srgb else T.SAMPLERTYPE_LINEAR_GRAYSCALE)
    if comp == C.TC_MASKS:
        return T.SAMPLERTYPE_VIRTUAL_MASKS if vt else T.SAMPLERTYPE_MASKS
    if vt:
        return T.SAMPLERTYPE_VIRTUAL_COLOR if srgb else T.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR
    return T.SAMPLERTYPE_COLOR if srgb else T.SAMPLERTYPE_LINEAR_COLOR


def preskinned(mat):
    raw = e(mat, unreal.MaterialExpressionPreSkinnedPosition, -1800, -200)
    interp = e(mat, unreal.MaterialExpressionVertexInterpolator, -1600, -200)
    if not MEL.connect_material_expressions(raw, "", interp, ""):
        MEL.connect_material_expressions(raw, "", interp, "VS")
    rawn = e(mat, unreal.MaterialExpressionPreSkinnedNormal, -1800, -100)
    interpn = e(mat, unreal.MaterialExpressionVertexInterpolator, -1600, -100)
    if not MEL.connect_material_expressions(rawn, "", interpn, ""):
        MEL.connect_material_expressions(rawn, "", interpn, "VS")
    return interp, interpn


# ---------------------------------------------------------------------------------------------------------------
# Cloth (shirt / shorts). P is the reference pose of the City Sample "tal" meshes, which is exactly the mannequin's
# (+Y forward, +X left, Z up, head bone at 162.6 cm), so the KitLib patterns and numbers line up with the body paint.
# ---------------------------------------------------------------------------------------------------------------
FONT = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "art", "kit_font.json")))
TEXT_LIB = r"""
// Signed-distance kit lettering (T_PK_KitFont, 8 x 5 cells; Tools/art/make_kit_font.py). Codes: 0-9 digits, 10-35 A-Z,
// 36 '.', 37 '-', 38 apostrophe, 39 space.
struct KitText
{
	float Adv(int c)
	{
		const float A[40] = { %s };
		return A[clamp(c, 0, 39)];
	}
	// coverage of a line of text centred on q = 0 (cm, x to the reader's right, y up; h = cap height in cm)
	float Draw(Texture2D T, SamplerState S, float2 q, float h, float4 A, float4 B, float4 C, float n, float track, out float outline)
	{
		float codes[12] = { A.x, A.y, A.z, A.w, B.x, B.y, B.z, B.w, C.x, C.y, C.z, C.w };
		int cnt = (int)clamp(n, 0.0, 12.0);
		float total = 0.0;
		for (int i = 0; i < cnt; i++) total += Adv((int)codes[i]) * h * track;
		float cell = h / %.4f;
		float2 cont = q / cell / float2(8.0, 5.0);
		float2 gx = ddx(cont), gy = ddy(cont);
		float x = q.x + total * 0.5, cum = 0.0, d = 0.0, hit = 0.0;
		for (int j = 0; j < cnt; j++)
		{
			float w = Adv((int)codes[j]) * h * track;
			float2 uv = float2(0.5 + (x - cum - w * 0.5) / cell, 0.5 - q.y / cell);
			if (x >= cum && x < cum + w && uv.y > 0.0 && uv.y < 1.0 && uv.x > 0.0 && uv.x < 1.0)
			{
				int c = (int)codes[j];
				d = T.SampleGrad(S, (float2(c %% 8, c / 8) + uv) / float2(8.0, 5.0), gx, gy).r;
				hit = 1.0;
			}
			cum += w;
		}
		float texels = 0.5 * (length(gx) * 1024.0 + length(gy) * 640.0);   // atlas texels per screen pixel
		float fw = clamp(texels / (2.0 * %.1f), 0.01, 0.2);                // distance units per pixel (spread in texels)
		float vis = hit * saturate(2.0 - texels / 6.0);                    // glyphs under a few pixels fade out
		outline = smoothstep(0.36 - fw, 0.36 + fw, d) * vis;
		return smoothstep(0.5 - fw, 0.5 + fw, d) * vis;
	}
};
""" % (", ".join("%.4f" % a for a in FONT["advance"]), FONT["glyph"], FONT["spread"] * 128.0)

CLOTH_LIB = KITLIB + TEXT_LIB + r"""
struct MH
{
	float2 Seg(float3 p, float3 a, float3 b)
	{
		float3 ab = b - a;
		float t = saturate(dot(p - a, ab) / dot(ab, ab));
		return float2(length(p - (a + ab * t)), t);
	}
	// t along the arm: 0 shoulder .. 1 elbow .. 2 wrist; -1 = not on the arm (torso, collar, hem)
	float ArmT(float3 p)
	{
		float3 q = float3(abs(p.x), p.y, p.z);
		if (q.x < 14.0) return -1;
		float2 u = Seg(q, float3(19.0, -2.6, 143.6), float3(35.0, -1.9, 120.9));
		float2 l = Seg(q, float3(35.0, -1.9, 120.9), float3(47.8, 15.7, 104.5));
		float d = min(u.x, l.x);
		// far from the arm bones only counts as torso near the body: loose sleeve folds round the elbow (well outside the
		// torso) stayed visible as slivers across the forearm
		if ((d > 9.5 && q.x < 24.0) || (u.x < l.x && u.y < 0.02 && q.x < 19.0)) return -1;
		return u.x < l.x ? u.y : 1.0 + l.y;
	}
	// crew-neck rib: the narrow band hugging the neck (the chest and shoulders fall away from the neck quickly)
	bool Collar(float3 p)
	{
		return length(p.xy - float2(0.0, -0.5)) < 8.8 && p.z > 143.0;
	}
};
KitLib K;
MH H;
KitText X;
float3 p = P;
float3 m = p;
float3 n = normalize(N);
bool keeper = KitB.z > 0.5;
float armT = H.ArmT(p);
float sleeveEnd = keeper ? 2.0 : saturate(KitA.z) * 0.95 + 0.05; // keeper sleeves run into the glove straps
float hemZ = 93.5 - (0.33 + 0.35 * saturate(KitA.w)) * (93.5 - 50.3); // below the pocket linings
"""

CLOTH_COLOR = CLOTH_LIB + r"""
float3 c;
if (Part < 0.5)
{
	// stripes follow the arc length round the torso (straight x stretched them into blobs on the side panels);
	// radius 14.32 cm makes the girth exactly 90 cm, a whole number of stripe and pinstripe periods
	bool striped = (KitA.x > 0.5 && KitA.x < 1.5) || KitA.x > 4.5;
	float3 mp = striped ? float3(atan2(m.x, m.y - 2.0) * 14.32, m.y, m.z) : m;
	c = armT < 0.0 ? K.ShirtColor(mp, KitA.x, Primary.rgb, Secondary.rgb) : (striped || keeper ? Primary.rgb : K.ShirtColor(m, KitA.x, Primary.rgb, Secondary.rgb));
	// lettering in the kit typeface (mannequin coordinates; seen from behind, the reader's right is the player's right: -X)
	float tens = floor(KitA.y / 10.0), ones = KitA.y - tens * 10.0;
	float4 num = KitA.y >= 10.0 ? float4(tens, ones, 0, 0) : float4(ones, 0, 0, 0);
	float numLen = KitA.y >= 10.0 ? 2.0 : 1.0;
	float edge;
	if (armT < 0.0 && n.y < -0.2 && KitA.y > 0.5)
	{
		float a = X.Draw(KitFont, KitFontSampler, float2(-m.x, m.z - 128.5), 18.0, num, 0, 0, numLen, 1.1, edge);
		c = lerp(c, Secondary.rgb * 0.45, edge * (1.0 - a) * 0.45);           // thin, subtle outline (a strong one read as a sticker)
		c = lerp(c, NumberColor.rgb, a);
		float an = X.Draw(KitFont, KitFontSampler, float2(-m.x, m.z - 143.0), 3.4, NameA, NameB, NameC, NameLen, 1.12, edge);
		c = lerp(c, NumberColor.rgb, an);
	}
	if (armT < 0.0 && n.y > 0.3)
	{
		if (KitA.y > 0.5)
		{
			float a = X.Draw(KitFont, KitFontSampler, float2(m.x, m.z - 138.0), 9.5, num, 0, 0, numLen, 1.1, edge);
			c = lerp(c, NumberColor.rgb, a);
		}
		// club sponsor across the chest (sponsor wordmark atlas, one 8:1 row per brand)
		float arcX = atan2(m.x, m.y - 2.0) * 14.32;   // same arc mapping as the stripes (planar x skewed it)
		float2 su = float2(0.5 + arcX / 34.0, (130.5 - m.z) / 4.25);
		if (su.x > 0.0 && su.x < 1.0 && su.y > 0.0 && su.y < 1.0)
		{
			float2 cont = float2(arcX / 34.0, -m.z / 4.25 / 8.0);
			float mark = Sponsors.SampleGrad(SponsorsSampler, float2(su.x, (Sponsor + su.y) / 8.0), ddx(cont), ddy(cont)).a;
			c = lerp(c, NumberColor.rgb, mark);
		}
		// maker's mark on the right chest
		float2 mq = float2(-m.x - 9.0, m.z - 140.0);
		c = lerp(c, Trim.rgb, step(abs(mq.x), 1.7) * step(abs(mq.y - 0.9 + abs(mq.x) * 0.7), 0.32));
	}
	if (armT < 0.0 && n.y > 0.3)
	{
		float2 q = float2(m.x - 9.0, m.z - 139.5);
		float shield = max(abs(q.x) - 2.2 + max(0.0, -q.y) * 0.45, abs(q.y) - 2.6);
		float inner = smoothstep(0.2, -0.2, shield + 0.45);
		c = lerp(c, Trim.rgb, smoothstep(0.2, -0.2, shield));
		c = lerp(c, Secondary.rgb, inner);
		c = lerp(c, Trim.rgb, inner * step(abs(q.y - 0.4 + abs(q.x) * 0.7), 0.4));
	}
	// collar band and sleeve hems in the trim colour
	if (armT < 0.0 && H.Collar(p)) c = Trim.rgb;
	if (armT >= 0.0 && armT > sleeveEnd - 0.07 && armT <= sleeveEnd) c = Trim.rgb;
}
else
{
	c = Shorts.rgb;
	if (p.z < hemZ + 2.0) c = Trim.rgb;                        // hem
}
return c;
"""

CLOTH_MASK = CLOTH_LIB + r"""
if (Part > 1.5) return 0.0;
if (Part < 0.5) return (armT < 0.0 || armT <= sleeveEnd) ? 1.0 : 0.0;
return p.z >= hemZ && p.z < 101.0 ? 1.0 : 0.0; // the waistband stays under the shirt
"""


def make_cloth():
    mat, full = fresh("M_PK_PlayerCloth")
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    P, N = preskinned(mat)
    names = [("Primary", (0.02, 0.12, 0.6, 1)), ("Secondary", (0.01, 0.03, 0.15, 1)), ("Trim", (0.9, 0.9, 0.9, 1)),
             ("Shorts", (0.9, 0.9, 0.9, 1)), ("NumberColor", (0.95, 0.95, 0.95, 1)), ("KitA", (0, 10, 0.55, 0.45)), ("KitB", (0, 0, 0, 1))]
    src = {"P": (P, ""), "N": (N, "")}
    y = 0
    for nm, val in names:
        v = vec(mat, nm, val, -1400, y)
        if nm in ("KitA", "KitB"):
            app = e(mat, unreal.MaterialExpressionAppendVector, -1200, y)
            MEL.connect_material_expressions(v, "", app, "A")
            MEL.connect_material_expressions(v, "A", app, "B")
            src[nm] = (app, "")
        else:
            src[nm] = (v, "")
        y += 100
    src["Part"] = (scal(mat, "Part", 0.0, -1400, y), "")
    order = ["P", "N", "Primary", "Secondary", "Trim", "Shorts", "NumberColor", "KitA", "KitB", "Part"]
    ins = [(k, src[k][0], src[k][1]) for k in order]
    # lettering: name codes (12 chars in three vectors), sponsor row, the two atlases as texture objects
    extra = []
    for k, nm in enumerate(("NameA", "NameB", "NameC")):
        v = vec(mat, nm, (39, 39, 39, 39), -1900, 1300 + 100 * k)
        app = e(mat, unreal.MaterialExpressionAppendVector, -1700, 1300 + 100 * k)  # RGB + A: the code reads all four chars
        MEL.connect_material_expressions(v, "", app, "A")
        MEL.connect_material_expressions(v, "A", app, "B")
        extra.append((nm, app, ""))
    extra.append(("NameLen", scal(mat, "NameLen", 0.0, -1700, 1600), ""))
    extra.append(("Sponsor", scal(mat, "Sponsor", 0.0, -1700, 1700), ""))
    for nm, path, y2 in (("KitFont", "/Game/PitchKings/Materials/Player/T_PK_KitFont", 1800), ("Sponsors", "/Game/PitchKings/Stadium/T_PK_Sponsors", 1950)):
        t = e(mat, unreal.MaterialExpressionTextureObjectParameter, -1700, y2, parameter_name=nm)
        tex = unreal.load_asset(path)
        t.set_editor_property("texture", tex)
        t.set_editor_property("sampler_type", sampler_for(tex))
        extra.append((nm, t, ""))
    col = custom(mat, CLOTH_COLOR, F3, -800, 0, ins + extra, "Kit colour")
    mask = custom(mat, CLOTH_MASK, F1, -800, 300, ins, "Cloth cut")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(mask, "", MP.MP_OPACITY_MASK)
    # cloth folds from the pack's normal map + a fine knit, roughness from the fabric
    uv = e(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 900)
    nmap = unreal.load_asset(CS + "/Male/NormalWeight/Textures/longSleevedTee/T_m_tal_nrw_longSleeve_N")
    ntex = tex_param(mat, "ClothNormal", nmap, -1100, 900, uv, sampler_for(nmap))
    knit = unreal.load_asset(CS + "/Shared/Materials/Clothing/Textures/knit/T_hoodie_doubleKnit_front_N")
    tile = e(mat, unreal.MaterialExpressionMultiply, -1250, 1050)
    MEL.connect_material_expressions(uv, "", tile, "A")
    tile.set_editor_property("const_b", 70.0)  # a fine performance mesh weave, not a jumper knit
    ktex = tex_param(mat, "KnitNormal", knit, -1100, 1050, tile, sampler_for(knit) if knit else None)
    kflat = e(mat, unreal.MaterialExpressionConstant3Vector, -950, 1150, constant=unreal.LinearColor(0, 0, 1, 1))
    klerp = e(mat, unreal.MaterialExpressionLinearInterpolate, -900, 1050)
    MEL.connect_material_expressions(kflat, "", klerp, "A")
    MEL.connect_material_expressions(ktex, "RGB", klerp, "B")
    klerp.set_editor_property("const_alpha", 0.45)
    blend = e(mat, unreal.MaterialExpressionMaterialFunctionCall, -800, 950)
    blend.set_editor_property("material_function", unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals"))
    # the borrowed tee's folds at 40 %: full strength they read as dirty blotches on bright kits
    nflat = e(mat, unreal.MaterialExpressionConstant3Vector, -950, 850, constant=unreal.LinearColor(0, 0, 1, 1))
    nlerp = e(mat, unreal.MaterialExpressionLinearInterpolate, -900, 900)
    MEL.connect_material_expressions(nflat, "", nlerp, "A")
    MEL.connect_material_expressions(ntex, "RGB", nlerp, "B")
    nlerp.set_editor_property("const_alpha", 0.4)
    MEL.connect_material_expressions(nlerp, "", blend, "BaseNormal")
    MEL.connect_material_expressions(klerp, "", blend, "AdditionalNormal")
    MEL.connect_material_property(blend, "", MP.MP_NORMAL)
    r = custom(mat, "return Part > 0.5 ? 0.82 : 0.62;", F1, -500, 600, [("Part", src["Part"][0], "")], "Fabric roughness")  # matte shorts, shirt sheen
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    # the collar band stands a few mm off the neck: lying on the face's neck it cut through it in jagged spikes
    rawp = e(mat, unreal.MaterialExpressionPreSkinnedPosition, -900, 1400)
    lift = custom(mat, "if (Part > 0.5) return 0.0;"
                       " float r = length(P.xy - float2(0.0, -0.5));"
                       " return 0.6 * smoothstep(140.0, 144.0, P.z) * (1.0 - smoothstep(9.0, 11.5, r));",
                  F1, -700, 1400, [("P", rawp, ""), ("Part", src["Part"][0], "")], "Collar lift (cm)")
    nws = e(mat, unreal.MaterialExpressionVertexNormalWS, -700, 1500)
    mul = e(mat, unreal.MaterialExpressionMultiply, -500, 1450)
    MEL.connect_material_expressions(lift, "", mul, "A")
    MEL.connect_material_expressions(nws, "", mul, "B")
    MEL.connect_material_property(mul, "", MP.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    log("cloth material built (normal %s, knit %s)" % (nmap.get_name() if nmap else None, knit.get_name() if knit else None))
    for name, part, normal in (("MI_PK_Shirt", 0, nmap), ("MI_PK_Shorts", 1, unreal.load_asset(CS + "/Male/NormalWeight/Textures/slacks/T_m_nrw_slacks_N")), ("MI_PK_ClothHidden", 2, None)):
        p = MAT + "/" + name
        if EAL.does_asset_exist(p):
            EAL.delete_asset(p)
        mi = AT.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, mat)
        MEL.set_material_instance_scalar_parameter_value(mi, "Part", float(part))
        if normal and sampler_for(normal) == sampler_for(nmap):
            MEL.set_material_instance_texture_parameter_value(mi, "ClothNormal", normal)
        MEL.update_material_instance(mi)
        EAL.save_asset(p)


# ---------------------------------------------------------------------------------------------------------------
# Skin (MetaHuman head) and eyes
# ---------------------------------------------------------------------------------------------------------------
SKIN = r"""
float hair = saturate(HairMap.a * 2.2); // a denser painted base, so gaps between the cards show hair, not skin
float brow = saturate(BrowMap.a * 1.8);
float beard = saturate(BeardMap.a) * Beard;
float strands = lerp(0.8, 1.2, HairMap.r);
float ao = saturate(AO.a);
// warm, darker creases (eyelids, nostrils, lips, ears) from the MetaHuman occlusion map
float3 c = Skin.rgb * lerp(float3(0.72, 0.6, 0.56), float3(1.0, 1.0, 1.0), ao);  // lighter creases: deep ones aged the faces
// a little redness on the cheeks, nose and ears, darker around the eyes
c = lerp(c, Hair.rgb * strands * 0.55, saturate(max(hair, max(brow, beard * 0.85)))); // as dark as the cards above it
return c;
"""


def make_skin():
    mat, full = fresh("M_PK_PlayerSkin")
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_PREINTEGRATED_SKIN)
    uv = e(mat, unreal.MaterialExpressionTextureCoordinate, -1600, 300)
    skin = vec(mat, "Skin", (0.55, 0.38, 0.27, 1), -1300, 0)
    hairc = vec(mat, "Hair", (0.02, 0.015, 0.01, 1), -1300, 100)
    beard = scal(mat, "Beard", 0.0, -1300, 200)
    maps = {}
    T = CS + "/Shared/Textures/Hair/%s/%s"
    for k, (pname, tname) in enumerate((("HairAttributeMap", "Hair_S_BuzzCut_baked"), ("EyebrowsAttributeMap", "Eyebrows_M_Full_baked"), ("BeardAttributeMap", "Beard_S_Stubble_baked"))):
        t = unreal.load_asset(T % (tname, tname))
        maps[pname] = tex_param(mat, pname, t, -1300, 300 + 120 * k, uv, sampler_for(t))
    occ = unreal.load_asset(CS + "/Shared/Textures/MH_head_occlusion_map")
    ao = tex_param(mat, "Occlusion", occ, -1300, 700, uv, sampler_for(occ))
    col = custom(mat, SKIN, F3, -800, 100, [("Skin", skin, ""), ("Hair", hairc, ""), ("Beard", beard, ""), ("HairMap", maps["HairAttributeMap"], "RGBA"),
                                            ("BrowMap", maps["EyebrowsAttributeMap"], "RGBA"), ("BeardMap", maps["BeardAttributeMap"], "RGBA"), ("AO", ao, "RGBA")], "Skin colour")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(ao, "A", MP.MP_AMBIENT_OCCLUSION)
    # per-head sculpt detail (the head's own normal map, set per player) plus fine pores
    hn = unreal.load_asset(CS + "/Male/m_001/Textures/m_001_nrw_LODnormals")
    head_n = tex_param(mat, "HeadNormal", hn, -1300, 900, uv, sampler_for(hn))
    micro = unreal.load_asset(CS + "/Shared/Textures/T_SkinMicroNormal")
    tile = e(mat, unreal.MaterialExpressionMultiply, -1450, 1100)
    MEL.connect_material_expressions(uv, "", tile, "A")
    tile.set_editor_property("const_b", 22.0)
    mn = tex_param(mat, "MicroNormal", micro, -1300, 1100, tile, sampler_for(micro))
    flat = e(mat, unreal.MaterialExpressionConstant3Vector, -1100, 1200, constant=unreal.LinearColor(0, 0, 1, 1))
    lerpn = e(mat, unreal.MaterialExpressionLinearInterpolate, -900, 1100)
    MEL.connect_material_expressions(flat, "", lerpn, "A")
    MEL.connect_material_expressions(mn, "RGB", lerpn, "B")
    lerpn.set_editor_property("const_alpha", 0.3)
    blend = e(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, 1000)
    blend.set_editor_property("material_function", unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals"))
    # the scans' wrinkles at half strength (full strength every player read 50+)
    hflat = e(mat, unreal.MaterialExpressionConstant3Vector, -1100, 1000, constant=unreal.LinearColor(0, 0, 1, 1))
    hlerp = e(mat, unreal.MaterialExpressionLinearInterpolate, -1000, 950)
    MEL.connect_material_expressions(hflat, "", hlerp, "A")
    MEL.connect_material_expressions(head_n, "RGB", hlerp, "B")
    hlerp.set_editor_property("const_alpha", 0.5)
    MEL.connect_material_expressions(hlerp, "", blend, "BaseNormal")
    MEL.connect_material_expressions(lerpn, "", blend, "AdditionalNormal")
    MEL.connect_material_property(blend, "", MP.MP_NORMAL)
    # rougher in the creases, a touch of sheen on the forehead / nose / cheekbones
    rough = custom(mat, "return lerp(0.62, 0.46, saturate(AO.a));", F1, -600, 400, [("AO", ao, "RGBA")], "Roughness")
    MEL.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    spec = e(mat, unreal.MaterialExpressionConstant, -600, 500, r=0.26)  # less sun glare on foreheads
    MEL.connect_material_property(spec, "", MP.MP_SPECULAR)
    op = e(mat, unreal.MaterialExpressionConstant, -600, 600, r=0.05)  # ears glowed orange
    MEL.connect_material_property(op, "", MP.MP_OPACITY)
    # scatter tinted by the skin itself (a fixed orange turned every face orange in the sun)
    ssc = custom(mat, "return saturate(Skin.rgb * float3(0.85, 0.66, 0.6));", F3, -600, 700, [("Skin", skin, "")], "Scatter colour")
    MEL.connect_material_property(ssc, "", MP.MP_SUBSURFACE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    log("skin material built (occlusion %s, head normal %s)" % (occ is not None, hn is not None))


EYE = r"""
float2 d = UV - float2(0.5, 0.5);
float r = length(d);
float iris = 1.0 - smoothstep(0.115, 0.13, r);
float pupil = 1.0 - smoothstep(0.045, 0.055, r);
float3 c = lerp(float3(0.38, 0.36, 0.34), Iris.rgb * (0.7 + 0.6 * frac(atan2(d.y, d.x) * 6.0)), iris);
c = lerp(c, float3(0.01, 0.01, 0.01), pupil);
// lid shadow: the eyeball darkens toward the edges, so the eye sits in its socket
return c * lerp(1.0, 0.35, smoothstep(0.18, 0.42, r));
"""


def make_eye_and_hidden():
    mat, full = fresh("M_PK_PlayerEye")
    mat.set_editor_property("used_with_skeletal_mesh", True)
    uv = e(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    iris = vec(mat, "Iris", (0.12, 0.06, 0.02, 1), -900, 100)
    col = custom(mat, EYE, F3, -500, 0, [("UV", uv, ""), ("Iris", iris, "")], "Eye")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    r = e(mat, unreal.MaterialExpressionConstant, -300, 200, r=0.3)  # a mirror finish reflected the sky and blanked the iris
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    sp = e(mat, unreal.MaterialExpressionConstant, -300, 300, r=0.2)
    MEL.connect_material_property(sp, "", MP.MP_SPECULAR)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    mat, full = fresh("M_PK_PlayerHidden")
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    z = e(mat, unreal.MaterialExpressionConstant, -300, 0, r=0.0)
    MEL.connect_material_property(z, "", MP.MP_OPACITY_MASK)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    mat, full = fresh("M_PK_PlayerTeeth")
    mat.set_editor_property("used_with_skeletal_mesh", True)
    c = e(mat, unreal.MaterialExpressionConstant3Vector, -300, 0, constant=unreal.LinearColor(0.7, 0.66, 0.58, 1))
    MEL.connect_material_property(c, "", MP.MP_BASE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    log("eye / hidden / teeth materials built")


if not EAL.does_directory_exist(MAT):
    EAL.make_directory(MAT)
make_cloth()
make_skin()
make_eye_and_hidden()
for n in ("M_PK_PlayerCloth", "M_PK_PlayerSkin", "M_PK_PlayerEye", "M_PK_PlayerHidden", "M_PK_PlayerTeeth"):
    log("asset %s: %s" % (n, EAL.does_asset_exist(MAT + "/" + n)))
