"""Builds SKM_PK_Body: the UE5 mannequin with its armour-plate grooves, joint rings and moulded muscles smoothed away
(Taubin smoothing: no shrinkage), so the kit reads as cloth and bare arms and legs read as skin. Hands, feet and the
face keep their shape. Same skeleton and skin weights, so every animation still applies. Run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_player_body.py
"""
import math
import unreal

SRC = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
DST_DIR = "/Game/PitchKings/Characters"
DST = DST_DIR + "/SKM_PK_Body"
GS = unreal
EAL = unreal.EditorAssetLibrary

src = unreal.load_asset(SRC)
if not EAL.does_asset_exist(DST):
    EAL.duplicate_asset(SRC, DST)
dst = unreal.load_asset(DST)

mesh = unreal.DynamicMesh()
opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
lod = unreal.GeometryScriptMeshReadLOD()
mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(src, mesh, opts, lod)
unreal.log("[Body] read: %s, %d vertices, %d triangles" % (outcome, mesh.get_vertex_count(), mesh.get_triangle_count()))
# The mannequin is split along its UV / part seams; smoothing the copies apart opened cracks at the elbows and wrists.
# Weld coincident boundary edges first so every seam moves as one surface.
def boundary_edges(m):
    try:
        r = unreal.GeometryScript_MeshQueries.get_num_open_border_edges(m)
        return [x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, int)][0]
    except Exception:
        return -1


unreal.log("[Body] open border edges before: %d" % boundary_edges(mesh))
tj = unreal.GeometryScriptResolveTJunctionOptions()
tj.set_editor_property("tolerance", 0.05)
unreal.GeometryScript_MeshRepair.resolve_mesh_t_junctions(mesh, tj)
wo = unreal.GeometryScriptWeldEdgesOptions()
wo.set_editor_property("tolerance", 0.3)
wo.set_editor_property("only_unique_pairs", False)
unreal.GeometryScript_MeshRepair.weld_mesh_edges(mesh, wo)
unreal.log("[Body] open border edges after: %d" % boundary_edges(mesh))
unreal.log("[Body] welded: %d vertices" % mesh.get_vertex_count())

def first(r, kind):
    """Picks the result of type `kind` out of a Geometry Script call (they also return the mesh and flags)."""
    items = r if isinstance(r, tuple) else (r,)
    for x in items:
        if type(x).__name__ == kind:
            return x
    raise RuntimeError("no %s in %s" % (kind, [type(x).__name__ for x in items]))


pos_obj = first(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False), "GeometryScriptVectorList")
unreal.log("[Body] positions object: %s" % type(pos_obj).__name__)
pos_arr = first(unreal.GeometryScript_List.convert_vector_list_to_array(pos_obj, []), "Array") if False else unreal.GeometryScript_List.convert_vector_list_to_array(pos_obj)
if isinstance(pos_arr, tuple):
    pos_arr = [x for x in pos_arr if not isinstance(x, unreal.GeometryScriptVectorList)][-1]
positions = [(v.x, v.y, v.z) for v in pos_arr]
tri_obj = first(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False), "GeometryScriptTriangleList")
tri_arr = unreal.GeometryScript_List.convert_triangle_list_to_array(tri_obj)
if isinstance(tri_arr, tuple):
    tri_arr = [x for x in tri_arr if not isinstance(x, unreal.GeometryScriptTriangleList)][-1]
tris = [(t.x, t.y, t.z) for t in tri_arr]
n = len(positions)
unreal.log("[Body] %d positions, %d triangles" % (n, len(tris)))

nbr = [set() for _ in range(n)]
for a, b, c in tris:
    if a < 0:
        continue
    nbr[a].update((b, c))
    nbr[b].update((a, c))
    nbr[c].update((a, b))
nbr = [list(s) for s in nbr]


def weight(p):
    """How strongly a vertex is smoothed (mesh space: +X left, +Y forward, Z up, 180 cm)."""
    x, y, z = abs(p[0]), p[1], p[2]
    w = 1.0
    w *= min(1.0, max(0.0, (42.0 - x) / 5.0))          # hands keep their fingers
    w *= min(1.0, max(0.0, (z - 11.0) / 5.0))          # feet / boots keep their shape
    if z > 158.0:                                       # head: the face is left alone, the skull rounded a little
        w *= 1.0 if y < 3.0 else 0.25  # skull creases smoothed fully, the face lightly
    return w


W = [weight(p) for p in positions]
P = [list(p) for p in positions]


def step(factor):
    out = []
    for i in range(n):
        ns = nbr[i]
        if not ns or W[i] <= 0.0:
            out.append(P[i])
            continue
        sx = sy = sz = 0.0
        for j in ns:
            q = P[j]
            sx += q[0]; sy += q[1]; sz += q[2]
        k = 1.0 / len(ns)
        f = factor * W[i]
        p = P[i]
        out.append([p[0] + f * (sx * k - p[0]), p[1] + f * (sy * k - p[1]), p[2] + f * (sz * k - p[2])])
    return out


for it in range(28):  # Taubin lambda / mu: removes grooves and ridges without shrinking the limbs
    P = step(0.55)
    P = step(-0.58)

# Second pass, plain Laplacian on the body form: flattens the moulded pecs, abs and the chest disc so the shirt
# hangs like cloth (a slightly slimmer, athletic build). Strongest on the torso, fading out down the limbs.
def form_weight(p):
    x, y, z = abs(p[0]), p[1], p[2]
    torso = min(1.0, max(0.0, (z - 90.0) / 8.0)) * min(1.0, max(0.0, (152.0 - z) / 6.0)) * min(1.0, max(0.0, (24.0 - x) / 6.0))
    limbs = 0.35 * min(1.0, max(0.0, (z - 14.0) / 8.0)) * min(1.0, max(0.0, (40.0 - x) / 6.0)) * (1.0 if z < 152.0 else 0.0)
    return max(torso, limbs)


W = [form_weight(p) for p in positions]
for it in range(45):
    P = step(0.5)

moved = max(math.dist(P[i], positions[i]) for i in range(n))
unreal.log("[Body] smoothed, max move %.2f cm" % moved)

vl = unreal.GeometryScript_List.convert_array_to_vector_list([unreal.Vector(p[0], p[1], p[2]) for p in P])
if isinstance(vl, tuple):
    vl = [x for x in vl if isinstance(x, unreal.GeometryScriptVectorList)][0]
unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh, vl)
# one shared normal per vertex: the mannequin's normals were split along its UV seams, which drew thin seam lines
# down the forearms and around the wrists once the plates were smoothed away
unreal.GeometryScript_Normals.set_per_vertex_normals(mesh)
unreal.GeometryScript_Normals.recompute_normals(mesh, unreal.GeometryScriptCalculateNormalsOptions())

# one render section: the mannequin's torso / head-legs split ran round the elbows and wrists, and the duplicated
# vertices on that boundary drifted apart under skinning (dashed cracks). The kit material covers every slot anyway.
unreal.GeometryScript_Materials.remap_material_i_ds(mesh, 1, 0)
wopts = unreal.GeometryScriptCopyMeshToAssetOptions()
wopts.set_editor_property("enable_recompute_normals", False)
wopts.set_editor_property("enable_recompute_tangents", True)
res = unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(mesh, dst, wopts, unreal.GeometryScriptMeshWriteLOD())
unreal.log("[Body] write: %s" % (res,))
EAL.save_asset(DST)
unreal.log("[Body] saved " + DST)
