import json
def T(_tool, **kw):
    return execute_tool(_tool, json.dumps(kw))["returnValue"]
def run():
    acts = T("editor_toolset.toolsets.scene.SceneTools.find_actors", name="", tag="", collision_channels=[])
    out = []
    for a in acts:
        cls = T("editor_toolset.toolsets.object.ObjectTools.get_class", instance=a)
        comps = T("editor_toolset.toolsets.actor.ActorTools.get_components", actor=a, component_type={"refPath": "/Script/Engine.ActorComponent"})
        out.append({"actor": a["refPath"].split(".")[-1], "class": str(cls)[-60:], "comps": [c["refPath"].split(".")[-1] for c in comps]})
    return {"level": T("editor_toolset.toolsets.scene.SceneTools.get_current_level"), "actors": out}
