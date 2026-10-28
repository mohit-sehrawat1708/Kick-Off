"""
Player boots from Tools/art/boots/vrfs_boots.glb ("Vrfs boots" by zanman730, CC-BY 4.0, see CREDITS.txt there).

Imports the GLB, takes one boot, finds its length / up / toe directions from its shape (the sock collar is the tall
end, the toe the low one), scales and places it over the mannequin's left foot in the reference pose (fitted to the
foot's own vertex bounds), then bakes it into foot_l bone space -> SM_PK_Boot_L. The right boot is the mirror image
baked into foot_r space -> SM_PK_Boot_R. PKFootballer attaches them to the foot sockets with no offset and hides the
mannequin foot under them. Material: M_PK_Boot (upper colour per player, dark sole and studs, trim flash).

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/build_player_boots.py
"""
import os

import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
GA = unreal.GeometryScript_AssetUtils
GN = unreal.GeometryScript_NewAssetUtils
GT = unreal.GeometryScript_MeshTransforms
GQ = unreal.GeometryScript_MeshQueries
GL = unreal.GeometryScript_List
OUT = "/Game/PitchKings/Characters/Boots"
TMP = OUT + "/Import"
BODY = "/Game/PitchKings/Characters/SKM_PK_Body"
GLB = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "art", "boots", "vrfs_boots.glb")


def log(s):
    unreal.log("[Boots] " + s)


def pick(result, cls):
    if isinstance(result, cls):
        return result
    for x in (result if isinstance(result, tuple) else ()):
        if isinstance(x, cls):
            return x
    return None


def positions(dm):
    vl = pick(GQ.get_all_vertex_positions(dm, False), unreal.GeometryScriptVectorList)
    arr = GL.convert_vector_list_to_array(vl)
    if isinstance(arr, tuple):
        arr = [x for x in arr if isinstance(x, list)][0]
    return [(v.x, v.y, v.z) for v in arr]


def bounds(pts):
    return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]


# ---- import --------------------------------------------------------------------------------------------------------
if EAL.does_directory_exist(TMP):
    EAL.delete_directory(TMP)
task = unreal.AssetImportTask()
task.filename = GLB
task.destination_path = TMP
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
meshes = [unreal.load_asset(p) for p in EAL.list_assets(TMP, recursive=True)]
meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
log("imported static meshes: %s" % [m.get_name() for m in meshes])
src = meshes[0]

dm = unreal.DynamicMesh()
read = unreal.GeometryScriptMeshReadLOD()
read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
dm = pick(GA.copy_mesh_from_static_mesh(src, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read), unreal.DynamicMesh) or dm
pts = positions(dm)
lo, hi = bounds(pts)
ext = [hi[i] - lo[i] for i in range(3)]
log("source boot %d verts, extents %s" % (len(pts), ["%.2f" % e for e in ext]))

# ---- orient by principal axes (the boot is imported at an angle): length = largest spread, up = second, width = least
n = float(len(pts))
c0 = [sum(p[k] for p in pts) / n for k in range(3)]
cov = [[sum((p[a] - c0[a]) * (p[b] - c0[b]) for p in pts) / n for b in range(3)] for a in range(3)]


def matvec(m, v):
    return [sum(m[r][k] * v[k] for k in range(3)) for r in range(3)]


def norm(v):
    l = sum(x * x for x in v) ** 0.5
    return [x / l for x in v]


axes = []
m = [row[:] for row in cov]
for _ in range(3):
    v = norm([1.0, 0.7, 0.3])
    for _ in range(200):
        v = norm(matvec(m, v))
    lam = sum(a * b for a, b in zip(v, matvec(m, v)))
    axes.append(v)
    m = [[m[r][k] - lam * v[r] * v[k] for k in range(3)] for r in range(3)]
A_L, A_U, A_W = axes
proj = lambda p, a: sum((p[k] - c0[k]) * a[k] for k in range(3))
lv = [proj(p, A_L) for p in pts]
uv = [proj(p, A_U) for p in pts]
mid_l = (min(lv) + max(lv)) * 0.5
def span(sel):
    vs = [u for l, u in zip(lv, uv) if sel(l)]
    return max(vs) - min(vs), sum(vs) / len(vs)
span_a, mean_a = span(lambda l: l < mid_l)
span_b, mean_b = span(lambda l: l >= mid_l)
toe_sign = 1.0 if span_a > span_b else -1.0          # toe points toward the low end
toe_mean = mean_b if toe_sign > 0 else mean_a
up_sign = 1.0 if toe_mean < (min(uv) + max(uv)) * 0.5 else -1.0
log("principal axes L %s U %s W %s, toe %+d up %+d" % (["%.2f" % x for x in A_L], ["%.2f" % x for x in A_U], ["%.2f" % x for x in A_W], toe_sign, up_sign))


def to_boot(p):
    # boot frame: x = width, y = toe direction, z = up
    return (proj(p, A_W), proj(p, A_L) * toe_sign, proj(p, A_U) * up_sign)


bp = [to_boot(p) for p in pts]
blo, bhi = bounds(bp)

# ---- the mannequin's left foot (reference pose, component space) ---------------------------------------------------
body = unreal.load_asset(BODY)
bdm = unreal.DynamicMesh()
bdm = pick(GA.copy_mesh_from_skeletal_mesh(body, bdm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()), unreal.DynamicMesh) or bdm
body_pts = positions(bdm)
foot = [p for p in body_pts if p[0] > 4.0 and p[2] < 11.0]
# the left leg from the ankle up to mid-calf, for fitting the sock collar
leg = [p for p in body_pts if p[0] > 4.0 and 6.0 < p[2] < 26.0]
ank = [p for p in leg if 12.0 < p[2] < 16.0]
acx, acy = sum(p[0] for p in ank) / len(ank), sum(p[1] for p in ank) / len(ank)
flo, fhi = bounds(foot)
log("mannequin left foot bounds %s .. %s" % (["%.1f" % v for v in flo], ["%.1f" % v for v in fhi]))
# fit: boot length = foot length * 1.06 (the upper sits over the foot), width to the foot + 1 cm
s_len = (fhi[1] - flo[1]) * 1.06 / (bhi[1] - blo[1])
s_wid = (fhi[0] - flo[0] + 1.0) / (bhi[0] - blo[0])
# per-axis fit: an averaged uniform scale made the boot 17 % longer than the foot, so its heel collar stood off
# behind the ankle; the heel is aligned to the foot's heel rather than centred
s_len *= 1.02 / 1.06
s = (s_len + s_wid) * 0.5
cx, cy = (flo[0] + fhi[0]) * 0.5, (flo[1] + fhi[1]) * 0.5
bcx, bcy = (blo[0] + bhi[0]) * 0.5, (blo[1] + bhi[1]) * 0.5
log("scale length %.3f, width %.3f, height %.3f" % (s_len, s_wid, s))

ref = unreal.AnimPoseExtensions.get_reference_pose(body.get_editor_property("skeleton"))
W_SPACE = unreal.AnimPoseSpaces.WORLD


def fit_collar(comp, mirror):
    """Pulls (or pushes) the sock collar onto the leg: in each 1 cm slice and 15-degree sector round the ankle, the
    boot's outermost radius becomes the leg's radius + 4 mm, blended in above the shoe body (a wide collar left the leg
    standing in a bucket; a tight one let the sock poke through)."""
    import math
    sgn = -1.0 if mirror else 1.0
    def key(x, y, z):
        a = math.atan2(y - acy, x * sgn - acx)
        return int(round(z)), int((a + math.pi) / (math.pi / 12.0)) % 24
    leg_r = {}
    for p in leg:
        k = key(p[0] * sgn, p[1], p[2])
        r = math.hypot(p[0] - acx, p[1] - acy)
        if r < 12.0:
            leg_r[k] = max(leg_r.get(k, 0.0), r)
    boot_r = {}
    for v in comp:
        if v.z > 6.0:
            k = key(v.x, v.y, v.z)
            boot_r[k] = max(boot_r.get(k, 0.0), math.hypot(v.x * sgn - acx, v.y - acy))
    out, n = [], 0
    for v in comp:
        w = min(max((v.z - 7.0) / 3.0, 0.0), 1.0)
        k = key(v.x, v.y, v.z)
        if w <= 0.0 or k not in boot_r or k not in leg_r or boot_r[k] < 0.5:
            out.append(v)
            continue
        scale = (leg_r[k] + 0.4) / boot_r[k]
        f = 1.0 + (scale - 1.0) * w
        x = acx + (v.x * sgn - acx) * f
        y = acy + (v.y - acy) * f
        out.append(unreal.Vector(x * sgn, y, v.z))
        n += 1
    log("collar fit: %d vertices moved" % n)
    return out


def baked(mirror):
    side = "r" if mirror else "l"
    bone = unreal.AnimPoseExtensions.get_bone_pose(ref, "foot_" + side, W_SPACE)
    comp = []
    for p in bp:
        x = (p[0] - bcx) * s_wid * 1.1 + cx            # roomy: the mannequin ankle stays inside
        y = (p[1] - blo[1]) * s_len * 1.1 + flo[1] - 2.5   # heel counter wraps the mannequin heel
        z = (p[2] - blo[2]) * s * 1.08 + max(flo[2], 0.0) - 0.4     # studs just into the grass
        if z > 10.0:
            z = 10.0 + (z - 10.0) * 0.55   # a lower sock collar (the tall heel tab read as a patch on the calf)
        comp.append(unreal.Vector(-x if mirror else x, y, z))
    # (fit_collar() is kept for reference but off: scaling per slice tore the thin multi-shell collar into spikes)
    zs = [v.z for v in comp]
    z0 = min(zs)
    out = unreal.DynamicMesh()
    out = pick(GA.copy_mesh_from_static_mesh(src, out, unreal.GeometryScriptCopyMeshFromAssetOptions(), read), unreal.DynamicMesh) or out
    vl = pick(GL.convert_array_to_vector_list(comp), unreal.GeometryScriptVectorList)
    unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(out, vl)
    if mirror:
        unreal.GeometryScript_MeshNormals.flip_normals(out) if hasattr(unreal, "GeometryScript_MeshNormals") else None
        unreal.GeometryScript_Normals.flip_normals(out)
    # smooth shared normals (the faceted heel), then a sole mask in vertex colour red: 0 on the sole and studs
    unreal.GeometryScript_Normals.set_per_vertex_normals(out)
    unreal.GeometryScript_Normals.recompute_normals(out, unreal.GeometryScriptCalculateNormalsOptions())
    cols = [unreal.LinearColor(min(max((z - z0 - 1.6) / 1.0, 0.0), 1.0), 0, 0, 1) for z in zs]
    cl = pick(GL.convert_array_to_color_list(cols), unreal.GeometryScriptColorList)
    unreal.GeometryScript_VertexColors.set_mesh_per_vertex_colors(out, cl)
    GT.transform_mesh(out, bone.inverse())
    path = "%s/SM_PK_Boot_%s" % (OUT, side.upper())
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.set_editor_property("enable_nanite", False)
    opts.set_editor_property("enable_collision", False)
    mesh = pick(GN.create_new_static_mesh_asset_from_mesh(out, path, opts), unreal.StaticMesh)
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, unreal.load_asset(OUT + "/M_PK_Boot"))
    EAL.save_asset(path)
    b = mesh.get_bounding_box()
    log("%s: %d tris, bone-space box (%.1f %.1f %.1f)-(%.1f %.1f %.1f)" % (path, mesh.get_num_triangles(0), b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))


# ---- material ------------------------------------------------------------------------------------------------------
BOOT = r"""
// boot-local height above the sole from world position is not available on a rigid attached mesh, so use the
// pre-baked vertex height: local Z in foot-bone space is not "up"; the sole is picked by the world normal instead
float3 n = normalize(Nrm);
float sole = 1.0 - saturate(VC.r);                         // baked: the sole plate and studs
float3 c = lerp(Upper.rgb, Sole.rgb, sole);
// a trim flash along the side, from the object-space position (stable as the foot moves)
// one swept band on each side (several parallel stripes read as a sportswear brand mark)
float flash = Flash * step(0.55, abs(n.x)) * step(abs(LP.y * 0.8 + LP.z * 0.5 - 6.0), 0.9);
return lerp(c, Trim.rgb, flash);
"""


def make_material():
    path = OUT + "/M_PK_Boot"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_PK_Boot", OUT, unreal.Material, unreal.MaterialFactoryNew())
    def node(cls, x, y, **kw):
        n = MEL.create_material_expression(mat, cls, x, y)
        for k, v in kw.items():
            n.set_editor_property(k, v)
        return n
    up = node(unreal.MaterialExpressionVectorParameter, -800, 0, parameter_name="Upper", default_value=unreal.LinearColor(0.9, 0.9, 0.9, 1))
    tr = node(unreal.MaterialExpressionVectorParameter, -800, 120, parameter_name="Trim", default_value=unreal.LinearColor(0.02, 0.02, 0.02, 1))
    fl = node(unreal.MaterialExpressionScalarParameter, -800, 240, parameter_name="Flash", default_value=0.0)
    so = node(unreal.MaterialExpressionVectorParameter, -800, 540, parameter_name="Sole", default_value=unreal.LinearColor(0.03, 0.03, 0.035, 1))
    vc = node(unreal.MaterialExpressionVertexColor, -800, 640)
    nr = node(unreal.MaterialExpressionVertexNormalWS, -800, 340)
    lp = node(unreal.MaterialExpressionLocalPosition, -800, 440)
    cn = node(unreal.MaterialExpressionCustom, -400, 150, code=BOOT, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm in ("Upper", "Trim", "Flash", "Nrm", "LP", "Sole", "VC"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    cn.set_editor_property("inputs", ins)
    for nm, n in (("Upper", up), ("Trim", tr), ("Flash", fl), ("Nrm", nr), ("LP", lp), ("Sole", so), ("VC", vc)):
        MEL.connect_material_expressions(n, "", cn, nm)
    MEL.connect_material_property(cn, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = node(unreal.MaterialExpressionConstant, -400, 400, r=0.35)
    MEL.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    EAL.save_asset(path)
    return mat


try:
    make_material()
    baked(False)
    baked(True)
    EAL.delete_directory(TMP)
    log("done")
except Exception as ex:
    import traceback
    unreal.log_error("[Boots] FAILED %s %s" % (ex, traceback.format_exc()))
