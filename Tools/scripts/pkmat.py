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
