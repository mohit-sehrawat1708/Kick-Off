import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
def run():
    s = T("editor_toolset.toolsets.object.ObjectTools.get_properties", instance={"refPath": "/Game/Maps/Stadium.Stadium:PersistentLevel.PostProcessVolume_0"}, properties=["Settings"])
    s = json.loads(s)["Settings"]
    return {k: v for k, v in s.items() if "xposureM" in k or "ExposureBias" in k or "Vignette" in k}
