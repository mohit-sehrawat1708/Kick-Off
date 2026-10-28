"""Skyline backdrop: imports Tools/art/T_PK_Skyline.png (make_skyline.py) and builds M_PK_Skyline, an unlit translucent
material for the far cylinder PKPitch places round the stadium. The band is mapped by elevation seen from the pitch,
so the city rises from just under the roof line: from the gameplay cameras only the towers and Table Mountain show
above the roof, a glimpse of the city rather than a modelled one.

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_skyline_material.py
"""
import json
import os

import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
DEST = "/Game/PitchKings/Stadium"
ART = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "art")
BAND = json.load(open(os.path.join(ART, "skyline.json")))

# --- texture: sRGB colour + alpha, no mips (it is never minified much), never streamed out
t = unreal.AssetImportTask()
t.filename = os.path.join(ART, "T_PK_Skyline.png")
t.destination_path = DEST
t.destination_name = "T_PK_Skyline"
t.replace_existing = True
t.automated = True
t.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
tex = unreal.load_asset(DEST + "/T_PK_Skyline")
tex.set_editor_property("srgb", True)
tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)  # DXT5: colour + alpha
tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tex.set_editor_property("never_stream", True)
tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
EAL.save_loaded_asset(tex)

CODE = r"""
// direction from the middle of the pitch (eye height); azimuth wraps round the band, elevation picks the row
float3 d = WP - float3(0.0, 0.0, 200.0);
float e = degrees(atan2(d.z, length(d.xy)));
float u = frac(atan2(d.y, d.x) / 6.2831853 + Yaw);
float p = (e - Base) / Stretch + Bottom;          // elevation in the photo
float v = (Top - p) / (Top - Bottom);
// only the band itself: the roof hides what is below it from the pitch (an opaque haze wall there leaked into the
// pitch lighting as pale streaks)
float inBand = step(0.0, v) * step(v, 1.0);
float4 s = Tex.SampleLevel(TexSampler, float2(u, saturate(v)), 0);
float3 c = lerp(s.rgb, HazeColor.rgb, saturate(Haze + 0.25 * v));
return float4(c * Brightness, s.a * inBand);
"""

path = DEST + "/M_PK_Skyline"
if EAL.does_asset_exist(path):
    EAL.delete_asset(path)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_PK_Skyline", DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
# masked (cheaper than translucency); fog is turned off on the component in C++, and it must NOT be an "is sky"
# material: the sky light then captured it and smeared grey rays across the pitch
# translucent so it can opt out of the height fog (at 600 m the fog washed the photo into grey ghosts); the haze is
# the material's own, tuned to sit behind the roof line
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("use_translucency_vertex_fog", False)  # "Apply Fogging" in the editor
mat.set_editor_property("compute_fog_per_pixel", False)


def node(cls, x, y, **props):
    n = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def scalar(name, val, y):
    return node(unreal.MaterialExpressionScalarParameter, -900, y, parameter_name=name, default_value=val)


wp = node(unreal.MaterialExpressionWorldPosition, -900, -200)
tx = node(unreal.MaterialExpressionTextureObjectParameter, -900, -100, parameter_name="Tex", texture=tex)
params = {
    "Yaw": scalar("Yaw", 0.48, 0),                 # turns the CBD behind the main stand
    "Base": scalar("Base", 12.0, 80),               # world elevation (deg) of the band's bottom edge
    "Stretch": scalar("Stretch", 1.35, 160),        # world degrees per photo degree
    "Top": scalar("Top", BAND["elev_top"], 240),
    "Bottom": scalar("Bottom", BAND["elev_bottom"], 320),
    "Brightness": scalar("Brightness", 6500.0, 400),  # nits (physically lit scene)
    "Haze": scalar("Haze", 0.2, 480),
}
haze_col = node(unreal.MaterialExpressionVectorParameter, -900, 560, parameter_name="HazeColor",
                default_value=unreal.LinearColor(0.52, 0.62, 0.75, 1))
cn = node(unreal.MaterialExpressionCustom, -450, 100, code=CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
          description="Skyline band")
names = ["WP", "Tex"] + list(params.keys()) + ["HazeColor"]
ins = []
for nm in names:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", nm)
    ins.append(ci)
cn.set_editor_property("inputs", ins)
MEL.connect_material_expressions(wp, "", cn, "WP")
MEL.connect_material_expressions(tx, "", cn, "Tex")
for nm, n in params.items():
    MEL.connect_material_expressions(n, "", cn, nm)
MEL.connect_material_expressions(haze_col, "", cn, "HazeColor")
rgb = node(unreal.MaterialExpressionComponentMask, -200, 50, r=True, g=True, b=True, a=False)
alpha = node(unreal.MaterialExpressionComponentMask, -200, 200, r=False, g=False, b=False, a=True)
MEL.connect_material_expressions(cn, "", rgb, "")
MEL.connect_material_expressions(cn, "", alpha, "")
MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(mat)
EAL.save_asset(path)
cyl = unreal.load_asset("/Engine/BasicShapes/Cylinder")
b = cyl.get_bounding_box()
unreal.log("[Skyline] built; texture %dx%d; engine cylinder bounds z %.1f..%.1f, slots %d" % (
    tex.blueprint_get_size_x(), tex.blueprint_get_size_y(), b.min.z, b.max.z, len(cyl.static_materials)))
