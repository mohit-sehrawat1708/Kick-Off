"""Builds the football kit materials for the UE5 mannequin (run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/make_kit_material.py).

M_PK_Kit       opaque, on the body: paints shirt / shorts / socks / boots / skin / hair from the mesh's reference pose.
M_PK_KitShell  masked + world position offset, on a second copy of the body that follows its pose: gives the cloth
               thickness (sleeve and shorts hems, flared shorts, sock tops) and volume to the hair.

Every point is classified by its distance to the mannequin's bones in the reference pose (PreSkinnedLocalPosition,
mesh space: +Y forward, +X the player's left, Z up, feet at 0, 180 cm tall). Parameters (set per player in C++):
  Primary, Secondary, Trim, Shorts, Socks, Boots, NumberColor, Skin, Hair, Gloves  (colours)
  KitA = (pattern, number, sleeve length 0..1, shorts length 0..1)
  KitB = (hair style 0..4, beard 0..1, keeper 0/1, boot accent 0/1)
"""
import unreal

PATH = "/Game/PitchKings/Materials"
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

LIB = r"""
struct KitLib
{
	float2 Seg(float3 p, float3 a, float3 b)
	{
		float3 ab = b - a;
		float t = saturate(dot(p - a, ab) / dot(ab, ab));
		return float2(length(p - (a + ab * t)), t);
	}
	// x: region (0 torso, 1 neck, 2 head, 3 upper arm, 4 forearm, 5 hand, 6 thigh, 7 calf, 8 foot), y: t along the bone
	float2 Classify(float3 p)
	{
		float3 q = float3(abs(p.x), p.y, p.z);
		float3 qt = float3(q.x * 0.62, q.y, q.z);
		float best = 1e5;
		float2 r = float2(0, 0);
		float2 s;
		s = Seg(qt, float3(0, 2.3, 95.9), float3(0, 0.5, 141.1)); if (s.x / 12.5 < best) { best = s.x / 12.5; r = float2(0, s.y); }
		s = Seg(qt, float3(0, 0.5, 141.1), float3(0, -1.6, 150.0)); if (s.x / 12.5 < best) { best = s.x / 12.5; r = float2(0, 1); }
		s = Seg(q, float3(1.4, -1.7, 146.3), float3(17.0, -2.6, 144.0)); if (s.x / 7.5 < best) { best = s.x / 7.5; r = float2(0, 1); }
		s = Seg(q, float3(0, -1.6, 150.0), float3(0, 0.7, 162.6)); if (s.x / 6.5 < best) { best = s.x / 6.5; r = float2(1, s.y); }
		s = Seg(q, float3(0, 0.7, 164.0), float3(0, 2.0, 176.0)); if (s.x / 10.5 < best) { best = s.x / 10.5; r = float2(2, s.y); }
		s = Seg(q, float3(19.0, -2.6, 143.6), float3(35.0, -1.9, 120.9)); if (s.x / 5.5 < best) { best = s.x / 5.5; r = float2(3, s.y); }
		s = Seg(q, float3(35.0, -1.9, 120.9), float3(47.8, 15.7, 104.5)); if (s.x / 4.3 < best) { best = s.x / 4.3; r = float2(4, s.y); }
		s = Seg(q, float3(47.8, 15.7, 104.5), float3(55.3, 26.0, 94.9)); if (s.x / 4.0 < best) { best = s.x / 4.0; r = float2(5, s.y); }
		s = Seg(q, float3(10.0, 2.6, 93.5), float3(12.3, 2.6, 50.3)); if (s.x / 8.5 < best) { best = s.x / 8.5; r = float2(6, s.y); }
		s = Seg(q, float3(12.3, 2.6, 50.3), float3(14.1, -1.0, 8.2)); if (s.x / 6.0 < best) { best = s.x / 6.0; r = float2(7, s.y); }
		s = Seg(q, float3(14.1, -1.0, 8.2), float3(15.8, 13.9, 0.8)); if (s.x / 5.5 < best) { best = s.x / 5.5; r = float2(8, s.y); }
		s = Seg(q, float3(15.8, 13.9, 0.8), float3(16.3, 21.0, 1.5)); if (s.x / 4.5 < best) { best = s.x / 4.5; r = float2(8, 1); }
		return r;
	}
	// Height above the hairline (cm, negative below it).
	float HairAbove(float3 p, float4 KitB)
	{
		float front = saturate((p.y + 3.0) / 12.0);
		float back = KitB.x > 2.5 ? 157.5 : 164.5;
		return p.z - lerp(back, 177.0, front);
	}
	// Head: 0 skin, 5 hair (and brows), 8 beard, 9 eye white, 10 iris, 11 mouth.
	float Head(float3 p, float3 n, float4 KitB)
	{
		float style = KitB.x;
		// Hairline: low at the back, above the forehead at the front.
		float front = saturate((p.y + 3.0) / 12.0);
		float back = style > 2.5 ? 157.5 : 164.5;
		float hairline = lerp(back, 177.0, front);
		if (style > 0.5 && p.z > hairline) return 5;
		if (n.y > 0.45 && p.y > 4.0)
		{
			float ax = abs(p.x);
			float browZ = 171.4 - 0.1 * (ax - 3.3) * (ax - 3.3);
			if (ax > 1.4 && ax < 5.3 && abs(p.z - browZ) < 0.42) return 5;
			float2 e = float2((ax - 3.25) / 1.0, (p.z - 169.0) / 0.48);
			float e2 = dot(e, e);
			if (e2 < 1.0) return e2 < 0.5 ? 10 : 9;
			if (ax < 2.0 && abs(p.z - 161.9) < 0.22 * (1.0 - ax / 2.0) + 0.12) return 11;
		}
		// Beard: jaw and chin at the front, below the cheekbones; the lips stay clear.
		if (KitB.y > 0.05 && p.y > 4.0 + abs(p.x) * 0.15 && p.z > 157.5 && p.z < 164.6 - abs(p.x) * 0.25 && !(abs(p.x) < 2.3 && p.z > 161.3 && p.z < 162.6)) return 8;
		return 0;
	}
	// Surface: 0 skin, 1 shirt, 2 shorts, 3 socks, 4 boots, 5 hair, 6 gloves, 7 trim, 8 beard
	float Surface(float3 p, float3 n, float4 KitA, float4 KitB)
	{
		float2 r = Classify(p);
		float sleeve = KitA.z;
		float shorts = KitA.w;
		bool keeper = KitB.z > 0.5;
		if (r.x < 0.5)
		{
			if (p.z < 97.5) return 2;
			if (p.z < 99.5) return 7; // waistband
			// crew neck: an ellipse round the base of the neck (shoulders stay shirt), shallow V at the front
			float2 nq = float2(p.x / 8.5, (p.y + 0.8) / 7.0);
			float nr = length(nq);
			float neckTop = 147.5 - (p.y > 2.0 ? max(0.0, 3.0 - abs(p.x) * 0.9) : 0.0);
			if (nr < 1.0 && p.z > neckTop + 1.4) return 0;
			if (nr < 1.15 && p.z > neckTop) return 7; // collar band
			return 1;
		}
		if (r.x < 1.5) return p.z > 160.0 ? Head(p, n, KitB) : 0; // neck: skin (the collar band is on the torso)
		if (r.x < 2.5) return Head(p, n, KitB);
		if (r.x < 3.5)
		{
			if (keeper) return 1;
			if (r.y < sleeve - 0.07) return 1;
			if (r.y < sleeve) return 7; // cuff
			return 0;
		}
		if (r.x < 4.5) return keeper ? (r.y < 0.9 ? 1 : 7) : 0;
		if (r.x < 5.5) return keeper ? 6 : 0;
		if (r.x < 6.5)
		{
			if (r.y < shorts - 0.05) return 2;
			if (r.y < shorts) return 7; // hem
			return 0;
		}
		if (r.x < 7.5)
		{
			if (r.y < 0.14) return 0;
			if (r.y < 0.22) return 7; // sock top
			return 3;
		}
		return 4;
	}
	float Box(float2 q, float2 c, float2 h)
	{
		float2 d = abs(q - c) - h;
		return length(max(d, 0)) + min(max(d.x, d.y), 0);
	}
	float Cap(float2 q, float2 a, float2 b)
	{
		float2 pa = q - a, ba = b - a;
		float t = saturate(dot(pa, ba) / dot(ba, ba));
		return length(pa - ba * t);
	}
	// Football block digit, 12 x 24 cm centred at the origin: strokes joined without gaps, a flag on the 1.
	float Digit(float2 q, float n)
	{
		int i = (int)(n + 0.5);
		float w = 4.6, h = 9.6, th = 1.75;
		float2 tl = float2(-w, h), tr = float2(w, h), ml = float2(-w, 0), mr = float2(w, 0), bl = float2(-w, -h), br = float2(w, -h);
		float d = 1e5;
		if (i == 0) { d = min(min(Cap(q, tl, tr), Cap(q, bl, br)), min(Cap(q, tl, bl), Cap(q, tr, br))); }
		else if (i == 1) { d = min(Cap(q, float2(1.0, h), float2(1.0, -h)), Cap(q, float2(1.0, h), float2(-3.2, h - 4.2))); d = min(d, Cap(q, float2(-3.0, -h), float2(4.6, -h))); }
		else if (i == 2) { d = min(min(Cap(q, tl, tr), Cap(q, tr, mr)), min(Cap(q, mr, ml), min(Cap(q, ml, bl), Cap(q, bl, br)))); }
		else if (i == 3) { d = min(min(Cap(q, tl, tr), Cap(q, tr, br)), min(Cap(q, bl, br), Cap(q, float2(-1.5, 0), mr))); }
		else if (i == 4) { d = min(min(Cap(q, tl, ml), Cap(q, ml, mr)), Cap(q, float2(w * 0.55, h), float2(w * 0.55, -h))); }
		else if (i == 5) { d = min(min(Cap(q, tr, tl), Cap(q, tl, ml)), min(Cap(q, ml, mr), min(Cap(q, mr, br), Cap(q, br, bl)))); }
		else if (i == 6) { d = min(min(Cap(q, tr, tl), Cap(q, tl, bl)), min(Cap(q, bl, br), min(Cap(q, br, mr), Cap(q, mr, ml)))); }
		else if (i == 7) { d = min(Cap(q, tl, tr), Cap(q, tr, float2(-1.0, -h))); }
		else if (i == 8) { d = min(min(Cap(q, tl, tr), Cap(q, bl, br)), min(min(Cap(q, tl, bl), Cap(q, tr, br)), Cap(q, ml, mr))); }
		else { d = min(min(Cap(q, tl, tr), Cap(q, tr, br)), min(Cap(q, br, bl), min(Cap(q, tl, ml), Cap(q, ml, mr)))); }
		return d - th;
	}
	float Number(float2 q, float number)
	{
		float nn = floor(number + 0.5);
		float tens = floor(nn / 10.0);
		float ones = nn - tens * 10.0;
		if (tens > 0.5) return min(Digit(q - float2(-7.6, 0), tens), Digit(q - float2(7.6, 0), ones));
		return Digit(q, ones);
	}
	float3 ShirtColor(float3 p, float pattern, float3 a, float3 b)
	{
		// pattern edges anti-aliased by their screen footprint (hard steps stair-stepped into kinks and wobbles)
		float s = 0;
		if (pattern > 0.5 && pattern < 1.5) { float t = frac(p.x / 9.0 + 0.25); float w = max(fwidth(p.x / 9.0), 1e-3); s = smoothstep(0.5 - w, 0.5 + w, t) * (1.0 - smoothstep(1.0 - w, 1.0, t)); }
		else if (pattern < 2.5 && pattern > 1.5) { float t = frac((p.z - 100.0) / 11.0); float w = max(fwidth(p.z / 11.0), 1e-3); s = smoothstep(0.5 - w, 0.5 + w, t) * (1.0 - smoothstep(1.0 - w, 1.0, t)); }
		else if (pattern < 3.5 && pattern > 2.5) { float d = abs(p.x * 0.85 + (p.z - 122.0)); float w = max(fwidth(d), 1e-3); s = 1.0 - smoothstep(6.5 - w, 6.5 + w, d); }
		else if (pattern < 4.5 && pattern > 3.5) { float w = max(fwidth(p.x), 1e-3); s = smoothstep(-w, w, p.x); }
		else if (pattern > 4.5) { float t = frac(p.x / 5.0); float w = max(fwidth(p.x / 5.0), 1e-3); s = smoothstep(0.86 - w, 0.86 + w, t) * (1.0 - smoothstep(1.0 - w, 1.0, t)); }
		return lerp(a, b, s);
	}
};
"""

COLOR = LIB + r"""
KitLib K;
float3 n = normalize(N);
float surf = K.Surface(P, n, KitA, KitB);
float3 c = Skin.rgb;
if (surf < 0.5)
{
	c = Skin.rgb * float3(1.1, 0.96, 0.9);  // warmer: the arms read grey next to the scattering face
	// wrist tape over the joint where the mannequin's hand meets the forearm (outfield players)
	float2 rw = K.Classify(P);
	if (KitB.z < 0.5 && rw.x > 3.5 && rw.x < 4.5 && rw.y > 0.9) c = float3(0.82, 0.82, 0.8);
}
else if (surf < 1.5)
{
	float2 r = K.Classify(P);
	float3 ps = P;  // same pattern on body and sleeves: no break at the seam
	c = (r.x < 0.5 || r.x > 2.5) ? K.ShirtColor(ps, KitA.x, Primary.rgb, Secondary.rgb) : Primary.rgb;
	// Number on the back (seen from behind, the player's right is the viewer's right: -X).
	if (r.x < 0.5 && n.y < -0.2 && P.z > 108.0 && P.z < 146.0 && abs(P.x) < 12.5 && KitA.y > 0.5)
	{
		float d = K.Number(float2(-P.x, P.z - 126.0) * 0.9, KitA.y) / 0.9;
		float fw = max(fwidth(d), 0.05);
		c = lerp(c, Secondary.rgb * 0.5, smoothstep(0.6 + fw, 0.6 - fw, d) * 0.7);  // thin outline
		c = lerp(c, NumberColor.rgb, smoothstep(fw, -fw, d));
	}
	if (r.x < 0.5 && n.y > 0.35 && P.z > 124.0 && P.z < 142.0 && abs(P.x) < 12.0 && KitA.y > 0.5)
	{
		float d = K.Number(float2(P.x, P.z - 133.0) * 1.9, KitA.y) / 1.9;  // centred upper-chest number (12 cm)
		float fw = max(fwidth(d), 0.03);
		c = lerp(c, NumberColor.rgb, smoothstep(fw, -fw, d));
	}
	// Crest on the left chest.
	if (r.x < 0.5 && n.y > 0.2)
	{
		float2 q = float2(P.x - 9.0, P.z - 139.5);
		float shield = max(abs(q.x) - 2.2 + max(0.0, -q.y) * 0.45, abs(q.y) - 2.6);
		float inner = smoothstep(0.2, -0.2, shield + 0.45);
		c = lerp(c, Trim.rgb, smoothstep(0.2, -0.2, shield));
		c = lerp(c, Secondary.rgb, inner);
		c = lerp(c, Trim.rgb, inner * step(abs(q.y - 0.4 + abs(q.x) * 0.7), 0.4));  // chevron emblem
	}
}
else if (surf < 2.5) c = Shorts.rgb;
else if (surf < 3.5) c = Socks.rgb;
else if (surf < 4.5)
{
	c = Boots.rgb;
	if (P.z < 1.6) c = float3(0.02, 0.02, 0.02);                                   // sole
	// laces across the instep
	float ax2 = abs(P.x);
	if (n.z > 0.35 && P.y > 3.0 && P.y < 12.0 && abs(ax2 - 15.4) < 1.4 && P.z > 4.0) c = lerp(c, float3(0.85, 0.85, 0.85), step(0.55, frac(P.y / 1.3)));
	if (KitB.w > 0.5 && abs(P.z - 4.5) < 0.9 && abs(n.x) > 0.5) c = Trim.rgb;          // side flash
}
else if (surf < 5.5) c = Hair.rgb;
else if (surf < 6.5)
{
	// keeper gloves: a wrist strap in the trim colour, a grippy grey latex palm, the back in the glove colour
	float2 rg = K.Classify(P);
	c = Gloves.rgb;
	if (rg.x > 4.5 && rg.y < 0.2) c = Trim.rgb;
	if (rg.x > 3.5 && rg.x < 4.5) c = Trim.rgb;      // cuff over the end of the forearm
}
else if (surf < 7.5) c = Trim.rgb;
else if (surf < 8.5) c = lerp(Skin.rgb, Hair.rgb, saturate(KitB.y) * 0.75);
else if (surf < 9.5) c = float3(0.42, 0.4, 0.38);   // eye white (in shadow under the brow)
else if (surf < 10.5) c = float3(0.03, 0.02, 0.015); // iris
else c = Skin.rgb * 0.45;                         // mouth
// Fabric: a faint knit so the cloth is not flat plastic.
// (no fine knit pattern: it shimmered as moire)
return c;
"""

BUMP = LIB + r"""
KitLib K;
float3 n = normalize(N);
float surf = K.Surface(P, n, KitA, KitB);
float2 r = K.Classify(P);
float h = 0.0;
float x = P.x, ax = abs(P.x), z = P.z;
if (surf > 0.5 && surf < 3.5)
{
	// cloth: loose folds (stronger at the waist, armpits and behind the knees) and a fine knit, faded with distance
	float folds = sin(P.z * 0.9 + sin(P.x * 0.35) * 2.2) * 0.5 + sin(P.z * 1.7 - P.x * 0.6 + 1.3) * 0.25;
	float where = (r.x < 0.5 ? smoothstep(118.0, 100.0, z) * 0.9 + 0.25 : 0.5) + (r.x > 2.5 && r.x < 3.5 ? 0.5 : 0.0);
	h += folds * where * 0.25;
}
if (surf < 0.5 && r.x > 2.5 && r.x < 5.5)
{
	// limbs: muscle and tendon undulation along the arm, knuckle creases on the hand
	// relief from the position only (distance from the shoulder): any per-region parameter jumps where the bone
	// regions meet, and that jump drew a dashed crease round the forearm
	float3 sh = float3(19.0, -2.6, 143.6);
	float along = length(float3(ax, P.y, P.z) - sh);
	float wristFade = smoothstep(50.0, 42.0, along);
	h += (sin(along * 0.33) * 0.12 + sin(along * 0.21 + atan2(P.y + 2.0, P.z - 120.0) * 3.0) * 0.05) * wristFade;
}
if (surf > 5.5 && surf < 6.5)
{
	// glove padding: a raised strap edge and quilted finger backs
	h += smoothstep(0.17, 0.2, r.y) * smoothstep(0.24, 0.21, r.y) * 0.35 * step(4.5, r.x);
	h += sin(r.y * 40.0) * 0.06 * step(0.35, r.y) * step(4.5, r.x);
}
if (surf > 2.5 && surf < 3.5)
{
	// socks: knit ribbing and a shin-pad bulge on the front of the shin
	h += sin(z * 14.0) * 0.008;  // fine knit rib (the coarse one read as garden hose)
	h += 0.45 * smoothstep(0.0, 1.0, saturate((P.y + 1.0) / 4.0)) * smoothstep(20.0, 26.0, z) * smoothstep(42.0, 36.0, z);
}
if (surf > 3.5 && surf < 4.5 && n.z > 0.35 && P.y > 3.0 && P.y < 12.0 && abs(abs(P.x) - 15.4) < 1.4) h += step(0.55, frac(P.y / 1.3)) * 0.12;
float3 dpdx = ddx(WP), dpdy = ddy(WP);
float dhdx = ddx(h), dhdy = ddy(h);
float3 nn = normalize(NW);
float3 r1 = cross(dpdy, nn), r2 = cross(nn, dpdx);
float det = dot(dpdx, r1);
float3 grad = sign(det) * (dhdx * r1 + dhdy * r2);
return normalize(abs(det) * nn - grad);
"""

ROUGH = LIB + r"""
KitLib K;
float surf = K.Surface(P, normalize(N), KitA, KitB);
if (surf > 3.5 && surf < 4.5) return 0.32;  // boots
if (surf > 4.5 && surf < 5.5) return 0.72;  // hair
if (surf > 0.5 && surf < 3.5) return 0.62;  // cloth
if (surf > 6.5 && surf < 7.5) return 0.55;
return 0.62;                                // skin (matte like the faces)
"""

# Shell: how far the cloth / hair stands off the body (cm, 0 = no shell there).
OFFSET = LIB + r"""
KitLib K;
float3 n = normalize(N);
float surf = K.Surface(P, n, KitA, KitB);
float2 r = K.Classify(P);
bool keeper = KitB.z > 0.5;
// Offsets depend only on the body region and smoothly on the position along it: where the opacity mask cuts the
// shell (hems, cuffs, sock tops) the triangles stay unstretched, so the cut edge is clean.
if (surf > 4.5 && surf < 5.5)
{
	if (r.x < 1.5 || n.z < -0.2) return 0.0;               // eyebrows / under the chin: painted only
	float style = KitB.x;
	float volume = style < 1.5 ? 0.2 : (style < 2.5 ? 0.6 : (style < 3.5 ? 0.95 : 2.2));
	// tufts: the volume varies over the head so the silhouette is not a smooth cap
	float tuft = 0.7 + 0.3 * (sin(P.x * 0.9 + P.z * 0.7) + sin(P.y * 1.1 - P.z * 0.5) * 0.7 + sin((P.x + P.y) * 2.3) * 0.3);
	float frontFade = 1.0 - 0.7 * saturate((P.y + 1.0) / 8.0);  // little volume over the forehead (no fringe over the eyes)
	return volume * tuft * frontFade * saturate(K.HairAbove(P, KitB) / 4.0);
}
if (r.x < 0.5) return lerp(1.25, 0.9, saturate((P.z - 95.5) / 5.0));                       // shorts top -> shirt
if (r.x < 1.5) return 0.0;                                                                     // collar: painted only, clear of the chin
if (r.x < 2.5) return 0.0;
if (r.x < 3.5) return keeper ? 1.0 : 1.0 + 1.1 * saturate(r.y / max(KitA.z, 0.1));         // sleeve flares to the cuff
if (r.x < 4.5) return keeper ? lerp(1.0, 0.7, r.y) : 2.1;
if (r.x < 5.5) return keeper ? 0.5 : 0.0;
if (r.x < 6.5) return lerp(1.5 + 2.2 * saturate(r.y / max(KitA.w, 0.1)), 0.65, smoothstep(KitA.w + 0.04, 0.97, r.y)); // shorts flare to the hem, then back down to the sock top by the knee
if (r.x < 7.5) return (0.35 + 0.3 * (1.0 - smoothstep(0.2, 0.3, r.y))) * (1.0 - smoothstep(0.88, 1.0, r.y)); // sock top, tapering into the boot
return 0.0;
"""

MASK = LIB + r"""
KitLib K;
float3 n = normalize(N);
float surf = K.Surface(P, n, KitA, KitB);
float2 r = K.Classify(P);
if (surf > 4.5 && surf < 5.5) return (r.x < 1.5 || n.z < -0.2 || (n.y > 0.45 && P.z < 173.0)) ? 0.0 : 1.0;
if (surf > 0.5 && surf < 3.5) return 1.0;
if (surf > 6.5 && surf < 7.5) return 1.0;
return 0.0;
"""

# With the MetaHuman head / shirt / shorts on (MHParts = 1) the body hides what they cover: head and neck, the torso,
# the upper arms inside the sleeves (the whole arm for keepers) and the thighs inside the shorts. Cut lines match
# M_PK_PlayerCloth (sleeve end 0.05 + 0.95 * KitA.z along the upper arm, hem 0.33 + 0.35 * KitA.w along the thigh).
BODY_MASK = LIB + r"""
KitLib K;
if (BootMeshes > 0.5)
{
	float2 rb = K.Classify(P);
	// only the bare foot goes: the sock keeps running down into the boot collar (hiding the ankle left the leg
	// floating above an empty boot)
	if (rb.x > 7.5) return 0.0;                                       // the whole foot (the instep peeked out of the throat)
	if (rb.x > 6.5 && P.z < 10.0) return 0.0;                         // ankle inside the boot collar (9.5 poked through; 10.5 showed a torn hem)
}
if (MHParts < 0.5) return 1.0;
float2 r = K.Classify(P);
bool keeper = KitB.z > 0.5;
if (r.x < 2.5) return 0.0;                                         // torso, neck, head (the body's neck flares wider than the face's: no filler)
if (r.x < 3.5) return keeper || r.y < saturate(KitA.z) * 0.95 + 0.05 - 0.06 ? 0.0 : 1.0;   // upper arm
if (r.x < 4.5) return keeper && r.y < 0.95 ? 0.0 : 1.0;          // forearm (keeper sleeves end at 92 %)
if (r.x > 5.5 && r.x < 6.5) return r.y < 0.33 + 0.35 * saturate(KitA.w) - 0.04 ? 0.0 : 1.0; // thigh
return 1.0;
"""

VECTORS = [
    ("Primary", (0.02, 0.12, 0.6, 1)), ("Secondary", (0.01, 0.03, 0.15, 1)), ("Trim", (0.9, 0.9, 0.9, 1)),
    ("Shorts", (0.85, 0.85, 0.85, 1)), ("Socks", (0.02, 0.12, 0.6, 1)), ("Boots", (0.02, 0.02, 0.02, 1)),
    ("NumberColor", (0.9, 0.9, 0.9, 1)), ("Skin", (0.5, 0.32, 0.2, 1)), ("Hair", (0.02, 0.015, 0.01, 1)),
    ("Gloves", (0.9, 0.9, 0.1, 1)), ("KitA", (0, 10, 0.55, 0.45)), ("KitB", (2, 0, 0, 1)),
]
INPUTS = ["P", "N", "Primary", "Secondary", "Trim", "Shorts", "Socks", "Boots", "NumberColor", "Skin", "Hair", "Gloves", "KitA", "KitB"]


def custom(mat, code, out_type, x, y, sources, desc):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out_type)
    node.set_editor_property("description", desc)
    ins = []
    for name in INPUTS:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    for name in INPUTS:
        MEL.connect_material_expressions(sources[name], "", node, name)
    return node


def make(name, shell):
    full = PATH + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    mat = AT.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    if shell:
        mat.set_editor_property("two_sided", True)
    src = {}
    # Pre-skinned position / normal only exist in the vertex shader: interpolate them to the pixel shader.
    raw_p = MEL.create_material_expression(mat, unreal.MaterialExpressionPreSkinnedPosition, -1500, -200)
    raw_n = MEL.create_material_expression(mat, unreal.MaterialExpressionPreSkinnedNormal, -1500, -100)
    interp_p = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -1300, -200)
    interp_n = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -1300, -100)
    for raw, interp in ((raw_p, interp_p), (raw_n, interp_n)):
        if not MEL.connect_material_expressions(raw, "", interp, ""):
            MEL.connect_material_expressions(raw, "", interp, "VS")
    src["P"] = interp_p
    src["N"] = interp_n
    y = 0
    for pname, default in VECTORS:
        v = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1200, y)
        v.set_editor_property("parameter_name", pname)
        v.set_editor_property("default_value", unreal.LinearColor(*default))
        if pname in ("KitA", "KitB"):
            app = MEL.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -950, y)
            MEL.connect_material_expressions(v, "", app, "A")
            MEL.connect_material_expressions(v, "A", app, "B")
            src[pname] = app
        else:
            src[pname] = v
        y += 110
    col = custom(mat, COLOR, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -500, -200, src, "Kit colour")
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = custom(mat, ROUGH, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -500, 100, src, "Kit roughness")
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Face relief and cloth folds: a height field turned into a world-space normal (screen-space derivatives).
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1500, 1200)
    wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    nw = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1500, 1300)
    bump = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 1200)
    bump.set_editor_property("code", BUMP)
    bump.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    bump.set_editor_property("description", "Face relief + cloth folds")
    names = INPUTS + ["WP", "NW"]
    ins = []
    for nm in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    bump.set_editor_property("inputs", ins)
    for nm in INPUTS:
        MEL.connect_material_expressions(src[nm], "", bump, nm)
    MEL.connect_material_expressions(wp, "", bump, "WP")
    MEL.connect_material_expressions(nw, "", bump, "NW")
    mat.set_editor_property("tangent_space_normal", False)
    MEL.connect_material_property(bump, "", unreal.MaterialProperty.MP_NORMAL)
    if not shell:
        mh = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1200, y + 50)
        mh.set_editor_property("parameter_name", "MHParts")
        mh.set_editor_property("default_value", 0.0)
        bm = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1200, y + 150)
        bm.set_editor_property("parameter_name", "BootMeshes")
        bm.set_editor_property("default_value", 0.0)
        node = custom(mat, BODY_MASK, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -500, 400, src, "Body under the cloth")
        ins = list(node.get_editor_property("inputs"))
        for nm in ("MHParts", "BootMeshes"):
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", nm)
            ins.append(ci)
        node.set_editor_property("inputs", ins)
        for nm in INPUTS:
            MEL.connect_material_expressions(src[nm], "", node, nm)
        MEL.connect_material_expressions(mh, "", node, "MHParts")
        MEL.connect_material_expressions(bm, "", node, "BootMeshes")
        MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    if shell:
        mask = custom(mat, MASK, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -500, 400, src, "Cloth mask")
        MEL.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
        vsrc = dict(src)
        vsrc["P"] = raw_p
        vsrc["N"] = raw_n
        off = custom(mat, OFFSET, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -500, 700, vsrc, "Cloth offset (cm)")
        normal_ws = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -500, 900)
        mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 800)
        MEL.connect_material_expressions(off, "", mul, "A")
        MEL.connect_material_expressions(normal_ws, "", mul, "B")
        MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(full)
    unreal.log("PK kit material built: " + full)


make("M_PK_Kit", False)
make("M_PK_KitShell", True)
