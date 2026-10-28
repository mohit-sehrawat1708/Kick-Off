import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
OT = "editor_toolset.toolsets.object.ObjectTools."
ST = "editor_toolset.toolsets.scene.SceneTools."
L = "/Game/Maps/Stadium.Stadium:PersistentLevel."
def run():
    out = {}
    for name in ["Floor_0", "StaticMeshActor_0", "PlayerStart_0"]:
        try:
            T(ST + "remove_from_scene", actor={"refPath": L + name})
        except Exception as e:
            out[name] = str(e)[:120]
    T(OT + "set_properties", instance={"refPath": L + "DirectionalLight_0.LightComponent0"}, values=json.dumps({"Intensity": 80000.0, "RelativeRotation": {"pitch": -40.0, "yaw": 35.0, "roll": 0.0}, "DynamicShadowDistanceMovableLight": 16000.0, "LightSourceAngle": 0.53}))
    T(OT + "set_properties", instance={"refPath": L + "ExponentialHeightFog_0.HeightFogComponent0"}, values=json.dumps({"FogDensity": 0.004, "StartDistance": 4000.0}))
    ppv = T(ST + "add_to_scene_from_class", actor_type={"refPath": "/Script/Engine.PostProcessVolume"}, name="PP_Stadium", xform={"translation": {"x": 0, "y": 0, "z": 0}, "rotation": {"x": 0, "y": 0, "z": 0, "w": 1}, "scale3D": {"x": 1, "y": 1, "z": 1}}, parent=None, snap_to_ground=False)
    out["ppv"] = ppv
    props = T(OT + "list_properties", instance=ppv)
    props = json.loads(props) if isinstance(props, str) else props
    out["ppv_props"] = list(props.keys())
    out["settings_keys"] = [k for k in json.dumps(props.get("settings", {}))[:20000].split('"') if "Exposure" in k][:40]
    return out
