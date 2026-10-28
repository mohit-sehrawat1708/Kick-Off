import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
OT = "editor_toolset.toolsets.object.ObjectTools."
L = "/Game/Maps/Stadium.Stadium:PersistentLevel."
def run():
    ppv = {"refPath": L + "PostProcessVolume_0"}
    r = T(OT + "set_properties", instance=ppv, values=json.dumps({"bUnbound": True, "Settings": {
        "bOverride_AutoExposureMinBrightness": True, "AutoExposureMinBrightness": 12.0,
        "bOverride_AutoExposureMaxBrightness": True, "AutoExposureMaxBrightness": 15.5,
        "bOverride_AutoExposureBias": True, "AutoExposureBias": 0.0,
        "bOverride_VignetteIntensity": True, "VignetteIntensity": 0.25,
        "bOverride_MotionBlurAmount": True, "MotionBlurAmount": 0.15,
        "bOverride_BloomIntensity": True, "BloomIntensity": 0.4}}))
    s = T(OT + "get_properties", instance=ppv, properties=["bUnbound", "Settings"])
    s = json.loads(s) if isinstance(s, str) else s
    st = s["Settings"]
    sun = T(OT + "get_properties", instance={"refPath": L + "DirectionalLight_0.LightComponent0"}, properties=["Intensity", "RelativeRotation"])
    return {"set": r, "bUnbound": s["bUnbound"], "min": st.get("AutoExposureMinBrightness"), "max": st.get("AutoExposureMaxBrightness"), "sun": sun}
