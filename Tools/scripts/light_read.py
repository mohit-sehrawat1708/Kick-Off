import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
OT = "editor_toolset.toolsets.object.ObjectTools."
L = "/Game/Maps/Stadium.Stadium:PersistentLevel."
def run():
    out = {}
    out["sun"] = T(OT + "get_properties", instance={"refPath": L + "DirectionalLight_0.LightComponent0"}, properties=["Intensity", "LightSourceAngle", "Temperature", "bUseTemperature", "bAtmosphereSunLight", "Mobility", "RelativeRotation", "DynamicShadowDistanceMovableLight", "CascadeDistributionExponent"])
    out["sky"] = T(OT + "get_properties", instance={"refPath": L + "SkyLight_0.SkyLightComponent0"}, properties=["Intensity", "bRealTimeCapture", "Mobility"])
    out["fog"] = T(OT + "get_properties", instance={"refPath": L + "ExponentialHeightFog_0.HeightFogComponent0"}, properties=["FogDensity", "FogHeightFalloff", "StartDistance"])
    out["cloud"] = T(OT + "get_properties", instance={"refPath": L + "VolumetricCloud_0.VolumetricCloudComponent"}, properties=["Material", "LayerBottomAltitude", "LayerHeight"])
    return out
