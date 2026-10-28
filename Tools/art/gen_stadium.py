"""Generates the stadium meshes as OBJ files (plain Python, no dependencies). Units: cm, Z up, pitch centre at the origin,
pitch length along X. Import with Tools/editor_scripts/import_stadium.py.

The layout numbers are shared with APKPitch (PKPitch.cpp, namespace PKStadium) which places the crowd on the same seat
rows: keep them in step.
"""
import math, os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "meshes")

# ---- shared layout (PKStadium in PKPitch.cpp) ----
HALF_X, HALF_Y = 6150.0, 4300.0      # bowl front (row 0) rounded rectangle half extents
CORNER_R = 1700.0                     # bowl corner radius at the front
ROW_DEPTH, RISE = 80.0, 50.0          # single tier rows
ROWS = 36
FRONT_H = 150.0                       # front wall height (first step starts here)
LOWER_ROWS = 16                       # main stand (far long side, +Y): lower tier rows
BOX_H = 380.0                         # hospitality boxes between the tiers
UPPER_ROWS, UPPER_RISE = 20, 62.0     # main stand upper tier
UPPER_OVERHANG = 420.0                # the upper tier front hangs over the lower tier
ROOF_IN, ROOF_OUT = 250.0, 3100.0     # roof ring between these offsets from the bowl front
ROOF_Z_IN, ROOF_Z_OUT = 3000.0, 3150.0
BACK_D = ROWS * ROW_DEPTH             # back of the single tier


class Mesh:
    def __init__(self):
        self.v, self.vt, self.vn = [], [], []
        self.faces = {}  # material -> list of [(v, vt, vn) x3]

    def tri(self, mat, a, b, c, ua, ub, uc, n=None):
        if n is None:
            e1 = [b[i] - a[i] for i in range(3)]
            e2 = [c[i] - a[i] for i in range(3)]
            n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
            l = math.sqrt(sum(x * x for x in n)) or 1.0
            n = [x / l for x in n]
        idx = []
        for p, u in ((a, ua), (b, ub), (c, uc)):
            self.v.append(p)
            self.vt.append(u)
            self.vn.append(n)
            k = len(self.v)
            idx.append((k, k, k))
        self.faces.setdefault(mat, []).append(idx)

    def quad(self, mat, a, b, c, d, ua=(0, 0), ub=(1, 0), uc=(1, 1), ud=(0, 1), n=None):
        # a b c d counter-clockwise seen from the front
        self.tri(mat, a, b, c, ua, ub, uc, n)
        self.tri(mat, a, c, d, ua, uc, ud, n)

    def quad_out(self, mat, a, b, c, d, out, ua=(0, 0), ub=(1, 0), uc=(1, 1), ud=(0, 1)):
        """Quad whose front face points along `out` (coincident back-to-back copies get merged by the importer, so
        thin parts are built as slabs from outward-facing quads instead)."""
        e1 = [b[i] - a[i] for i in range(3)]
        e2 = [c[i] - a[i] for i in range(3)]
        n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        if sum(n[i] * out[i] for i in range(3)) < 0:
            self.quad(mat, d, c, b, a, ud, uc, ub, ua)
        else:
            self.quad(mat, a, b, c, d, ua, ub, uc, ud)

    def box(self, mat, c, s, uv_scale=100.0):
        x, y, z = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        P = [(x + sx * hx, y + sy * hy, z + sz * hz) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
        def p(i, j, k): return P[i * 4 + j * 2 + k]
        u = lambda a, b: (a / uv_scale, b / uv_scale)
        self.quad(mat, p(1, 0, 0), p(1, 1, 0), p(1, 1, 1), p(1, 0, 1), u(0, 0), u(s[1], 0), u(s[1], s[2]), u(0, s[2]))
        self.quad(mat, p(0, 1, 0), p(0, 0, 0), p(0, 0, 1), p(0, 1, 1), u(0, 0), u(s[1], 0), u(s[1], s[2]), u(0, s[2]))
        self.quad(mat, p(1, 1, 0), p(0, 1, 0), p(0, 1, 1), p(1, 1, 1), u(0, 0), u(s[0], 0), u(s[0], s[2]), u(0, s[2]))
        self.quad(mat, p(0, 0, 0), p(1, 0, 0), p(1, 0, 1), p(0, 0, 1), u(0, 0), u(s[0], 0), u(s[0], s[2]), u(0, s[2]))
        self.quad(mat, p(0, 0, 1), p(1, 0, 1), p(1, 1, 1), p(0, 1, 1), u(0, 0), u(s[0], 0), u(s[0], s[1]), u(0, s[1]))
        self.quad(mat, p(0, 1, 0), p(1, 1, 0), p(1, 0, 0), p(0, 0, 0), u(0, 0), u(s[0], 0), u(s[0], s[1]), u(0, s[1]))

    def tube(self, mat, a, b, r, sides=8):
        d = [b[i] - a[i] for i in range(3)]
        l = math.sqrt(sum(x * x for x in d)) or 1.0
        d = [x / l for x in d]
        up = (0, 0, 1) if abs(d[2]) < 0.9 else (1, 0, 0)
        u = [d[1] * up[2] - d[2] * up[1], d[2] * up[0] - d[0] * up[2], d[0] * up[1] - d[1] * up[0]]
        ul = math.sqrt(sum(x * x for x in u)); u = [x / ul for x in u]
        w = [d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2], d[0] * u[1] - d[1] * u[0]]
        ring = []
        for i in range(sides):
            t = 2 * math.pi * i / sides
            ring.append([math.cos(t) * u[k] + math.sin(t) * w[k] for k in range(3)])
        for i in range(sides):
            n0, n1 = ring[i], ring[(i + 1) % sides]
            p0 = [a[k] + n0[k] * r for k in range(3)]; p1 = [a[k] + n1[k] * r for k in range(3)]
            q0 = [b[k] + n0[k] * r for k in range(3)]; q1 = [b[k] + n1[k] * r for k in range(3)]
            nn = [(n0[k] + n1[k]) * 0.5 for k in range(3)]
            self.quad(mat, p0, p1, q1, q0, (i / sides, 0), ((i + 1) / sides, 0), ((i + 1) / sides, l / 100), (i / sides, l / 100), nn)

    def write(self, name):
        os.makedirs(OUT, exist_ok=True)
        path = os.path.join(OUT, name + ".obj")
        with open(path, "w") as f:
            f.write("# Pitch Kings generated mesh\n")
            # UE's OBJ import mirrors Y (right- to left-handed): pre-mirror positions and normals and reverse the
            # winding, so the asset comes out in the coordinates written here with the same faces in front.
            for p in self.v:
                f.write("v %.3f %.3f %.3f\n" % (p[0], -p[1], p[2]))
            for u in self.vt:
                f.write("vt %.5f %.5f\n" % (u[0], 1.0 - u[1]))  # the importer flips V back
            for n in self.vn:
                f.write("vn %.5f %.5f %.5f\n" % (n[0], -n[1], n[2]))
            for mat, tris in self.faces.items():
                f.write("usemtl %s\n" % mat)
                for t in tris:
                    f.write("f %s\n" % " ".join("%d/%d/%d" % x for x in t[::-1]))
        print("wrote", path, len(self.v) // 3, "tris")


# ---- the bowl path: a rounded rectangle offset outwards by d ----
def loop_point(s, d):
    """Point and outward normal at front arc length s along the bowl loop, offset outwards by d (matches
    PKStadium::Sample in PKPitch.cpp: corners and straights, counter-clockwise from the +X end)."""
    cx, cy = HALF_X - CORNER_R, HALF_Y - CORNER_R
    centres = [(cx, cy), (-cx, cy), (-cx, -cy), (cx, -cy)]
    arc = CORNER_R * math.pi / 2
    lengths = [arc, 2 * cx, arc, 2 * cy, arc, 2 * cx, arc, 2 * cy]
    for seg in range(8):
        if s > lengths[seg] and seg < 7:
            s -= lengths[seg]
            continue
        c = centres[seg // 2]
        if seg % 2 == 0:
            a = math.pi / 2 * (seg // 2) + s / CORNER_R
            n = (math.cos(a), math.sin(a))
            return (c[0] + n[0] * (CORNER_R + d), c[1] + n[1] * (CORNER_R + d)), n
        a = math.pi / 2 * (seg // 2 + 1)
        n = (math.cos(a), math.sin(a))
        t = (-n[1], n[0])
        return (c[0] + n[0] * (CORNER_R + d) + t[0] * s, c[1] + n[1] * (CORNER_R + d) + t[1] * s), n


def front_length():
    return 4 * (HALF_X - CORNER_R) + 4 * (HALF_Y - CORNER_R) + 2 * math.pi * CORNER_R


def bowl_samples(arc_steps=48):  # 48 per corner: 20 left the curved front wall faceted
    """List of (kind, data) along the loop, counter-clockwise from the middle of the +X end: points are evaluated for
    any offset d with point(sample, d). Returns samples with their arc length on the front edge."""
    cx, cy = HALF_X - CORNER_R, HALF_Y - CORNER_R
    out = []
    corners = [(cx, cy, 0.0), (-cx, cy, 90.0), (-cx, -cy, 180.0), (cx, -cy, 270.0)]
    # each side: straight from the end of one corner arc to the start of the next
    for i, (ccx, ccy, a0) in enumerate(corners):
        for k in range(arc_steps + 1):
            a = math.radians(a0 + 90.0 * k / arc_steps)
            out.append(("arc", (ccx, ccy, a)))
    out.append(out[0])  # close the loop
    return out


def point(sample, d, z=0.0):
    kind, (ccx, ccy, a) = sample
    r = CORNER_R + d
    return (ccx + r * math.cos(a), ccy + r * math.sin(a), z)


def normal(sample):
    _, (_, _, a) = sample
    return (math.cos(a), math.sin(a), 0.0)


def loop_lengths(samples):
    s, acc = [0.0], 0.0
    for i in range(1, len(samples)):
        p0, p1 = point(samples[i - 1], 0), point(samples[i], 0)
        acc += math.dist(p0, p1)
        s.append(acc)
    return s


def side_of(sample):
    """+1 main stand (far long side, +Y), -1 near long side, 0 ends / corners."""
    p = point(sample, 0)
    if abs(p[0]) < HALF_X - CORNER_R + 1 and p[1] > 0:
        return 1
    return 0


def strip(m, mat, samples, s_len, d0, z0, d1, z1, v0=0.0, v1=1.0, only=None):
    """Surface between offset/height (d0, z0) and (d1, z1) along the loop. UV: u = length along the front (m), v."""
    for i in range(len(samples) - 1):
        if only is not None and not only(samples[i], samples[i + 1]):
            continue
        a, b = samples[i], samples[i + 1]
        if math.dist(point(a, 0), point(b, 0)) < 1e-3:
            continue
        pa0, pb0 = point(a, d0, z0), point(b, d0, z0)
        pa1, pb1 = point(a, d1, z1), point(b, d1, z1)
        ua, ub = s_len[i] / 100.0, s_len[i + 1] / 100.0
        m.quad(mat, pa0, pa1, pb1, pb0, (ua, v0), (ua, v1), (ub, v1), (ub, v0))


def build_bowl():
    samples = bowl_samples()
    # insert the straight sides: arcs end/start at the same angles, so consecutive corners are joined by a straight
    # quad automatically (point(arc_end) of one corner -> point(arc_start) of the next).
    s_len = loop_lengths(samples)
    m = Mesh()
    main = lambda a, b: side_of(a) == 1 and side_of(b) == 1
    rest = lambda a, b: not main(a, b)

    # front wall (facing the pitch)
    strip(m, "Concrete", samples, s_len, 0, 0, 0, FRONT_H)
    # single-tier terrace everywhere but the main stand: riser then tread per row; v = row index for the material
    for r in range(ROWS):
        d, z = r * ROW_DEPTH, FRONT_H + r * RISE
        strip(m, "Riser", samples, s_len, d, z, d, z + RISE, r, r + 0.5, rest)
        strip(m, "Seats", samples, s_len, d, z + RISE, d + ROW_DEPTH, z + RISE, r, r + 1, rest)
    top = FRONT_H + ROWS * RISE
    # back wall up to the roof
    strip(m, "Cladding", samples, s_len, BACK_D, top, BACK_D, ROOF_Z_OUT + 40, 0, 1, rest)
    strip(m, "Facade", samples, s_len, BACK_D + 40, ROOF_Z_OUT + 40, BACK_D + 40, 0, 0, 1, rest)  # outer face

    # main stand: lower tier, hospitality boxes, upper tier
    for r in range(LOWER_ROWS):
        d, z = r * ROW_DEPTH, FRONT_H + r * RISE
        strip(m, "Riser", samples, s_len, d, z, d, z + RISE, r, r + 0.5, main)
        strip(m, "Seats", samples, s_len, d, z + RISE, d + ROW_DEPTH, z + RISE, r, r + 1, main)
    lower_top = FRONT_H + LOWER_ROWS * RISE
    box_d = LOWER_ROWS * ROW_DEPTH
    strip(m, "Glass", samples, s_len, box_d, lower_top, box_d, lower_top + BOX_H, 0, 1, main)
    up_d0 = box_d - UPPER_OVERHANG
    up_z0 = lower_top + BOX_H + 60
    # upper tier front fascia (hangs over the boxes) and its soffit
    strip(m, "Ribbon", samples, s_len, up_d0, up_z0 - 170, up_d0, up_z0 + 60, 0, 1, main)  # LED ribbon
    strip(m, "Concrete", samples, s_len, box_d, up_z0 - 170, up_d0, up_z0 - 170, 0, 1, main)
    for r in range(UPPER_ROWS):
        d, z = up_d0 + r * ROW_DEPTH, up_z0 + 60 + r * UPPER_RISE
        strip(m, "Riser", samples, s_len, d, z, d, z + UPPER_RISE, 40 + r, 40 + r + 0.5, main)
        strip(m, "Seats", samples, s_len, d, z + UPPER_RISE, d + ROW_DEPTH, z + UPPER_RISE, 40 + r, 40 + r + 1, main)
    up_back = up_d0 + UPPER_ROWS * ROW_DEPTH
    up_top = up_z0 + 60 + UPPER_ROWS * UPPER_RISE
    strip(m, "Cladding", samples, s_len, up_back, up_top, up_back, ROOF_Z_OUT + 40, 0, 1, main)
    strip(m, "Facade", samples, s_len, up_back + 40, ROOF_Z_OUT + 40, up_back + 40, 0, 0, 1, main)
    # End walls where the two-tier main stand meets the single-tier corners.
    cx = HALF_X - CORNER_R
    rest_z = lambda d: FRONT_H + min(ROWS, d // ROW_DEPTH + 1) * RISE
    roof_z = lambda d: ROOF_Z_IN + (d - ROOF_IN) / (ROOF_OUT - ROOF_IN) * (ROOF_Z_OUT - ROOF_Z_IN)
    # One solid slab per side (boxes render reliably from every side; the corner terraces run into it).
    z0, z1 = rest_z(up_d0), roof_z(up_back + 40)
    for s in (-1, 1):
        # glazed cladding, like the hospitality level (bare concrete read as an unfinished blank slab)
        # plain concrete: the facade's fine window grid shimmered on these narrow blocks
        m.box("Concrete", (s * cx, HALF_Y + (up_d0 + up_back + 40) / 2, (z0 + z1) / 2), (30.0, up_back + 40 - up_d0, z1 - z0), 100)
    m.write("SM_PK_Bowl")


def build_roof():
    samples = bowl_samples()
    s_len = loop_lengths(samples)
    m = Mesh()
    # underside (seen from the pitch), top, front fascia and the floodlight strip under the front edge
    strip(m, "RoofUnder", samples, s_len, ROOF_OUT, ROOF_Z_OUT, ROOF_IN, ROOF_Z_IN, 0, 1)
    strip(m, "RoofTop", samples, s_len, ROOF_IN, ROOF_Z_IN + 60, ROOF_OUT, ROOF_Z_OUT + 60, 0, 1)
    # LED ribbon all round the roof edge, 6 cm proud of the roof edge (flush, it shared edges with the roof panels and
    # tore into thin slits) and a little taller so it overlaps them
    strip(m, "Ribbon", samples, s_len, ROOF_IN - 6, ROOF_Z_IN - 130, ROOF_IN - 6, ROOF_Z_IN + 70, 0, 1)
    strip(m, "Lights", samples, s_len, ROOF_IN + 140, ROOF_Z_IN - 100, ROOF_IN, ROOF_Z_IN - 120, 0, 1)
    # Cantilever girders under the roof every 8 m (deep at the back, tapering to the front edge) and a hanging truss
    # girder along the front edge: the structural rhythm seen from the pitch.
    roof_z = lambda d: ROOF_Z_IN + (d - ROOF_IN) / (ROOF_OUT - ROOF_IN) * (ROOF_Z_OUT - ROOF_Z_IN)
    total = front_length()
    s0 = 0.0
    while s0 < total - 1:
        P0, N0 = loop_point(s0, 0.0)
        T0 = (-N0[1], N0[0])
        prof = [ROOF_IN + 60, ROOF_IN + 900, ROOF_OUT - 100]
        depth = [55.0, 110.0, 150.0]
        for k in range(len(prof) - 1):
            da, db = prof[k], prof[k + 1]
            ha, hb = depth[k], depth[k + 1]
            A, _ = loop_point(s0, da)
            B, _ = loop_point(s0, db)
            for side in (-1, 1):
                o = (T0[0] * 14 * side, T0[1] * 14 * side)
                a_top = (A[0] + o[0], A[1] + o[1], roof_z(da) - 4)
                b_top = (B[0] + o[0], B[1] + o[1], roof_z(db) - 4)
                a_bot = (A[0] + o[0], A[1] + o[1], roof_z(da) - 4 - ha)
                b_bot = (B[0] + o[0], B[1] + o[1], roof_z(db) - 4 - hb)
                m.quad_out("Truss", a_bot, b_bot, b_top, a_top, (T0[0] * side, T0[1] * side, 0))
            # underside of the girder
            oa = (T0[0] * 14, T0[1] * 14)
            m.quad_out("Truss", (A[0] - oa[0], A[1] - oa[1], roof_z(da) - 4 - ha), (A[0] + oa[0], A[1] + oa[1], roof_z(da) - 4 - ha),
                       (B[0] + oa[0], B[1] + oa[1], roof_z(db) - 4 - hb), (B[0] - oa[0], B[1] - oa[1], roof_z(db) - 4 - hb), (0, 0, -1))
        s0 += 800.0
    # front truss: two chords and zig-zag diagonals, hung under the front of the roof
    d_t = ROOF_IN + 220
    z_top, z_bot = ROOF_Z_IN - 20, ROOF_Z_IN - 260
    step = 300.0
    s0 = 0.0
    prev = None
    while s0 <= total + 1:
        P, _ = loop_point(min(s0, total), d_t)
        top = (P[0], P[1], z_top)
        bot = (P[0], P[1], z_bot)
        if prev is not None:
            m.tube("Truss", prev[0], top, 9, 6)
            m.tube("Truss", prev[1], bot, 9, 6)
            m.tube("Truss", prev[0], bot, 5, 5)
        m.tube("Truss", top, bot, 5, 5)
        prev = (top, bot)
        s0 += step
    m.write("SM_PK_Roof")


def build_goal():
    """Goal at the origin, mouth on the YZ plane facing -X (the pitch side is -X), net behind towards +X."""
    m = Mesh()
    hw, h, r = 366.0, 244.0, 6.0  # post and bar centres (PK::GoalHalfWidth, PK::GoalHeight)
    depth_top, depth_bottom = 90.0, 210.0
    # frame
    m.tube("Frame", (0, -hw, 0), (0, -hw, h + r), r, 16)
    m.tube("Frame", (0, hw, 0), (0, hw, h + r), r, 16)
    m.tube("Frame", (0, -hw - r, h), (0, hw + r, h), r, 16)
    # back supports (thin, grey)
    for s in (-1, 1):
        m.tube("Support", (0, s * hw, h), (depth_top, s * hw, h), 2.5, 8)
        m.tube("Support", (depth_top, s * hw, h), (depth_bottom, s * hw, 0), 2.5, 8)
        m.tube("Support", (0, s * hw, 0), (depth_bottom, s * hw, 0), 2.5, 8)
    m.tube("Support", (depth_bottom, -hw, 2), (depth_bottom, hw, 2), 2.5, 8)
    # net: roof, back, two sides (double-sided material); UVs in metres so the mesh pattern has a fixed size
    # Roof and back of the net as subdivided grids that sag between the supports (UV in metres along the cloth).
    def grid(corner, eu, ev, nu, nv, sag, uvf):
        pts = {}
        for i in range(nu + 1):
            for j in range(nv + 1):
                fu, fv = i / nu, j / nv
                p = [corner[k] + eu[k] * fu + ev[k] * fv for k in range(3)]
                bump = math.sin(math.pi * fu) * math.sin(math.pi * fv)
                p = [p[k] + sag[k] * bump for k in range(3)]
                pts[(i, j)] = p
        for i in range(nu):
            for j in range(nv):
                a, b, c, d = pts[(i, j)], pts[(i + 1, j)], pts[(i + 1, j + 1)], pts[(i, j + 1)]
                m.quad("Net", a, b, c, d, uvf(i / nu, j / nv), uvf((i + 1) / nu, j / nv), uvf((i + 1) / nu, (j + 1) / nv), uvf(i / nu, (j + 1) / nv))
    W2 = 2 * hw
    roof_len = depth_top - 2
    grid((2, -hw, h - 2), (0, W2, 0), (roof_len, 0, 0), 16, 4, (0, 0, -14), lambda u, v: (u * W2 / 100, v * roof_len / 100))
    slope = math.hypot(depth_bottom - depth_top, h - 3)
    grid((depth_top, -hw, h - 2), (0, W2, 0), (depth_bottom - depth_top, 0, -(h - 3)), 16, 6, (12, 0, 0), lambda u, v: (u * W2 / 100, v * slope / 100))
    for s in (-1, 1):
        a, b, c, d = (2, s * hw, 1), (depth_bottom, s * hw, 1), (depth_top, s * hw, h - 2), (2, s * hw, h - 2)
        uv = lambda p: (p[0] / 100.0, p[2] / 100.0)
        m.tri("Net", a, b, c, uv(a), uv(b), uv(c))
        m.tri("Net", a, c, d, uv(a), uv(c), uv(d))
    m.write("SM_PK_Goal")


def build_board():
    """LED advertising board, 600 x 90 cm screen facing -Y, tilted back 10 deg; pivot at the bottom centre."""
    m = Mesh()
    L, H, T = 600.0, 92.0, 12.0
    tilt = math.radians(10)
    dy, dz = math.sin(tilt) * H, math.cos(tilt) * H
    a, b = (-L / 2, 0, 4), (L / 2, 0, 4)
    c, d = (L / 2, dy, 4 + dz), (-L / 2, dy, 4 + dz)
    m.quad("Screen", a, b, c, d, (0, 1), (1, 1), (1, 0), (0, 0))
    # back panel / frame
    m.quad("Frame", (L / 2, dy + T, 4 + dz), (L / 2, T + 60, 0), (-L / 2, T + 60, 0), (-L / 2, dy + T, 4 + dz))
    m.quad("Frame", (-L / 2, 0, 0), (L / 2, 0, 0), (L / 2, 0, 4), (-L / 2, 0, 4))
    m.quad("Frame", d, c, (L / 2, dy + T, 4 + dz), (-L / 2, dy + T, 4 + dz))
    for s in (-1, 1):
        x = s * L / 2
        p = [(x, 0, 0), (x, T + 60, 0), (x, dy + T, 4 + dz), (x, dy, 4 + dz)]
        if s < 0:
            p = p[::-1]
        m.quad("Frame", *p)
    m.write("SM_PK_Board")


def build_card():
    """Crowd card: a 60 x 100 cm quad standing on its bottom edge, facing -Y (towards the pitch once placed)."""
    m = Mesh()
    w, h = 60.0, 100.0
    m.quad("Crowd", (-w / 2, 0, 0), (w / 2, 0, 0), (w / 2, 0, h), (-w / 2, 0, h), (0, 1), (1, 1), (1, 0), (0, 0), (0, -1, 0))
    m.write("SM_PK_CrowdCard")


def build_flag():
    m = Mesh()
    m.tube("Pole", (0, 0, 0), (0, 0, 160), 1.1, 8)
    m.quad("Flag", (0, 0, 120), (42, 0, 120), (42, 0, 158), (0, 0, 158), (0, 1), (1, 1), (1, 0), (0, 0))
    m.quad("Flag", (42, 0, 120), (0, 0, 120), (0, 0, 158), (42, 0, 158), (1, 1), (0, 1), (0, 0), (1, 0))
    m.write("SM_PK_CornerFlag")


def build_dugout():
    """Dugout, 8 m long along X, pivot at the front bottom centre; the open side faces -Y (place it facing the pitch)."""
    m = Mesh()
    L, D, H = 800.0, 200.0, 220.0
    steps = 10
    # Canopy: a quarter ellipse from the ground at the back (y = D) over to the top front edge (y = 0, z = H).
    pts = [(D * math.cos(math.radians(90.0 * i / steps)), H * math.sin(math.radians(90.0 * i / steps))) for i in range(steps + 1)]
    for i in range(steps):
        (y0, z0), (y1, z1) = pts[i], pts[i + 1]
        m.quad("Perspex", (-L / 2, y1, z1), (L / 2, y1, z1), (L / 2, y0, z0), (-L / 2, y0, z0))
        m.quad("Perspex", (-L / 2, y0, z0), (L / 2, y0, z0), (L / 2, y1, z1), (-L / 2, y1, z1))
    # End panels (fans from the front bottom corner) and a frame rib along the front edge.
    for s in (-1, 1):
        x = s * L / 2
        for i in range(steps):
            (y0, z0), (y1, z1) = pts[i], pts[i + 1]
            m.tri("Frame", (x, 0, 0), (x, y0, z0), (x, y1, z1), (0, 0), (y0 / 100, z0 / 100), (y1 / 100, z1 / 100))
            m.tri("Frame", (x, 0, 0), (x, y1, z1), (x, y0, z0), (0, 0), (y1 / 100, z1 / 100), (y0 / 100, z0 / 100))
    m.tube("Frame", (-L / 2, 0, H), (L / 2, 0, H), 5, 8)
    # Bench seats facing the pitch.
    for k in range(10):
        x = -L / 2 + 60 + k * 75
        m.box("Seat", (x, D - 75, 45), (58, 48, 8), 100)
        m.box("Seat", (x, D - 50, 75), (58, 6, 55), 100)
    m.box("Floor", (0, D / 2, 2), (L, D, 4), 100)
    m.write("SM_PK_Dugout")


def build_ring():
    """Selection ring for the controlled player: flat annulus (r 46..54 cm) lying on the ground, facing up."""
    m = Mesh()
    steps = 48
    for i in range(steps):
        a0, a1 = 2 * math.pi * i / steps, 2 * math.pi * (i + 1) / steps
        p = lambda r, a: (r * math.cos(a), r * math.sin(a), 0.0)
        m.quad("Glow", p(46, a0), p(54, a0), p(54, a1), p(46, a1), (0, 0), (1, 0), (1, 1), (0, 1), (0, 0, 1))
    m.write("SM_PK_Ring")


def build_chevron():
    """Downward chevron shown above the controlled player's head: 24 cm wide, 4 cm thick, tip at the origin."""
    m = Mesh()
    outline = [(0, 0), (12, 12), (12, 18), (0, 7), (-12, 18), (-12, 12)]
    t = 2.0
    for y, n in ((-t, (0, -1, 0)), (t, (0, 1, 0))):
        pts = [(x, y, z) for x, z in outline]
        for i in range(1, len(pts) - 1):
            tri = (pts[0], pts[i + 1], pts[i]) if y > 0 else (pts[0], pts[i], pts[i + 1])  # outline runs CCW about -Y
            m.tri("Glow", *tri, (0, 0), (1, 0), (1, 1), n)
    for i in range(len(outline)):
        (x0, z0), (x1, z1) = outline[i], outline[(i + 1) % len(outline)]
        m.quad("Glow", (x0, t, z0), (x1, t, z1), (x1, -t, z1), (x0, -t, z0))
    m.write("SM_PK_Chevron")


def build_tree():
    """Park tree, ~9 m: trunk and a lumpy ellipsoid canopy (pivot at the base)."""
    m = Mesh()
    m.tube("Bark", (0, 0, 0), (0, 0, 420), 22, 8)
    rings, segs = 7, 12
    cz, rx, rz = 600.0, 330.0, 300.0
    def pt(i, j):
        th = math.pi * i / rings
        ph = 2 * math.pi * j / segs
        bump = 1.0 + 0.12 * math.sin(3 * ph + i) * math.sin(th * 2)
        return (rx * bump * math.sin(th) * math.cos(ph), rx * bump * math.sin(th) * math.sin(ph), cz + rz * math.cos(th))
    for i in range(rings):
        for j in range(segs):
            a, b, c, d = pt(i, j), pt(i + 1, j), pt(i + 1, j + 1), pt(i, j + 1)
            m.quad("Foliage", a, b, c, d)
    m.write("SM_PK_Tree")


def build_screen():
    """Big video screen, 12 x 6.75 m, hung from its top edge (pivot at the top centre), facing -Y."""
    m = Mesh()
    W, H, T = 1200.0, 675.0, 60.0
    m.quad("BigScreen", (-W / 2, 0, -H), (W / 2, 0, -H), (W / 2, 0, 0), (-W / 2, 0, 0), (0, 1), (1, 1), (1, 0), (0, 0))
    m.box("ScreenFrame", (0, T / 2 + 1, -H / 2), (W + 40, T, H + 40), 100)
    for s in (-1, 1):
        m.tube("ScreenFrame", (s * W * 0.35, T / 2, 0), (s * W * 0.35, T / 2, 400), 8, 6)  # hangers up to the truss
    m.write("SM_PK_Screen")


def build_car():
    """Parked car, 4.5 m long along Y, pivot on the ground."""
    m = Mesh()
    m.box("Car", (0, 0, 55), (180, 450, 70), 100)
    m.box("Car", (0, -20, 115), (160, 240, 50), 100)
    m.write("SM_PK_Car")


def build_banner():
    """Supporters' banner, 10 x 1.3 m, hanging flat facing -Y, pivot at the bottom centre."""
    m = Mesh()
    L, H = 1000.0, 130.0
    m.quad("Banner", (-L / 2, 0, 0), (L / 2, 0, 0), (L / 2, 0, H), (-L / 2, 0, H), (0, 1), (1, 1), (1, 0), (0, 0))
    m.write("SM_PK_Banner")


if __name__ == "__main__":
    build_bowl()
    build_roof()
    build_goal()
    build_board()
    build_card()
    build_flag()
    build_dugout()
    build_ring()
    build_chevron()
    build_tree()
    build_screen()
    build_car()
    build_banner()
