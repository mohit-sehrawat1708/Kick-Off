"""Match ball material M_PK_Ball (procedural: white casing, six swirled accent panels in two colours, seams), driven by
the ball's local position so the pattern rolls with the ball. Run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_ball_material.py
"""
import unreal

MAT = "/Game/PitchKings/Materials"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
MP = unreal.MaterialProperty

LIB = r"""
float3 p = normalize(LP);
float3 a = abs(p);
float2 uv; float face;
if (a.x >= a.y && a.x >= a.z) { uv = p.yz / a.x; face = p.x > 0 ? 0 : 1; }
else if (a.y >= a.z)          { uv = p.xz / a.y; face = p.y > 0 ? 2 : 3; }
else                          { uv = p.xy / a.z; face = p.z > 0 ? 4 : 5; }
// panel seams along the cube-face edges: a narrow stitched groove, anti-aliased by its screen footprint
float edge = 1.0 - max(abs(uv.x), abs(uv.y));
float px = max(fwidth(edge), 0.002);
float seam = 1.0 - smoothstep(0.012, 0.012 + px * 1.5, edge);
// printed swirl graphic: crisp edges (screen-space widths) and a thin dark key line around it
float r = length(uv);
float th = atan2(uv.y, uv.x) + r * 2.4 + face * 0.7;
float f = frac(th / 3.14159265);
float fpx = max((length(fwidth(uv)) / max(r, 0.05) + 2.4 * fwidth(r)) / 3.14159265, 0.002);  // not fwidth(atan2): it spikes at the branch cut
float band = smoothstep(0.0, fpx, f) * (1.0 - smoothstep(0.5 - fpx, 0.5, f));
float rpx = max(fwidth(r), 0.002);
float ring = smoothstep(0.2, 0.2 + rpx, r) * (1.0 - smoothstep(0.82 - rpx, 0.82, r));
float blade = band * ring;
float bandO = smoothstep(-0.02, -0.02 + fpx, f - 0.0) * (1.0 - smoothstep(0.52 - fpx, 0.52, f));
float ringO = smoothstep(0.185, 0.185 + rpx, r) * (1.0 - smoothstep(0.835 - rpx, 0.835, r));
float keyline = saturate(max(bandO, step(0.98, f)) * ringO - blade);
float useB = fmod(face, 2.0);
"""

COLOR = LIB + r"""
float3 white = float3(0.86, 0.86, 0.84);
float3 accA = float3(0.02, 0.05, 0.22);
float3 accB = float3(0.95, 0.62, 0.05);
float3 c = lerp(white, lerp(accA, accB, useB), blade);
c = lerp(c, float3(0.03, 0.03, 0.05), keyline * 0.85);
c *= 1.0 - seam * 0.6;
return c;
"""

ROUGH = LIB + r"""
return lerp(lerp(0.34, 0.42, blade), 0.75, seam);
"""

# seam grooves and a fine pebbled casing (PU texture), as a tangent-space normal from the local position
NORMAL = r"""
float3 p = normalize(LP);
float3 a = abs(p);
float2 uv;
if (a.x >= a.y && a.x >= a.z) uv = p.yz / a.x; else if (a.y >= a.z) uv = p.xz / a.y; else uv = p.xy / a.z;
float edge = 1.0 - max(abs(uv.x), abs(uv.y));
float groove = saturate(1.0 - edge / 0.03);
float2 g = (abs(uv.x) > abs(uv.y) ? float2(sign(uv.x), 0) : float2(0, sign(uv.y))) * groove * 0.6;
float3 q = p * 260.0;
float peb = sin(q.x) * sin(q.y * 1.1) * sin(q.z * 0.9);
g += float2(cos(q.x) * 0.08, cos(q.y * 1.1) * 0.08) * peb;
return normalize(float3(g, 1.0));
"""


def build():
    full = MAT + "/M_PK_Ball"
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = AT.create_asset("M_PK_Ball", MAT, unreal.Material, unreal.MaterialFactoryNew())
    if hasattr(unreal, "MaterialExpressionLocalPosition"):
        lp = MEL.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -900, 0)
    else:  # older engines: world position transformed to local space
        wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 0)
        lp = MEL.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -900, 0)
        lp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        lp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        MEL.connect_material_expressions(wp, "", lp, "")
    for code, out, prop, y, desc in ((COLOR, F3, MP.MP_BASE_COLOR, 0, "Ball colour"), (ROUGH, F1, MP.MP_ROUGHNESS, 250, "Ball roughness"),
                                     (NORMAL, F3, MP.MP_NORMAL, 500, "Ball seams + casing")):
        node = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, y)
        node.set_editor_property("code", code)
        node.set_editor_property("output_type", out)
        node.set_editor_property("description", desc)
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", "LP")
        node.set_editor_property("inputs", [ci])
        MEL.connect_material_expressions(lp, "", node, "LP")
        MEL.connect_material_property(node, "", prop)
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    unreal.log("[BallMat] built " + full)


build()
