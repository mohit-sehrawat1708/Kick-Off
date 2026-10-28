import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
OT = "editor_toolset.toolsets.object.ObjectTools."
L = "/Game/Maps/Stadium.Stadium:PersistentLevel."
def setp(path, vals):
    if not T(OT + "set_properties", instance={"refPath": L + path}, values=json.dumps(vals)):
        raise RuntimeError("failed " + path)
def run():
    setp("DirectionalLight_0.LightComponent0", {"Intensity": 80000.0, "DynamicShadowDistanceMovableLight": 16000.0, "LightSourceAngle": 0.53})
    setp("DirectionalLight_0.LightComponent0", {"RelativeRotation": {"Pitch": -40.0, "Yaw": 35.0, "Roll": 0.0}})
    setp("ExponentialHeightFog_0.HeightFogComponent0", {"FogDensity": 0.004, "StartDistance": 4000.0})
    return {"sun": T(OT + "get_properties", instance={"refPath": L + "DirectionalLight_0.LightComponent0"}, properties=["Intensity", "RelativeRotation"])}
