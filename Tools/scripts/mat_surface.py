import json
MT = "editor_toolset.toolsets.material.MaterialTools."
OT = "editor_toolset.toolsets.object.ObjectTools."
AT = "editor_toolset.toolsets.asset.AssetTools."
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
def get_or_create(folder, name):
    path = folder + "/" + name + "." + name
    if T(AT + "exists", path=path):
        return T(AT + "load_asset", asset_path=path)
    return T(MT + "create_material", folder_path=folder, asset_name=name)
def add(m, cls, x=0, y=0, **props):
    e = T(MT + "add_expression", material_or_function=m, expression_class="/Script/Engine.MaterialExpression" + cls, x=x, y=y)
    if props:
        ok = T(OT + "set_properties", instance=e, values=json.dumps(props))
        if not ok:
            raise RuntimeError("set_properties failed on %s with %s" % (cls, list(props.keys())))
    return e
def link(a, b, inp, out=""):
    T(MT + "connect_expressions", from_expression=a, from_output_name=out, to_expression=b, to_input_name=inp)
def to_out(e, prop, out=""):
    T(MT + "connect_to_output", expression=e, output_name=out, material_property=prop)
def clear(m):
    for e in T(MT + "get_expressions", material_or_function=m):
        T(MT + "delete_expression", material_or_function=m, expression=e)
def run():
    m = get_or_create("/Game/PitchKings/Materials", "M_PK_Surface")
    clear(m)
    col = add(m, "VectorParameter", -600, -200, parameterName="BaseColor", group="Surface", defaultValue={"r": 0.5, "g": 0.5, "b": 0.5, "a": 1})
    rough = add(m, "ScalarParameter", -600, 0, parameterName="Roughness", group="Surface", defaultValue=0.6)
    metal = add(m, "ScalarParameter", -600, 100, parameterName="Metallic", group="Surface", defaultValue=0.0)
    spec = add(m, "ScalarParameter", -600, 200, parameterName="Specular", group="Surface", defaultValue=0.5)
    emis = add(m, "VectorParameter", -800, 350, parameterName="Emissive", group="Emissive", defaultValue={"r": 0, "g": 0, "b": 0, "a": 1})
    emis_s = add(m, "ScalarParameter", -800, 500, parameterName="EmissiveStrength", group="Emissive", defaultValue=0.0)
    mul = add(m, "Multiply", -400, 400)
    link(emis, mul, "A", "RGB")
    link(emis_s, mul, "B")
    to_out(col, "MP_BaseColor", "RGB")
    to_out(rough, "MP_Roughness")
    to_out(metal, "MP_Metallic")
    to_out(spec, "MP_Specular")
    to_out(mul, "MP_EmissiveColor")
    T(MT + "recompile", material_or_function=m)
    T(AT + "save_assets", asset_paths=["/Game/PitchKings/Materials/M_PK_Surface"])
    return {"ok": True, "inputs_mul": T(MT + "get_expression_input_names", expression=mul)}
