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
def color(r, g, b):
    return {"r": r, "g": g, "b": b, "a": 1}

def build_grass():
    m = get_or_create("/Game/PitchKings/Materials", "M_PK_Grass")
    clear(m)
    wp = add(m, "WorldPosition", -1600, 0)
    mask = add(m, "ComponentMask", -1400, 0, r=True, g=False, b=False, a=False)
    link(wp, mask, "None", "XYZ")
    width = add(m, "ScalarParameter", -1400, 120, parameterName="StripeWidth", group="Grass", defaultValue=525.0)
    div = add(m, "Divide", -1200, 0)
    link(mask, div, "A"); link(width, div, "B")
    fl = add(m, "Floor", -1050, 0); link(div, fl, "None")
    half = add(m, "Multiply", -900, 0, constB=0.5); link(fl, half, "A")
    fr = add(m, "Frac", -750, 0); link(half, fr, "None")
    two = add(m, "Multiply", -600, 0, constB=2.0); link(fr, two, "A")
    dark = add(m, "VectorParameter", -600, -250, parameterName="GrassDark", group="Grass", defaultValue=color(0.055, 0.19, 0.03))
    light = add(m, "VectorParameter", -600, -120, parameterName="GrassLight", group="Grass", defaultValue=color(0.085, 0.26, 0.045))
    stripe = add(m, "LinearInterpolate", -400, -150)
    link(dark, stripe, "A", "RGB"); link(light, stripe, "B", "RGB"); link(two, stripe, "Alpha")
    n1 = add(m, "Noise", -600, 200, scale=0.0012, levels=4, outputMin=0.0, outputMax=1.0, quality=1)
    v1 = add(m, "LinearInterpolate", -400, 200, constA=0.92, constB=1.06); link(n1, v1, "Alpha")
    n2 = add(m, "Noise", -600, 400, scale=0.15, levels=2, outputMin=0.0, outputMax=1.0, quality=1)
    v2 = add(m, "LinearInterpolate", -400, 400, constA=0.94, constB=1.04); link(n2, v2, "Alpha")
    c1 = add(m, "Multiply", -200, -100); link(stripe, c1, "A"); link(v1, c1, "B")
    c2 = add(m, "Multiply", -50, -100); link(c1, c2, "A"); link(v2, c2, "B")
    rough = add(m, "ScalarParameter", -200, 250, parameterName="Roughness", group="Grass", defaultValue=0.88)
    spec = add(m, "ScalarParameter", -200, 350, parameterName="Specular", group="Grass", defaultValue=0.3)
    to_out(c2, "MP_BaseColor"); to_out(rough, "MP_Roughness"); to_out(spec, "MP_Specular")
    T(MT + "recompile", material_or_function=m)

def build_crowd():
    m = get_or_create("/Game/PitchKings/Materials", "M_PK_Crowd")
    clear(m)
    n1 = add(m, "Noise", -800, 0, scale=0.06, levels=1, outputMin=0.0, outputMax=1.0, quality=1)
    n2 = add(m, "Noise", -800, 200, scale=0.22, levels=1, outputMin=0.0, outputMax=1.0, quality=1)
    a = add(m, "VectorParameter", -800, -300, parameterName="CrowdA", group="Crowd", defaultValue=color(0.5, 0.48, 0.46))
    b = add(m, "VectorParameter", -800, -170, parameterName="CrowdB", group="Crowd", defaultValue=color(0.08, 0.08, 0.1))
    tint = add(m, "VectorParameter", -800, 400, parameterName="TeamTint", group="Crowd", defaultValue=color(0.5, 0.05, 0.05))
    mix = add(m, "LinearInterpolate", -500, -200); link(a, mix, "A", "RGB"); link(b, mix, "B", "RGB"); link(n1, mix, "Alpha")
    tinted = add(m, "LinearInterpolate", -300, -100, constAlpha=0.0)
    link(mix, tinted, "A"); link(tint, tinted, "B", "RGB")
    amt = add(m, "ScalarParameter", -500, 300, parameterName="TintAmount", group="Crowd", defaultValue=0.18)
    link(amt, tinted, "Alpha")
    shade = add(m, "LinearInterpolate", -500, 150, constA=0.25, constB=1.15); link(n2, shade, "Alpha")
    c = add(m, "Multiply", -100, 0); link(tinted, c, "A"); link(shade, c, "B")
    rough = add(m, "Constant", -100, 200, r=0.92)
    to_out(c, "MP_BaseColor"); to_out(rough, "MP_Roughness")
    T(MT + "recompile", material_or_function=m)

def run():
    build_grass()
    build_crowd()
    T(AT + "save_assets", asset_paths=["/Game/PitchKings/Materials/M_PK_Grass", "/Game/PitchKings/Materials/M_PK_Crowd"])
    return {"ok": True}
