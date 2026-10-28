"""M_PK_BallShadow: a soft deferred decal for the ball's contact shadow (in the shade there is no sun shadow, and the
ball looked like it floated). PKBall projects it straight down under the ball and fades it with height.

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_ball_shadow.py
"""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
PATH = "/Game/PitchKings/Materials/M_PK_BallShadow"
if EAL.does_asset_exist(PATH):
    EAL.delete_asset(PATH)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_PK_BallShadow", "/Game/PitchKings/Materials", unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -800, 0)
st = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 150)
st.set_editor_property("parameter_name", "Strength")
st.set_editor_property("default_value", 0.7)
op = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 50)
op.set_editor_property("code", "float r = length(UV - 0.5) * 2.0; return Strength * pow(saturate(1.0 - r), 1.6);")
op.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
ins = []
for nm in ("UV", "Strength"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", nm)
    ins.append(ci)
op.set_editor_property("inputs", ins)
MEL.connect_material_expressions(uv, "", op, "UV")
MEL.connect_material_expressions(st, "", op, "Strength")
col = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, -100)
col.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 0.0, 1))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(mat)
EAL.save_asset(PATH)
unreal.log("[BallShadow] built")
