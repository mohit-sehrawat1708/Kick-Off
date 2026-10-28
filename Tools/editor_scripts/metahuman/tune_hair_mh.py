"""
Retunes M_PK_HairCardsMH in place (its instances keep their atlases), and the per-groom coverage of the facial hair.
- Opacity: edge-on cards and faint coverage drop out instead of dithering into a grey, dotted halo round the hair.
- Colour: tips no lighter than the roots (bright sparse tips read as a grey fringe against the stands).
- Beards / moustaches: denser coverage, so short facial hair reads as a solid shape, not a pixel cloud on the jaw.

Run in the full editor (see build_player_heads.py); the same code is in build_player_heads.py for fresh builds.
"""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
PATH = "/Game/PitchKings/Characters/Heads/M_PK_HairCardsMH"
HEADS = "/Game/PitchKings/Characters/Heads"
OPACITY = ("float facing = saturate(abs(dot(normalize(Nrm), normalize(Cam))) * 2.5 + 0.05);"
           " float tips = 1.0 - 0.7 * smoothstep(0.55, 1.0, Tan.a);"
           " float a = Att.r * facing * tips * Cov;"
           " return smoothstep(0.2, 0.65, a);")
COLOR = r"""
float t = saturate(Tan.a);
float seed = frac(Att.g * 7.31 + Att.b * 3.17);
float3 c = HairColor.rgb * lerp(0.8, 1.0, t) * lerp(0.88, 1.12, seed);
return c;
"""
FACIAL_COVERAGE = 1.3

mat = unreal.load_asset(PATH)
done = []
for prop, code, key in ((unreal.MaterialProperty.MP_OPACITY_MASK, OPACITY, "facing"), (unreal.MaterialProperty.MP_BASE_COLOR, COLOR, "HairColor")):
    node = MEL.get_material_property_input_node(mat, prop)
    if isinstance(node, unreal.MaterialExpressionCustom) and key in node.get_editor_property("code"):
        node.set_editor_property("code", code)
        done.append(str(prop))
# duller sheen: the sky's specular lit the sparse tips into a grey fringe
for prop, val in ((unreal.MaterialProperty.MP_SPECULAR, 0.15), (unreal.MaterialProperty.MP_ROUGHNESS, 0.7)):
    node = MEL.get_material_property_input_node(mat, prop)
    if isinstance(node, unreal.MaterialExpressionConstant):
        node.set_editor_property("r", val)
        done.append(str(prop))
MEL.recompile_material(mat)
EAL.save_asset(PATH)
mis = 0
for a in EAL.list_assets(HEADS, recursive=False):
    name = a.split(".")[0].split("/")[-1]
    if name.startswith("MI_PK_") and (name.endswith("_Beard") or name.endswith("_Mustache")):
        mi = unreal.load_asset(a)
        MEL.set_material_instance_scalar_parameter_value(mi, "Coverage", FACIAL_COVERAGE)
        MEL.update_material_instance(mi)
        EAL.save_asset(a.split(".")[0])
        mis += 1
unreal.log("[HairMH] updated %s, facial-hair instances %d" % (done, mis))
