"""
SM_PK_Ball: a smooth sphere for the match ball (the engine's basic sphere showed facets on the silhouette in close-ups).
Same size and pivot as /Engine/BasicShapes/Sphere (radius 50 cm, centred), so M_PK_Ball's local-position panels still fit.

  UnrealEditor PitchKings.uproject -ExecutePythonScript=Tools/editor_scripts/make_ball_mesh.py  (quits when done)
"""
import unreal

EAL = unreal.EditorAssetLibrary
PATH = "/Game/PitchKings/Stadium/SM_PK_Ball"


def pick(result, cls):
    if isinstance(result, cls):
        return result
    for x in (result if isinstance(result, tuple) else ()):
        if isinstance(x, cls):
            return x
    return None


try:
    dm = unreal.DynamicMesh()
    opts = unreal.GeometryScriptPrimitiveOptions()
    dm = pick(unreal.GeometryScript_Primitives.append_sphere_lat_long(dm, opts, unreal.Transform(), 50.0, 48, 64), unreal.DynamicMesh) or dm
    if EAL.does_asset_exist(PATH):
        EAL.delete_asset(PATH)
    new = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    new.set_editor_property("enable_nanite", False)
    new.set_editor_property("enable_collision", False)
    mesh = pick(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, PATH, new), unreal.StaticMesh)
    mesh.set_material(0, unreal.load_asset("/Game/PitchKings/Materials/M_PK_Ball"))
    EAL.save_asset(PATH)
    b = mesh.get_bounding_box()
    unreal.log("[BallMesh] tris %d box (%.1f %.1f %.1f)-(%.1f %.1f %.1f)" % (mesh.get_num_triangles(0), b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
except Exception as ex:
    import traceback
    unreal.log_error("[BallMesh] FAILED %s %s" % (ex, traceback.format_exc()))
unreal.SystemLibrary.quit_editor()
