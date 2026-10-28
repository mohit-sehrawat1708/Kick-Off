import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
ST = "editor_toolset.toolsets.scene.SceneTools."
def run():
    existing = T(ST + "find_actors", name="", tag="", collision_channels=[], actor_type={"refPath": "/Script/PitchKings.PKPitch"})
    if existing:
        return {"already": existing}
    a = T(ST + "add_to_scene_from_class", actor_type={"refPath": "/Script/PitchKings.PKPitch"}, name="Stadium", xform={"translation": {"x": 0, "y": 0, "z": 0}, "rotation": {"x": 0, "y": 0, "z": 0, "w": 1}, "scale3D": {"x": 1, "y": 1, "z": 1}}, parent=None, snap_to_ground=False)
    T("editor_toolset.toolsets.asset.AssetTools.save_assets", asset_paths=["/Game/Maps/Stadium"])
    return {"placed": a}
