import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
MT = "editor_toolset.toolsets.material.MaterialTools."
OT = "editor_toolset.toolsets.object.ObjectTools."
def props(e, names):
    r = T(OT + "get_properties", instance=e, properties=names)
    return json.loads(r) if isinstance(r, str) else r
def setp(e, vals):
    if not T(OT + "set_properties", instance=e, values=json.dumps(vals)):
        raise RuntimeError("set failed " + e["refPath"])
def tweak(path, rules):
    changed = []
    for e in T(MT + "get_expressions", material_or_function=path):
        name = e["refPath"].split(":")[-1]
        for prefix, match, vals in rules:
            if not name.startswith(prefix):
                continue
            cur = props(e, list(match.keys()))
            if all(abs(float(cur.get(k, -999)) - v) < 1e-4 for k, v in match.items()):
                setp(e, vals)
                changed.append(name)
    T(MT + "recompile", material_or_function=path)
    return changed
def run():
    g = tweak("/Game/PitchKings/Materials/M_PK_Grass.M_PK_Grass", [
        ("MaterialExpressionLinearInterpolate", {"ConstA": 0.8, "ConstB": 1.12}, {"ConstA": 0.92, "ConstB": 1.06}),
        ("MaterialExpressionLinearInterpolate", {"ConstA": 0.86, "ConstB": 1.08}, {"ConstA": 0.94, "ConstB": 1.04}),
        ("MaterialExpressionNoise", {"Scale": 0.04}, {"Scale": 0.15}),
    ])
    c = tweak("/Game/PitchKings/Materials/M_PK_Crowd.M_PK_Crowd", [
        ("MaterialExpressionNoise", {"Scale": 0.035}, {"Scale": 0.06}),
        ("MaterialExpressionNoise", {"Scale": 0.11}, {"Scale": 0.22}),
        ("MaterialExpressionScalarParameter", {"DefaultValue": 0.35}, {"DefaultValue": 0.18}),
    ])
    T("editor_toolset.toolsets.asset.AssetTools.save_assets", asset_paths=["/Game/PitchKings/Materials/M_PK_Grass", "/Game/PitchKings/Materials/M_PK_Crowd"])
    return {"grass": g, "crowd": c}
