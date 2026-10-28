"""Builds the stadium materials (procedural, no textures) and assigns them to the meshes in /Game/PitchKings/Stadium.
Run headless after import_stadium.py:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_stadium_materials.py

MPC_PK_Stadium (material parameter collection, set from C++ by APKPitch):
  HomeA, HomeB, AwayA, AwayB   club colours for seats and fans
  Excite                       x: home fans, y: away fans (0 calm .. 1 goal celebration)

M_PK_Pitch      the pitch: mowing stripes, colour noise, wear and every FIFA marking drawn from world position
M_PK_Prop       opaque master for structure (Kind selects a procedural pattern) + one instance per mesh slot
M_PK_Net        masked two-sided goal net
M_PK_Board      emissive LED advertising (per-instance custom data: 0 ad index, 1 ribbon position)
M_PK_Fan        crowd card (per-instance custom data: 0 team 0/1/2, 1 seed, 2 standing)
"""
import unreal

MAT = "/Game/PitchKings/Materials"
STADIUM = "/Game/PitchKings/Stadium"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
MP = unreal.MaterialProperty

# ---------------------------------------------------------------------------------------------------------------
# Parameter collection
# ---------------------------------------------------------------------------------------------------------------
MPC_PATH = MAT + "/MPC_PK_Stadium"


def make_mpc():
    if EAL.does_asset_exist(MPC_PATH):
        mpc = unreal.load_asset(MPC_PATH)
        # keep the existing parameters: new ones get new ids, which orphans every material outside this script that
        # reads the collection (the 3D crowd's M_PK_FanVAT)
        names = [str(v.get_editor_property("parameter_name")) for v in mpc.get_editor_property("vector_parameters")]
        if names == ["HomeA", "HomeB", "AwayA", "AwayB", "Excite"] and len(mpc.get_editor_property("scalar_parameters")) == 3:
            return mpc
    else:
        mpc = AT.create_asset("MPC_PK_Stadium", MAT, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    vecs = []
    for name, val in (("HomeA", (0.6, 0.02, 0.02, 1)), ("HomeB", (0.9, 0.9, 0.9, 1)), ("AwayA", (0.02, 0.1, 0.5, 1)),
                      ("AwayB", (0.9, 0.9, 0.9, 1)), ("Excite", (0, 0, 0, 0))):
        v = unreal.CollectionVectorParameter()
        v.set_editor_property("parameter_name", name)
        v.set_editor_property("default_value", unreal.LinearColor(*val))
        vecs.append(v)
    mpc.set_editor_property("vector_parameters", vecs)
    scalars = []
    for name in ("HomeScore", "AwayScore", "Clock"):
        sp = unreal.CollectionScalarParameter()
        sp.set_editor_property("parameter_name", name)
        sp.set_editor_property("default_value", 0.0)
        scalars.append(sp)
    mpc.set_editor_property("scalar_parameters", scalars)
    EAL.save_asset(MPC_PATH)
    return mpc


# ---------------------------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------------------------
def fresh(name, path=MAT):
    full = path + "/" + name
    # Rebuilt from scratch (clearing the expressions of a loaded material asserts in the editor).
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = AT.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    return mat, full


def custom(mat, code, out_type, x, y, inputs, desc):
    """inputs: list of (name, expression[, output name])"""
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out_type)
    node.set_editor_property("description", desc)
    ins = []
    for item in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", item[0])
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    for item in inputs:
        MEL.connect_material_expressions(item[1], item[2] if len(item) > 2 else "", node, item[0])
    return node


def expr(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def mpc_param(mat, mpc, name, x, y):
    return expr(mat, unreal.MaterialExpressionCollectionParameter, x, y, collection=mpc, parameter_name=name)


def scalar(mat, name, val, x, y):
    return expr(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=val)


def vector(mat, name, val, x, y):
    return expr(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*val))


def finish(mat, full):
    MEL.recompile_material(mat)
    EAL.save_asset(full)
    unreal.log("[StadiumMat] built " + full)


# ---------------------------------------------------------------------------------------------------------------
# Shared HLSL: cheap hash / value noise
# ---------------------------------------------------------------------------------------------------------------
NOISE = r"""
struct PKN
{
	// Periodic hash (period 512 cells): exact for lattice points, so value noise stays seamless and precise far from the origin.
	float H(float2 p) { p = p - 512.0 * floor(p / 512.0); p = frac(p * float2(0.1234, 0.4562) + 0.17); p += dot(p, p + 45.32); return frac(p.x * p.y); }
	float V(float2 p)
	{
		float2 i = floor(p), f = frac(p);
		float2 u = f * f * (3.0 - 2.0 * f);
		return lerp(lerp(H(i), H(i + float2(1, 0)), u.x), lerp(H(i + float2(0, 1)), H(i + float2(1, 1)), u.x), u.y);
	}
	float F(float2 p) { return V(p) * 0.5 + V(p * 2.03 + 17.1) * 0.3 + V(p * 4.11 + 3.7) * 0.2; }
};
PKN N;
"""

# ---------------------------------------------------------------------------------------------------------------
# Grass + markings
# ---------------------------------------------------------------------------------------------------------------
GRASS_LIB = NOISE + r"""
float L = 5250.0, W = 3400.0;
float2 p = WP.xy;
float ax = abs(p.x), ay = abs(p.y);
float d = 1e5;
// touch and goal lines (outer edge of the line on the boundary: lines are inside the field of play)
if (ax <= L + 6.0) d = min(d, abs(ay - (W - 6.0)));
if (ay <= W + 6.0) d = min(d, abs(ax - (L - 6.0)));
if (ay <= W) d = min(d, abs(p.x));
d = min(d, abs(length(p) - 915.0));
float bx = L - 1650.0, sx = L - 550.0;
if (ay <= 2016.0) d = min(d, abs(ax - bx));
if (ax >= bx && ax <= L) d = min(d, abs(ay - 2016.0));
if (ay <= 916.0) d = min(d, abs(ax - sx));
if (ax >= sx && ax <= L) d = min(d, abs(ay - 916.0));
float2 ps = float2(ax - (L - 1100.0), p.y);
if (ax < bx) d = min(d, abs(length(ps) - 915.0));
d = min(d, abs(length(float2(ax - L, ay - W)) - 100.0));
float dots = min(length(p), length(ps));
float fw = max(length(fwidth(p)) * 0.75, 0.5);
float lineM = 1.0 - smoothstep(6.0 - fw, 6.0 + fw, d);
lineM = max(lineM, 1.0 - smoothstep(11.0 - fw, 11.0 + fw, dots));
lineM *= step(ax, L + fw) * step(ay, W + fw);
"""

GRASS_COLOR = GRASS_LIB + r"""
// mowing stripes across the pitch (bands along the length), light / dark depends on the view direction
float bandPos = (p.x + L) / 525.0;
float stripe = (smoothstep(0.0, 0.06, frac(bandPos)) - smoothstep(0.94, 1.0, frac(bandPos))) * (fmod(floor(bandPos), 2.0) < 0.5 ? 1.0 : -1.0);
float sheen = stripe * (0.5 + 0.5 * abs(Cam.x));
float n1 = N.F(p / 900.0), n2 = N.F(p / 140.0 + 7.0), n3 = N.V(p / 9.0);
float fine = lerp(0.5, n3, saturate(1.0 - fwidth(p.x / 9.0) * 1.5));
// blades: short streaks along the mowing direction (~0.45 cm wide, 1.6 cm long), only where they resolve; longer 5 cm
// streaks read as a smeared, stretched texture. Plus ~25 cm clumps.
float2 bp = float2(p.x / 1.6, p.y / 0.45);
float bfade = saturate(1.0 - max(fwidth(bp.x), fwidth(bp.y)) * 0.6);
float clump = N.V(p / 25.0);
float streak = N.V(float2(bp.x + N.V(bp * float2(0.2, 0.05)) * 3.0, bp.y)) * 0.65 + N.V(bp * float2(2.1, 1.7) + 9.3) * 0.35;
float blades = lerp(0.5, streak, bfade);
fine = blades;
float3 g = float3(0.065, 0.15, 0.042);
g *= 1.0 + sheen * 0.13 + stripe * 0.05;  // the bands also differ in colour, so they show in the shade
g *= 0.86 + n1 * 0.28;
g *= 0.92 + n2 * 0.16;
g *= 0.9 + fine * 0.2;  // visible blade streaks up close
g *= 0.93 + clump * 0.14 * saturate(1.0 - fwidth(p.x / 25.0) * 2.0);  // clumps, faded before they alias
g = lerp(g, g * float3(1.05, 0.95, 0.7), saturate(n1 * 1.5 - 0.6));
// wear: goalmouths, the keeper's six-yard box and the centre circle
float2 gm = float2(ax - (L - 300.0), p.y);
float wear = saturate(1.0 - length(gm / float2(420.0, 520.0))) * 0.8 + saturate(1.0 - length(p) / 600.0) * 0.25;
wear *= saturate(N.F(p / 60.0) * 1.6 - 0.2);
g = lerp(g, float3(0.16, 0.15, 0.07), saturate(wear) * 0.7);
// outside the playing area (run-off): a bit darker
float outside = saturate(max(ax - L, ay - W) / 200.0);
g *= 1.0 - outside * 0.12;
float3 lc = float3(0.82, 0.84, 0.82) * (0.93 + fine * 0.07);
return lerp(g, lc, lineM);
"""

GRASS_NORMAL = GRASS_LIB + r"""
float2 bp = float2(p.x / 1.6, p.y / 0.45);
float fade = saturate(1.0 - max(fwidth(bp.x), fwidth(bp.y)) * 0.6);
float e = 0.35;
float h0 = N.V(bp), hx = N.V(bp + float2(e, 0)), hy = N.V(bp + float2(0, e));
float2 m = p / 40.0;
float m0 = N.V(m), mx = N.V(m + float2(0.2, 0)), my = N.V(m + float2(0, 0.2));
float2 g = float2(h0 - hx, h0 - hy) * 0.45 * fade + float2(m0 - mx, m0 - my) * 0.35;
return normalize(float3(g * (1.0 - lineM), 1.0));
"""

GRASS_ROUGH = GRASS_LIB + r"""
// rougher toward grazing views: a low camera saw the sun's sheen wash the turf beige
float graze = 1.0 - saturate(abs(Cam.z) * 2.5);
return lerp(lerp(0.82, 0.95, graze), 0.62, lineM);
"""

# specular falls off toward grazing (blades shadow each other: real turf keeps its green at low angles)
GRASS_SPEC = r"""
float graze = 1.0 - saturate(abs(Cam.z) * 2.5);
return 0.35 * lerp(1.0, 0.2, graze);
"""


def make_grass():
    mat, full = fresh("M_PK_Pitch")
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -900, 0)
    cam = expr(mat, unreal.MaterialExpressionCameraVectorWS, -900, 150)
    col = custom(mat, GRASS_COLOR, F3, -500, 0, [("WP", wp), ("Cam", cam)], "Grass colour + markings")
    rough = custom(mat, GRASS_ROUGH, F1, -500, 250, [("WP", wp), ("Cam", cam)], "Grass roughness")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    nrm = custom(mat, GRASS_NORMAL, F3, -500, 500, [("WP", wp)], "Grass micro normal")
    MEL.connect_material_property(nrm, "", MP.MP_NORMAL)
    spec = custom(mat, GRASS_SPEC, F1, -300, 400, [("Cam", cam)], "Grass specular")
    MEL.connect_material_property(spec, "", MP.MP_SPECULAR)
    finish(mat, full)


# ---------------------------------------------------------------------------------------------------------------
# Structure master
#   Kind 0 plain + grime, 1 cladding / glazing band (UV v), 2 seats (UV u metres, v rows), 3 roof panels,
#   4 floodlight strip (emissive spots), 5 glass, 6 riser
# ---------------------------------------------------------------------------------------------------------------
PROP_LIB = NOISE + r"""
float k = Kind;
float3 c = Base.rgb;
float rough = Rough;
float3 em = 0;
float grime = N.F(WP.xy / 300.0 + WP.z / 500.0);
c *= 0.85 + grime * 0.3;
if ((k < 0.5 || (k > 5.5 && k < 6.5)) && abs(Nrm.z) < 0.5)
{
	float hz = (abs(Nrm.x) > abs(Nrm.y)) ? WP.y : WP.x;
	float streak = N.V(float2(hz / 18.0, WP.z / 600.0));
	c *= 1.0 + 0.0 * streak;  // (streaks read as drips on large walls)
	c *= 1.0 - 0.18 * step(0.985, frac(WP.z / 120.0)) - 0.12 * step(0.99, frac(hz / 240.0));   // formwork joints
}
if (k > 0.5 && k < 1.5)
{
	// cladding: glazed concourse band at the bottom, dark recessed steel with mullions above
	float mull = step(0.92, frac(UV.x / 2.5));
	float col = step(0.94, frac(UV.x / 8.0));
	if (UV.y < 0.38)
	{
		// concourse glazing just above the top row: warm lit interiors behind dark glass
		float cell = N.H(floor(UV.xx / 2.5));
		c = lerp(float3(0.02, 0.03, 0.04), float3(0.3, 0.25, 0.18), step(0.45, cell));
		rough = 0.15;
		em = c * 2200.0 * step(0.45, cell);
		c = lerp(c, float3(0.16, 0.16, 0.17), max(mull, step(abs(UV.y - 0.19), 0.012)));
	}
	else if (UV.y < 0.44) { c = Base.rgb * 0.8; }                          // slab edge
	else
	{
		// open gap under the roof: deep shadow, steel columns and a lattice of bracing
		float brace = step(abs(frac(UV.x / 8.0 + UV.y * 0.9) - 0.5), 0.012) + step(abs(frac(UV.x / 8.0 - UV.y * 0.9) - 0.5), 0.012);
		c = lerp(float3(0.07, 0.075, 0.085), float3(0.25, 0.26, 0.27), saturate(col + brace * 0.7));
		float lamp = step(length(float2(frac(UV.x / 4.0) - 0.5, (UV.y - 0.47) * 6.0)), 0.12);
		em = lamp * float3(1.0, 0.9, 0.75) * 3000.0;
		rough = 0.6;
	}
}
else if (k > 1.5 && k < 2.5)
{
	// seats: 50 cm pitch along the row, seat on the front part of each tread; club colours in blocks
	float fx = frac(UV.x * 2.0), fy = frac(UV.y);
	float seat = step(0.07, fx) * step(fx, 0.93) * step(0.1, fy) * step(fy, 0.72);
	float row = floor(UV.y);
	float block = floor(UV.x / 6.0);
	float useB = step(0.82, frac(row / 7.0 + 0.1));
	float3 sc = lerp(HomeA.rgb, HomeB.rgb, useB);
	sc *= 0.8 + 0.25 * fy;
	c = lerp(float3(0.26, 0.26, 0.25) * (0.85 + grime * 0.3), sc, seat);
	rough = lerp(0.85, 0.45, seat);
	// stairways every 14 m (light concrete, yellow nosing) and vomitory tunnels in rows 10-13 between them
	// (PKStadium::AisleEvery / IsVomitory in PKPitch.cpp keep the crowd out of the same places)
	float s100 = UV.x * 100.0;
	float aisle = step(fmod(s100, 1400.0), 75.0);
	float vom = 0.0;  // (tunnel openings removed: flat dark slabs read as errors)
	c = lerp(c, lerp(float3(0.42, 0.42, 0.4), float3(0.75, 0.6, 0.08), step(fy, 0.1)), aisle);
	c = lerp(c, float3(0.03, 0.03, 0.035) * (0.4 + 0.6 * fy), vom);
	rough = lerp(rough, 0.9, max(aisle, vom));
}
else if (k > 2.5 && k < 3.5)
{
	// roof underside: panel seams and purlins
	float seam = step(0.96, frac(UV.x / 1.5)) + step(0.94, frac(UV.y * 6.0));
	c *= 1.0 - saturate(seam) * 0.35;
}
else if (k > 3.5 && k < 4.5)
{
	// floodlight strip: rows of lamps
	float2 q = float2(frac(UV.x / 1.2), frac(UV.y * 2.0));
	float lamp = step(length(q - 0.5), 0.33);
	c = lerp(float3(0.05, 0.05, 0.055), float3(1, 1, 1), lamp);
	em = lamp * float3(1.0, 0.97, 0.9) * Emissive;
	rough = 0.3;
}
else if (k > 4.5 && k < 5.5)
{
	// glass boxes: dark tinted with mullions and a few lit interiors
	float cell = N.H(floor(float2(UV.x / 3.0, 0)));
	float mull = step(0.94, frac(UV.x / 3.0)) + step(0.92, frac(UV.y * 2.0));
	c = lerp(float3(0.03, 0.045, 0.06), float3(0.6, 0.5, 0.35), step(0.55, cell) * 0.4);
	em = step(0.55, cell) * float3(0.5, 0.42, 0.3) * 0.8 * Emissive;
	c = lerp(c, float3(0.15, 0.15, 0.16), saturate(mull));
	rough = lerp(0.08, 0.5, saturate(mull));
}
else if (k > 5.5 && k < 6.5)
{
	// riser: concrete; dark where a vomitory tunnel opens (rows 10-13, see the seats)
	c = c * (0.9 + 0.1 * step(0.9, frac(UV.y * 2.0)));
	float rowR = floor(UV.y);
	float vomR = 0.0;
	c = lerp(c, float3(0.012, 0.012, 0.014), vomR);
}
else if (k > 9.5 && k < 10.5)
{
	float u = UV.x, v = UV.y;
	float dg = min(abs(frac((u + v * 9.0) / 6.0) - 0.5), abs(frac((u - v * 9.0) / 6.0) - 0.5)) * 6.0;
	float lattice = 1.0 - smoothstep(0.12, 0.22, dg);
	float floorLine = step(0.9, frac(v * 8.0));
	float litF = step(0.35, N.H(float2(floor(u / 48.0), floor(v * 8.0))));   // whole floors lit along 48 m bays
	float mullion = step(0.93, frac(u / 1.5));
	c = lerp(lerp(float3(0.02, 0.025, 0.035), float3(0.28, 0.24, 0.18), litF * 0.6), float3(0.1, 0.1, 0.11), saturate(floorLine + mullion));
	em = (1.0 - floorLine) * (1.0 - mullion) * litF * float3(1.0, 0.85, 0.6) * 600.0 * (1.0 - lattice);
	c = lerp(c, float3(0.8, 0.8, 0.78), lattice);
	rough = lerp(0.12, 0.45, lattice);
}
else if (k > 6.5 && k < 7.5)
{
	// city building: window grid from world position, flat roofs; per-building tint (instance random)
	float3 tint = lerp(float3(0.75, 0.72, 0.68), float3(0.35, 0.38, 0.42), Rand) * (0.75 + 0.5 * frac(Rand * 7.3));
	if (Nrm.z > 0.5) { c = float3(0.18, 0.18, 0.19) * (0.8 + grime * 0.4); rough = 0.9; }
	else
	{
		float h = (abs(Nrm.x) > abs(Nrm.y)) ? WP.y : WP.x;
		// each building its own bay width, storey height and window proportion
		float bayW = lerp(220.0, 420.0, frac(Rand * 3.7)), storey = lerp(320.0, 400.0, frac(Rand * 5.3));
		float ww = lerp(0.12, 0.3, frac(Rand * 9.1));
		float2 g = float2(frac(h / bayW), frac(WP.z / storey));
		float win = step(ww, g.x) * step(g.x, 1.0 - ww) * step(0.25, g.y) * step(g.y, 0.8);
		float lit = step(0.8, N.H(floor(float2(h / bayW, WP.z / storey)) + Rand * 31.0));
		c = lerp(Base.rgb * tint, lerp(float3(0.1, 0.12, 0.15), float3(0.5, 0.42, 0.3), lit * 0.5), win * 0.8);
		rough = lerp(0.85, 0.15, win);
		em = win * lit * float3(1.0, 0.85, 0.6) * 300.0;
		if (Rand > 0.62)
		{
			// glass office tower: reflective blue-grey curtain wall with thin floor bands
			float band = step(0.92, frac(WP.z / 380.0));
			float3 glass = lerp(float3(0.05, 0.08, 0.12), float3(0.1, 0.13, 0.16), frac(Rand * 13.7));
			c = lerp(glass, float3(0.5, 0.52, 0.55), band + step(0.95, frac(h / 150.0)) * 0.6);
			rough = lerp(0.05, 0.5, band);
			em = 0;
		}
		else if (Rand < 0.2)
		{
			// red-brick block
			c = lerp(float3(0.28, 0.1, 0.06) * (0.8 + grime * 0.4), c, win);
		}
	}
}
else if (k > 7.5 && k < 8.5)
{
	// ground around the stadium: stone plaza, asphalt car parks with bay lines, then parkland with paths
	float r = length(WP.xy);
	float2 tile = frac(WP.xy / 120.0);
	float3 plaza = float3(0.42, 0.4, 0.37) * (0.85 + 0.15 * N.H(floor(WP.xy / 120.0))) * (1.0 - 0.3 * step(0.97, max(tile.x, tile.y)));
	float2 bay = frac(WP.xy / float2(260.0, 1100.0));
	float3 asphalt = float3(0.06, 0.06, 0.065) * (0.85 + grime * 0.3);
	asphalt = lerp(asphalt, float3(0.7, 0.7, 0.68), step(0.97, bay.x) * step(0.12, bay.y) * step(bay.y, 0.88));
	float3 park = float3(0.07, 0.13, 0.04) * (0.7 + N.F(WP.xy / 900.0) * 0.6);
	float ang = frac(atan2(WP.y, WP.x) / 6.2831853 * 6.0 + 0.04);
	float avenue = step(abs(ang - 0.5) * 6.2831853 / 6.0 * r, 700.0);
	float ring = step(abs(r - 20500.0), 650.0);
	float road = saturate(avenue + ring);
	float dash = step(abs(r - 20500.0), 12.0) * step(0.5, frac(atan2(WP.y, WP.x) * r / 600.0));
	park = lerp(park, float3(0.05, 0.05, 0.055) * (0.9 + grime * 0.2), road);
	park = lerp(park, float3(0.75, 0.72, 0.6), dash);
	c = r < 11000.0 ? plaza : (r < 17000.0 ? asphalt : park);
	rough = r < 17000.0 ? 0.8 : 0.95;
}
else if (k > 10.5)
{
	// parked car: random paint, dark glass band on the upper part
	float3 paints[6] = { float3(0.6, 0.6, 0.6), float3(0.02, 0.02, 0.02), float3(0.4, 0.02, 0.02), float3(0.02, 0.08, 0.3), float3(0.8, 0.8, 0.78), float3(0.15, 0.15, 0.16) };
	c = paints[(int)(Rand * 5.99)];
	float lz = frac(WP.z / 150.0);
	c = lerp(c, float3(0.02, 0.02, 0.025), step(0.6, UV.y) * 0.0 + step(85.0, WP.z) * step(WP.z, 135.0) * step(abs(Nrm.z), 0.5));
	rough = 0.25;
}
else if (k > 8.5 && k < 9.5)
{
	// foliage
	c = float3(0.04, 0.09, 0.025) * (0.6 + N.F(WP.xy / 150.0 + WP.z / 90.0) * 0.8) * (0.8 + Rand * 0.4);
	rough = 0.9;
}
// aerial perspective outside the bowl: the city, parkland and trees fade toward the sky haze with distance
// (the height fog thins out with altitude and left the towers crisp)
if ((k > 6.5 && k < 9.5) || k > 10.5)
{
	float far = saturate((length(WP.xy) - 14000.0) / 32000.0);
	float haze = far * (0.55 + 0.25 * saturate(WP.z / 6000.0));
	c = lerp(c, float3(0.34, 0.42, 0.52), haze);
	em *= 1.0 - haze;
	rough = lerp(rough, 1.0, haze);
}
"""


def make_prop(mpc):
    mat, full = fresh("M_PK_Prop")
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1100, -200)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1100, -100)
    base = vector(mat, "Base", (0.4, 0.4, 0.4, 1), -1100, 0)
    kind = scalar(mat, "Kind", 0, -1100, 150)
    rough = scalar(mat, "Rough", 0.8, -1100, 250)
    metal = scalar(mat, "Metallic", 0, -1100, 350)
    emis = scalar(mat, "Emissive", 0, -1100, 450)
    ha = mpc_param(mat, mpc, "HomeA", -1100, 550)
    hb = mpc_param(mat, mpc, "HomeB", -1100, 650)
    nrm = expr(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 750)
    rnd = expr(mat, unreal.MaterialExpressionPerInstanceRandom, -1100, 850)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    ins = [("WP", wp), ("UV", uv), ("Base", base), ("Kind", kind), ("Rough", rough), ("Emissive", emis), ("HomeA", ha), ("HomeB", hb), ("Nrm", nrm), ("Rand", rnd)]
    col = custom(mat, PROP_LIB + "return c;", F3, -600, -100, ins, "Prop colour")
    rgh = custom(mat, PROP_LIB + "return rough;", F1, -600, 150, ins, "Prop roughness")
    em = custom(mat, PROP_LIB + "return em;", F3, -600, 400, ins, "Prop emissive")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(rgh, "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(em, "", MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(metal, "", MP.MP_METALLIC)
    finish(mat, full)
    return mat


# ---------------------------------------------------------------------------------------------------------------
# Net
# ---------------------------------------------------------------------------------------------------------------
NET = r"""
float2 q = UV * 100.0 / 10.0;            // 10 cm mesh
float2 f = abs(frac(q) - 0.5);
float2 w = fwidth(q);
float2 cov = saturate((0.018 + w - (0.5 - f)) / max(w * 2.0, 1e-4));
float c = max(cov.x, cov.y);
// Dither the coverage (TSR resolves it to a soft see-through net at a distance instead of a white slab).
return saturate(c * 1.15);
"""


def make_net():
    mat, full = fresh("M_PK_Net")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.4)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    cov = custom(mat, NET, F1, -600, 0, [("UV", uv)], "Net coverage")
    MEL.connect_material_property(cov, "", MP.MP_OPACITY_MASK)
    colr = expr(mat, unreal.MaterialExpressionConstant3Vector, -300, -150, constant=unreal.LinearColor(0.8, 0.8, 0.8, 1))
    MEL.connect_material_property(colr, "", MP.MP_BASE_COLOR)
    r = expr(mat, unreal.MaterialExpressionConstant, -300, 150, r=0.7)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    finish(mat, full)


# ---------------------------------------------------------------------------------------------------------------
# LED boards
# ---------------------------------------------------------------------------------------------------------------
BOARD = NOISE + r"""
// UV: u along the board (0..1), v 0 top .. 1 bottom. D0 = ad group, D1 = board index in its run (ribbon position).
// Each ad: background, a sponsor wordmark from the atlas (row = ad), an accent bar; a wipe changes ad every 9 s.
float t = Time;
float x = UDir * (D1 + UV.x) * BoardLen;          // metres along the ribbon (boards: U runs right-to-left from the pitch, UDir -1)
float y = VFlip > 0.5 ? 1.0 - UV.y : UV.y;
float plain = step(5.5, D0);                      // short corner boards: colour only
float slot = floor(t / 9.0 - D1 * 0.035);         // each board flips whole, a ripple runs along the ribbon
float ad = fmod(D0 - 6.0 * plain + slot, 6.0);
float3 bgs[6] = { float3(0.01, 0.03, 0.2), float3(0.35, 0.01, 0.02), float3(0.06, 0.07, 0.09), float3(0.0, 0.16, 0.06), float3(0.01, 0.01, 0.012), float3(0.85, 0.32, 0.0) };
float3 fgs[6] = { float3(1.0, 0.85, 0.1), float3(1, 1, 1), float3(0.95, 0.95, 0.95), float3(0.85, 1.0, 0.85), float3(0.15, 0.8, 1.0), float3(1, 1, 1) };
int ai = (int)(ad + 0.5);
float3 bg = bgs[ai], fg = fgs[ai];
float logoLen = LogoLen;                           // 8 x the panel height (the atlas rows are 8:1)
// One whole wordmark per logo cell; the atlas row (8:1) is fitted to the panel height.
float2 cell = float2(frac(x / logoLen), y);
float vfit = (cell.y - 0.5) * PanelH * 8.0 / logoLen + 0.5;
float mark = (vfit > 0.02 && vfit < 0.98) ? Texture2DSample(Tex, TexSampler, float2(cell.x, (ad + vfit) / 8.0)).a : 0.0;
float bar = step(0.9, y) * step(frac(x / logoLen + 0.25), 0.5);
float3 c = lerp(bg, fg, saturate(mark * (1.0 - plain) + bar * 0.8));
// LED pixel grid and board frame
float2 px = frac(float2(x * 100.0 / 1.6, y * 92.0 / 1.6));
float dotm = 0.8 + 0.2 * step(length(px - 0.5), 0.45);
float fade = saturate(fwidth(x * 100.0 / 1.6) - 0.5);
c *= lerp(dotm, 0.9, fade);
// board frame (left / right edges only on boards: the roof ribbon's u runs in metres, and the edge test dimmed all of it
// to 5 % except its first metre - a lone bright panel in a dark band)
float sides = Frame > 0.5 ? step(0.006, UV.x) * step(UV.x, 0.994) : 1.0;
c *= sides * step(0.03, y) * step(y, 0.97) * 0.95 + 0.05;
return c;
"""

SCORE = r"""
// Big screen scoreboard: home and away panels with the live score, the clock underneath.
struct SB
{
	float Cap(float2 q, float2 a, float2 b) { float2 pa = q - a, ba = b - a; float t = saturate(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * t); }
	float Digit(float2 q, float n)
	{
		int i = (int)(n + 0.5);
		float w = 4.6, h = 9.6;
		float2 tl = float2(-w, h), tr = float2(w, h), ml = float2(-w, 0), mr = float2(w, 0), bl = float2(-w, -h), br = float2(w, -h);
		float d = 1e5;
		if (i == 0) d = min(min(Cap(q, tl, tr), Cap(q, bl, br)), min(Cap(q, tl, bl), Cap(q, tr, br)));
		else if (i == 1) d = min(min(Cap(q, float2(1.0, h), float2(1.0, -h)), Cap(q, float2(1.0, h), float2(-3.2, h - 4.2))), Cap(q, float2(-3.0, -h), float2(4.6, -h)));
		else if (i == 2) d = min(min(Cap(q, tl, tr), Cap(q, tr, mr)), min(Cap(q, mr, ml), min(Cap(q, ml, bl), Cap(q, bl, br))));
		else if (i == 3) d = min(min(Cap(q, tl, tr), Cap(q, tr, br)), min(Cap(q, bl, br), Cap(q, float2(-1.5, 0), mr)));
		else if (i == 4) d = min(min(Cap(q, tl, ml), Cap(q, ml, mr)), Cap(q, float2(w * 0.55, h), float2(w * 0.55, -h)));
		else if (i == 5) d = min(min(Cap(q, tr, tl), Cap(q, tl, ml)), min(Cap(q, ml, mr), min(Cap(q, mr, br), Cap(q, br, bl))));
		else if (i == 6) d = min(min(Cap(q, tr, tl), Cap(q, tl, bl)), min(Cap(q, bl, br), min(Cap(q, br, mr), Cap(q, mr, ml))));
		else if (i == 7) d = min(Cap(q, tl, tr), Cap(q, tr, float2(-1.0, -h)));
		else if (i == 8) d = min(min(Cap(q, tl, tr), Cap(q, bl, br)), min(min(Cap(q, tl, bl), Cap(q, tr, br)), Cap(q, ml, mr)));
		else d = min(min(Cap(q, tl, tr), Cap(q, tr, br)), min(Cap(q, br, bl), min(Cap(q, tl, ml), Cap(q, ml, mr))));
		return d - 1.9;
	}
	float Number(float2 q, float v)
	{
		float nn = floor(v + 0.5), tens = floor(nn / 10.0), ones = nn - tens * 10.0;
		if (tens > 0.5) return min(Digit(q - float2(-7.6, 0), tens), Digit(q - float2(7.6, 0), ones));
		return Digit(q, ones);
	}
};
SB B;
float2 uv = UV;
float aspect = 1200.0 / 675.0;
float2 q = float2((0.5 - uv.x) * aspect, 0.5 - uv.y) * 100.0;   // centred, 100 units = screen height (U runs right-to-left from the front)
float3 c = lerp(float3(0.01, 0.015, 0.05), float3(0.03, 0.05, 0.12), uv.y);
// team panels
float panelY = step(abs(q.y - 8.0), 26.0);
float left = step(abs(q.x + 44.0), 34.0) * panelY;
float right = step(abs(q.x - 44.0), 34.0) * panelY;
c = lerp(c, HomeA.rgb * 0.9, left);
c = lerp(c, AwayA.rgb * 0.9, right);
c = lerp(c, float3(1.0, 0.75, 0.08), step(abs(q.y - 36.0), 1.2) * step(abs(q.x), 78.0));       // gold rule
float fw = max(fwidth(q.x), 0.05);
float dh = B.Number((q - float2(-44.0, 8.0)) / 1.6, HomeScore) * 1.6;
float da = B.Number((q - float2(44.0, 8.0)) / 1.6, AwayScore) * 1.6;
float digits = 1.0 - smoothstep(-fw, fw, min(dh, da));
c = lerp(c, float3(1, 1, 1), digits);
// dash between the scores
c = lerp(c, float3(1, 1, 1), step(abs(q.x), 4.0) * step(abs(q.y - 8.0), 1.4));
// clock MM:SS
float secs = max(Clock, 0.0);
float mm = floor(secs / 60.0), ss = floor(secs - mm * 60.0);
float2 cq = (q - float2(0.0, -33.0)) / 0.75;
float dm = min(B.Digit(cq - float2(-26.6, 0.0), floor(mm / 10.0)), B.Digit(cq - float2(-11.4, 0.0), mm - floor(mm / 10.0) * 10.0));
float ds = B.Number(cq - float2(19.0, 0.0), ss);
float tensS = floor(ss / 10.0);
ds = tensS < 0.5 ? min(B.Digit(cq - float2(11.4, 0.0), 0.0), B.Digit(cq - float2(26.6, 0.0), ss)) : ds;
float colon = step(length(cq - float2(0.0, 4.0)), 1.6) + step(length(cq - float2(0.0, -4.0)), 1.6);
float clock = 1.0 - smoothstep(-fw, fw, min(dm, ds) * 0.75);
c = lerp(c, float3(1.0, 0.85, 0.25), saturate(clock + colon));
// LED grid
float2 px = frac(uv * float2(1200.0, 675.0) / 2.0);
c *= 0.82 + 0.18 * step(length(px - 0.5), 0.45);
return c;
"""


def make_board():
    mat, full = fresh("M_PK_Board")
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    tm = expr(mat, unreal.MaterialExpressionTime, -1000, 100)
    d0 = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1000, 200, data_index=0)
    d1 = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1000, 300, data_index=1)
    blen = scalar(mat, "BoardLen", 6.0, -1000, 400)
    tex = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, 500, parameter_name="Sponsors",
               texture=unreal.load_asset("/Game/PitchKings/Stadium/T_PK_Sponsors"))
    llen = scalar(mat, "LogoLen", 6.0, -1000, 650)  # one logo per 6 m board
    udir = scalar(mat, "UDir", -1.0, -1000, 750)
    vflip = scalar(mat, "VFlip", 0.0, -1000, 850)
    panelh = scalar(mat, "PanelH", 0.92, -1000, 950)
    frame = scalar(mat, "Frame", 1.0, -1000, 1050)
    col = custom(mat, BOARD, F3, -600, 0, [("UV", uv), ("Time", tm), ("D0", d0), ("D1", d1), ("BoardLen", blen), ("Tex", tex),
                                          ("LogoLen", llen), ("UDir", udir), ("VFlip", vflip), ("PanelH", panelh), ("Frame", frame)], "LED ad")
    bright = scalar(mat, "Brightness", 7000.0, -600, 250)  # nits: the sun is 80,000 lux (physical exposure)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -300, 100)
    MEL.connect_material_expressions(col, "", mul, "A")
    MEL.connect_material_expressions(bright, "", mul, "B")
    MEL.connect_material_property(mul, "", MP.MP_EMISSIVE_COLOR)
    dk = expr(mat, unreal.MaterialExpressionConstant3Vector, -300, -100, constant=unreal.LinearColor(0.01, 0.01, 0.01, 1))
    MEL.connect_material_property(dk, "", MP.MP_BASE_COLOR)
    r = expr(mat, unreal.MaterialExpressionConstant, -300, 250, r=0.35)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    finish(mat, full)


# ---------------------------------------------------------------------------------------------------------------
# Crowd card
# ---------------------------------------------------------------------------------------------------------------
FAN_LIB = r"""
float s = D1;
float h1 = frac(s * 13.17 + 0.11), h2 = frac(s * 71.31 + 0.37), h3 = frac(s * 331.7 + 0.59), h4 = frac(s * 1731.13 + 0.83), h5 = frac(s * 97.7 + 0.21);
float team = D0;
float ex = team < 0.5 ? Excite.x : (team < 1.5 ? Excite.y : (team < 2.5 ? max(Excite.x, Excite.y) * 0.5 : 0.0));
float2 uv = UV;
float stand = saturate(D2);
// figure: v = 0 top of the card. Seated fans sit lower on the card.
float drop = (1.0 - stand) * 0.12;
float2 hc = float2(0.5 + (h1 - 0.5) * 0.08, 0.2 + drop + h2 * 0.04);
float2 dh = (uv - hc) / float2(0.125, 0.105);
float head = step(dot(dh, dh), 1.0);
float sh = hc.y + 0.13;
float tw = 0.29 + h3 * 0.08;
float2 t = uv - float2(0.5, sh);
float shoulder = saturate((0.07 - t.y) / 0.07);
float torso = step(0.0, t.y) * step(abs(t.x), tw - shoulder * shoulder * 0.12);
float neck = step(abs(uv.x - hc.x), 0.05) * step(hc.y, uv.y) * step(uv.y, sh + 0.02);
// arms up when excited (some fans), otherwise down at the sides
float up = step(h4, ex * 0.95);
float arms = 0;
float2 a0, a1, b0, b1;
a0 = float2(0.5 - tw + 0.09, sh + 0.08); b0 = float2(0.5 + tw - 0.09, sh + 0.08);
a1 = float2(0.5 - tw - 0.03, 0.04 + h5 * 0.05); b1 = float2(0.5 + tw + 0.03, 0.04 + h5 * 0.05);
float2 pa = uv - a0, ba = a1 - a0; float ta = saturate(dot(pa, ba) / dot(ba, ba));
float2 pb = uv - b0, bb = b1 - b0; float tb = saturate(dot(pb, bb) / dot(bb, bb));
arms = up * max(step(length(pa - ba * ta), 0.062 - 0.012 * ta), step(length(pb - bb * tb), 0.062 - 0.012 * tb));
float fists = up * max(step(length(uv - a1), 0.06), step(length(uv - b1), 0.06));
arms = max(arms, fists);
// scarf held up between the hands
float scarf = up * step(0.6, h3) * step(abs(uv.y - (a1.y + 0.03)), 0.035) * step(a1.x, uv.x) * step(uv.x, b1.x);
// arms down at the sides when not raised
float side = (1.0 - up) * step(sh + 0.03, uv.y) * step(uv.y, 0.92 - drop * 0.3) * step(abs(abs(uv.x - 0.5) - (tw + 0.035)), 0.04);
float hand = side * step(0.84 - drop * 0.3, uv.y);
// cap (some fans): crown on the head plus a brim
float cap = step(0.86, h2) * step(uv.y, hc.y - 0.01) * step(dot(dh, dh), 1.1);
float brim = step(0.86, h2) * step(abs(uv.y - (hc.y - 0.005)), 0.018) * step(abs(uv.x - hc.x - 0.04), 0.15);
float camM = step(3.5, team) * step(abs(uv.x - hc.x), 0.16) * step(abs(uv.y - hc.y - 0.02), 0.07);
// gestures: some fans clap (hands together in front of the chest), some wave one arm
float gest = frac(h4 * 7.3 + h2);
float clapT = sin(Time * 9.0 + s * 50.0);
float clap = (1.0 - up) * step(gest, 0.25) * step(0.2, ex) * step(length((uv - float2(0.5 + clapT * 0.04 * step(0.0, uv.x - 0.5), sh + 0.2)) / float2(0.09, 0.05)), 1.0);
float waveA = sin(Time * 5.0 + s * 30.0) * 0.08;
float2 wa = float2(0.5 + tw - 0.06, sh + 0.06), wb = float2(0.5 + tw + 0.08 + waveA, 0.03);
float2 wp = uv - wa, wv = wb - wa; float wt = saturate(dot(wp, wv) / dot(wv, wv));
float wave = (1.0 - up) * step(0.25, gest) * step(gest, 0.38) * step(0.3, ex) * step(length(wp - wv * wt), 0.05);
float mask = saturate(head + torso + neck + arms + scarf + side + brim + camM + clap + wave);
"""

FAN_COLOR = FAN_LIB + r"""
float3 skins[4] = { float3(0.62, 0.42, 0.3), float3(0.45, 0.3, 0.2), float3(0.25, 0.15, 0.09), float3(0.7, 0.52, 0.4) };
float3 skin = skins[(int)(h2 * 3.99)] * (0.85 + h5 * 0.3);
float3 hair = lerp(float3(0.02, 0.015, 0.01), float3(0.25, 0.15, 0.06), step(0.75, h1));
float3 ca = team < 0.5 ? HomeA.rgb : AwayA.rgb;
float3 cb = team < 0.5 ? HomeB.rgb : AwayB.rgb;
float3 shirt;
if (team > 3.5) shirt = float3(0.02, 0.02, 0.025);                                               // photographer
else if (team > 2.5) shirt = lerp(float3(0.85, 0.8, 0.02), float3(0.5, 0.5, 0.5), step(abs(frac(uv.y * 5.0) - 0.5), 0.06)); // steward hi-vis
else if (team > 1.5) shirt = lerp(float3(0.05, 0.05, 0.06), float3(0.5, 0.48, 0.45), h4) * float3(0.8 + h1 * 0.4, 0.85, 0.8 + h2 * 0.4);
else
{
	float3 casual[8] = { float3(0.015, 0.015, 0.02), float3(0.08, 0.08, 0.09), float3(0.03, 0.05, 0.12), float3(0.22, 0.22, 0.22),
	                     float3(0.14, 0.08, 0.04), float3(0.05, 0.09, 0.05), float3(0.35, 0.33, 0.3), float3(0.25, 0.04, 0.04) };
	float3 cas = casual[(int)(h1 * 7.99)] * (0.75 + h3 * 0.5);
	shirt = h5 < 0.3 ? ca * (0.85 + h3 * 0.2) : (h5 < 0.4 ? cb * 0.85 : cas);
}
float3 c = shirt;
c = lerp(c, shirt * 0.75, side);                          // sleeves a touch darker than the chest
c = lerp(c, skin, saturate(neck + arms * 0.7 + hand + clap + wave * 0.7));
float hairStyle = h3;                                     // < 0.12 bald, < 0.7 short, else long
float hairTop = step(uv.y, hc.y - 0.02 + h3 * 0.03) * step(0.12, hairStyle);
float longHair = step(0.7, hairStyle) * step(abs(uv.x - hc.x), 0.12) * step(uv.y, hc.y + 0.1) * (1.0 - step(abs(uv.x - hc.x), 0.07) * step(hc.y - 0.03, uv.y));
c = lerp(c, lerp(skin, hair, max(hairTop, longHair)), head);
// face: eyes and mouth, faded out before they alias
float faceVis = saturate(1.0 - fwidth(uv.x) * 90.0) * head * (1.0 - max(hairTop, longHair));
float eyes = step(length((float2(abs(uv.x - hc.x) - 0.04, uv.y - hc.y - 0.005)) / float2(0.016, 0.012)), 1.0);
float mouth = step(abs(uv.x - hc.x), 0.035) * step(abs(uv.y - hc.y - 0.055), 0.007);
c = lerp(c, float3(0.05, 0.035, 0.03), saturate(eyes + mouth * 0.8) * faceVis);
c = lerp(c, hair, longHair * (1.0 - head) * step(uv.y, sh + 0.04));
c = lerp(c, team < 1.5 ? ca : float3(0.1, 0.1, 0.1), saturate(cap * head + brim));
c = lerp(c, team < 1.5 ? cb : ca, scarf);
// photographer's camera and long lens in front of the face
float cam = step(3.5, team) * step(abs(uv.x - hc.x), 0.16) * step(abs(uv.y - hc.y - 0.02), 0.07);
c = lerp(c, float3(0.06, 0.06, 0.065), cam);
// volume: the torso is a cylinder and the head a sphere, lit from above-left; lower body shaded by the row in front
float xn = saturate(abs(uv.x - 0.5) / max(tw, 0.01));
float bodyShade = 0.62 + 0.38 * cos(xn * 1.45) + (0.5 - uv.x) * 0.15;
float2 hq = dh * float2(1.0, 1.0);
float headShade = 0.75 + 0.35 * saturate(1.0 - length(hq - float2(-0.35, -0.4)));
c *= lerp(bodyShade, headShade, head);
c *= lerp(1.0, 0.4, saturate((uv.y - 0.55) / 0.45));
return c;
"""

FAN_MASK = FAN_LIB + "return mask;"

FAN_WPO = r"""
float s = D1;
float h4 = frac(s * 1731.13 + 0.83);
float team = D0;
float ex = team < 0.5 ? Excite.x : (team < 1.5 ? Excite.y : (team < 2.5 ? max(Excite.x, Excite.y) * 0.5 : 0.0));
float idle = sin(Time * (1.3 + s) + s * 40.0) * 1.2;
float jump = max(0.0, sin(Time * (8.0 + h4 * 3.0) + s * 60.0)) * 22.0 * ex * step(h4, ex * 0.95);
return float3(0, 0, idle + jump);
"""


def make_fan(mpc):
    mat, full = fresh("M_PK_Fan")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    d0 = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1100, 100, data_index=0)
    d1 = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1100, 200, data_index=1)
    d2 = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1100, 300, data_index=2)
    ex = mpc_param(mat, mpc, "Excite", -1100, 400)
    ha = mpc_param(mat, mpc, "HomeA", -1100, 500)
    hb = mpc_param(mat, mpc, "HomeB", -1100, 600)
    aa = mpc_param(mat, mpc, "AwayA", -1100, 700)
    ab = mpc_param(mat, mpc, "AwayB", -1100, 800)
    tm = expr(mat, unreal.MaterialExpressionTime, -1100, 900)
    base = [("UV", uv), ("D0", d0), ("D1", d1), ("D2", d2), ("Excite", ex), ("Time", tm)]
    col = custom(mat, FAN_COLOR, F3, -600, 0, base + [("HomeA", ha), ("HomeB", hb), ("AwayA", aa), ("AwayB", ab)], "Fan colour")
    mask = custom(mat, FAN_MASK, F1, -600, 300, base, "Fan mask")
    wpo = custom(mat, FAN_WPO, F3, -600, 600, [("D0", d0), ("D1", d1), ("Excite", ex), ("Time", tm)], "Fan bob")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(mask, "", MP.MP_OPACITY_MASK)
    MEL.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)
    fill = expr(mat, unreal.MaterialExpressionMultiply, -300, 100)
    MEL.connect_material_expressions(col, "", fill, "A")
    fill.set_editor_property("const_b", 450.0)  # nits: bounce light from the sunlit pitch onto the stands
    MEL.connect_material_property(fill, "", MP.MP_EMISSIVE_COLOR)
    r = expr(mat, unreal.MaterialExpressionConstant, -300, 200, r=0.85)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    finish(mat, full)


# ---------------------------------------------------------------------------------------------------------------
# Instances of the structure master + slot assignment
# ---------------------------------------------------------------------------------------------------------------
# name: (base colour, kind, roughness, metallic, emissive)
PROPS = {
    "Concrete": ((0.36, 0.35, 0.33), 0, 0.85, 0, 0),
    "Riser": ((0.22, 0.22, 0.22), 6, 0.9, 0, 0),
    "Seats": ((0.3, 0.3, 0.3), 2, 0.6, 0, 0),
    "Cladding": ((0.5, 0.51, 0.53), 1, 0.6, 0.3, 0),
    "Glass": ((0.05, 0.06, 0.07), 5, 0.1, 0, 2500),
    "Fascia": ((0.05, 0.05, 0.06), 0, 0.5, 0.2, 0),
    "RoofUnder": ((0.62, 0.63, 0.65), 3, 0.5, 0.2, 0),
    "RoofTop": ((0.55, 0.56, 0.58), 3, 0.45, 0.4, 0),
    "Lights": ((0.1, 0.1, 0.1), 4, 0.3, 0, 60000),
    "Steel": ((0.75, 0.76, 0.78), 0, 0.35, 0.8, 0),
    "Frame": ((0.92, 0.92, 0.92), 0, 0.3, 0, 0),
    "Support": ((0.35, 0.36, 0.38), 0, 0.4, 0.6, 0),
    "Pole": ((0.85, 0.85, 0.82), 0, 0.4, 0, 0),
    "Flag": ((0.85, 0.15, 0.05), 0, 0.7, 0, 0),
    "Perspex": ((0.12, 0.14, 0.16), 0, 0.4, 0, 0),  # satin: a mirror canopy reflected the seats as smeared ghosts (SSR + TSR)
    "Seat": ((0.6, 0.05, 0.05), 0, 0.5, 0, 0),
    "Floor": ((0.15, 0.15, 0.15), 0, 0.9, 0, 0),
    "Ground": ((0.2, 0.2, 0.2), 8, 0.9, 0, 0),
    "Truss": ((0.55, 0.57, 0.6), 0, 0.45, 0.6, 0),  # light painted steel: dark girders read as stray slivers
    "Facade": ((0.6, 0.6, 0.6), 10, 0.4, 0.2, 0),
    "ScreenFrame": ((0.03, 0.03, 0.035), 0, 0.4, 0.3, 0),
    "Building": ((0.62, 0.6, 0.57), 7, 0.8, 0, 1),
    "Foliage": ((0.05, 0.1, 0.03), 9, 0.9, 0, 0),
    "Bark": ((0.12, 0.08, 0.05), 0, 0.9, 0, 0),
    "Car": ((0.5, 0.5, 0.5), 11, 0.3, 0.4, 0),
}


def make_score(mpc):
    mat, full = fresh("M_PK_Score")
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    ins = [("UV", uv)]
    for i, name in enumerate(("HomeA", "AwayA", "HomeScore", "AwayScore", "Clock")):
        ins.append((name, mpc_param(mat, mpc, name, -1000, 100 + 100 * i)))
    col = custom(mat, SCORE, F3, -600, 0, ins, "Scoreboard")
    mul = expr(mat, unreal.MaterialExpressionMultiply, -300, 100)
    MEL.connect_material_expressions(col, "", mul, "A")
    mul.set_editor_property("const_b", 3200.0)
    MEL.connect_material_property(mul, "", MP.MP_EMISSIVE_COLOR)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mul.set_editor_property("const_b", 6000.0)
    finish(mat, full)


BANNER = r"""
// Supporters' banner in the club colours with the club text (atlas rows 6-7), slightly creased cloth.
float3 a = Team < 0.5 ? HomeA.rgb : AwayA.rgb;
float3 b = Team < 0.5 ? HomeB.rgb : AwayB.rgb;
float u = 1.0 - UV.x, v = UV.y;
float border = step(v, 0.1) + step(0.9, v) + step(u, 0.012) + step(0.988, u);
float vfit = (v - 0.5) * 1.3 * 8.0 / 10.0 + 0.5;
float text = (vfit > 0.02 && vfit < 0.98) ? Texture2DSample(Tex, TexSampler, float2(u, (Row + vfit) / 8.0)).a : 0.0;
float3 c = lerp(a, b, saturate(border + text));
c *= 0.85 + 0.15 * sin(u * 60.0 + sin(v * 9.0) * 2.0);   // creases
return c;
"""


def make_banner(mpc):
    mat, full = fresh("M_PK_Banner")
    mat.set_editor_property("two_sided", True)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    tex = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, 100, parameter_name="Sponsors",
               texture=unreal.load_asset("/Game/PitchKings/Stadium/T_PK_Sponsors"))
    row = scalar(mat, "Row", 6.0, -1000, 200)
    team = scalar(mat, "Team", 0.0, -1000, 300)
    ins = [("UV", uv), ("Tex", tex), ("Row", row), ("Team", team)]
    for i, name in enumerate(("HomeA", "HomeB", "AwayA", "AwayB")):
        ins.append((name, mpc_param(mat, mpc, name, -1000, 400 + 100 * i)))
    col = custom(mat, BANNER, F3, -600, 0, ins, "Banner")
    MEL.connect_material_property(col, "", MP.MP_BASE_COLOR)
    r = expr(mat, unreal.MaterialExpressionConstant, -300, 200, r=0.85)
    MEL.connect_material_property(r, "", MP.MP_ROUGHNESS)
    finish(mat, full)


def make_instances(master):
    out = {}
    for name, (col, kind, rough, metal, emis) in PROPS.items():
        iname = "MI_PK_" + name
        full = MAT + "/" + iname
        if EAL.does_asset_exist(full):
            mi = unreal.load_asset(full)
        else:
            mi = AT.create_asset(iname, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        MEL.set_material_instance_vector_parameter_value(mi, "Base", unreal.LinearColor(col[0], col[1], col[2], 1))
        MEL.set_material_instance_scalar_parameter_value(mi, "Kind", float(kind))
        MEL.set_material_instance_scalar_parameter_value(mi, "Rough", float(rough))
        MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", float(metal))
        MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", float(emis))
        MEL.update_material_instance(mi)
        EAL.save_asset(full)
        out[name] = mi
    return out


def assign(instances):
    ribbon_path = MAT + "/MI_PK_Ribbon"
    ribbon = unreal.load_asset(ribbon_path) if EAL.does_asset_exist(ribbon_path) else AT.create_asset("MI_PK_Ribbon", MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(ribbon, unreal.load_asset(MAT + "/M_PK_Board"))
    MEL.set_material_instance_scalar_parameter_value(ribbon, "BoardLen", 1.0)
    MEL.set_material_instance_scalar_parameter_value(ribbon, "Brightness", 2500.0)  # readable against the sky, not a glare band
    MEL.set_material_instance_scalar_parameter_value(ribbon, "Frame", 0.0)          # one continuous ribbon, no board edges
    MEL.set_material_instance_scalar_parameter_value(ribbon, "LogoLen", 16.0)
    MEL.set_material_instance_scalar_parameter_value(ribbon, "UDir", 1.0)
    MEL.set_material_instance_scalar_parameter_value(ribbon, "VFlip", 1.0)   # the ribbon strips run bottom-up
    MEL.set_material_instance_scalar_parameter_value(ribbon, "PanelH", 2.0)
    MEL.update_material_instance(ribbon)
    EAL.save_asset(ribbon_path)
    big_path = MAT + "/MI_PK_BigScreen"
    big = unreal.load_asset(big_path) if EAL.does_asset_exist(big_path) else AT.create_asset("MI_PK_BigScreen", MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(big, unreal.load_asset(MAT + "/M_PK_Board"))
    MEL.set_material_instance_scalar_parameter_value(big, "BoardLen", 12.0)
    MEL.set_material_instance_scalar_parameter_value(big, "Brightness", 3500.0)
    MEL.update_material_instance(big)
    EAL.save_asset(big_path)
    special = {"Net": unreal.load_asset(MAT + "/M_PK_Net"), "Screen": unreal.load_asset(MAT + "/M_PK_Board"), "Ribbon": ribbon,
               "Crowd": unreal.load_asset(MAT + "/M_PK_Fan")}
    special["Glow"] = unreal.load_asset(MAT + "/M_PK_Surface")
    special["BigScreen"] = unreal.load_asset(MAT + "/M_PK_Score")
    special["Banner"] = unreal.load_asset(MAT + "/M_PK_Banner")
    for mesh_name in ("SM_PK_Bowl", "SM_PK_Roof", "SM_PK_Goal", "SM_PK_Board", "SM_PK_CrowdCard", "SM_PK_CornerFlag", "SM_PK_Dugout",
                      "SM_PK_Ring", "SM_PK_Chevron", "SM_PK_Tree", "SM_PK_Screen", "SM_PK_Car", "SM_PK_Banner"):
        full = STADIUM + "/" + mesh_name
        mesh = unreal.load_asset(full)
        if not mesh:
            unreal.log_warning("[StadiumMat] missing " + full)
            continue
        mats = mesh.static_materials
        for i, sm in enumerate(mats):
            slot = str(sm.material_slot_name)
            m = special.get(slot) or instances.get(slot)
            if slot == "Frame" and mesh_name == "SM_PK_Board":
                m = instances["Fascia"]
            if m:
                mesh.set_material(i, m)
            else:
                unreal.log_warning("[StadiumMat] no material for slot %s on %s" % (slot, mesh_name))
        # the crowd and boards never cast shadows; flat cards need no lighting build
        if mesh_name in ("SM_PK_CrowdCard",):
            pass
        EAL.save_asset(full)
        unreal.log("[StadiumMat] assigned " + mesh_name)


mpc = make_mpc()
make_grass()
master = make_prop(mpc)
make_net()
make_board()
make_fan(mpc)
make_score(mpc)
make_banner(mpc)
assign(make_instances(master))


# Report materials whose shaders do not compile (a broken Custom node falls back to the default grid material).
for _name in ("M_PK_Pitch", "M_PK_Prop", "M_PK_Net", "M_PK_Board", "M_PK_Fan", "M_PK_Score", "M_PK_Banner"):
    _m = unreal.load_asset(MAT + "/" + _name)
    _errors = []
    try:
        _errors = list(unreal.MaterialEditingLibrary.get_material_compile_errors(_m)) if hasattr(unreal.MaterialEditingLibrary, "get_material_compile_errors") else []
    except Exception as _e:
        _errors = ["(could not query: %s)" % _e]
    unreal.log("[StadiumMat] compile %s: %s" % (_name, "OK" if not _errors else "; ".join(str(x) for x in _errors)))
