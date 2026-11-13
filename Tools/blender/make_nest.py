"""Leafline: nest mound generator (Blender 5.x).

Run inside Blender (Scripting tab, or exec() it).  It clears the scene, builds the nest as ONE mesh
object, renders two previews, saves Art/Blender/Nest.blend and exports Art/Blender/SM_Nest.fbx.
Everything is procedural; no textures.

Units: Blender metres, like SM_Ant, so the default FBX export imports at real size in Unreal cm
(the mound is 2.40 m across and 7 cm high).  All maths below is in cm and converted on output.
Origin: the mound centre on the ground (Z = 0).  +X is the actor's forward (toward the trunk).

The mound surface is the walking surface of Source/AntTreeGame/Tree/TreeField.cpp GroundDistance:
a half ellipsoid (radius MOUND_R, height MOUND_H) smooth-unioned with the ground plane (k = BLEND_K),
solved per radius from the same formulas.  Ants walk on that field, so the mesh follows it to about a
millimetre; everything added on top (pellets, clumps, leaf scraps) is low and visual only.  The match is
exact only at 120 / 7: the 4 cm blend does not scale with the actor's mound scale.

Material slots: 0 soil (M_NestSoil), 1 leaf scraps (M_Leaf).
Vertex colour "Col" (data, exported LINEAR; see Art/Blender/README.md):
  soil  R soil cover (1 inside a wandering radius of 105..122 cm, linear to 0 at the skirt's outline;
        1 on pellets/clumps)
        G entrance (1 inside 0.45 x the entrance radius, linear to 0 at 1.05 x; pellets carry their centre's)
        B 0 on the mound surface, 1 on pellets and clumps (hard mask)
        A mound surface: loose spoil near pellets/clumps 0..1; pellets/clumps: per-clump random 0..1
  leaf  R 1 (fresh), G per-scrap random, B 0 (ground level), A 0 (plain richness)
UV0: soil = planar position (x / 240 + 0.5, y / 240 + 0.5); scraps = each scrap's bounding box 0..1,
U along its long axis, V across.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector, geometry

# ---------------------------------------------------------------- parameters (cm)
MOUND_R = 120.0       # FTreeParams::NestMoundRadius
MOUND_H = 7.0         # FTreeParams::NestMoundHeight
BLEND_K = 4.0         # SMin k in GroundDistance
SKIRT_LIFT = 0.15     # the skirt past the mound rides this far above the flat ground disk (no z-fighting)
TUCK = -0.3           # outline vertices sit this far below the ground, so the edge is a clean intersection
HOLE_R = 16.0         # mean entrance radius (today's disc was 40); the ant's feet reach about 9 cm from its centre
FUNNEL_DEG = 28.0     # vertex normals inside the entrance lean toward the centre (shading only)
SEED = 1113
RENDER_PREVIEWS = True
EXPORT = True
CM = 0.01             # cm -> Blender metres

try:
    _ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
except NameError:  # executed via exec() without __file__
    _ROOT = "D:/UnrealProjects/AntTreeGame"
OUT_DIR = os.path.join(_ROOT, "Art", "Blender")
EXTRA_SHOT = None     # optional path for a close-up render with the ant (not part of the deliverable)

rng = random.Random(SEED)
TAU = 2.0 * math.pi


# ---------------------------------------------------------------- the walking field (TreeField.cpp)
def smin(a, b, k):
    h = max(k - abs(a - b), 0.0) / k
    return min(a, b) - h * h * k * 0.25


def field(r, z):
    k0 = math.sqrt((r / MOUND_R) ** 2 + (z / MOUND_H) ** 2)
    k1 = math.sqrt((r / MOUND_R ** 2) ** 2 + (z / MOUND_H ** 2) ** 2)
    mound = k0 * (k0 - 1.0) / k1 if k1 > 1e-9 else -MOUND_H
    return smin(z, mound, BLEND_K)


def solve_surface(r):
    lo, hi = -2.0, MOUND_H + 3.0
    for _ in range(50):
        mid = 0.5 * (lo + hi)
        if field(r, mid) > 0.0:
            hi = mid
        else:
            lo = mid
    return 0.5 * (lo + hi)


TABLE_STEP = 0.02
TABLE_MAX = MOUND_R + BLEND_K + 2.0
_table = [solve_surface(i * TABLE_STEP) for i in range(int(TABLE_MAX / TABLE_STEP) + 2)]


def surf(r):
    """Field surface height (cm) at radius r."""
    if r >= TABLE_MAX:
        return 0.0
    f = r / TABLE_STEP
    i = int(f)
    t = f - i
    return _table[i] * (1.0 - t) + _table[i + 1] * t


# ---------------------------------------------------------------- outlines
def periodic(k_lo, k_hi, falloff):
    terms = [(k, rng.uniform(0, TAU), 1.0 / (k ** falloff)) for k in range(k_lo, k_hi + 1)]
    norm = sum(a for _, _, a in terms)
    return lambda th: sum(a * math.sin(k * th + p) for k, p, a in terms) / norm


_hole_n = periodic(2, 8, 0.9)


def hole_r(th):
    return HOLE_R * (1.0 + 0.2 * _hole_n(th))


def entrance(x, y):
    """G: 1 inside 0.45 x the entrance radius, linear to 0 at 1.05 x (the shader shapes it)."""
    r = math.hypot(x, y)
    u = r / hole_r(math.atan2(y, x))
    return min(1.0, max(0.0, (1.05 - u) / 0.6)), u


# Skirt outline: a wobbly circle just past the blend, with lobes where spoil was dumped.
_edge_n = periodic(3, 14, 0.7)
LOBES = []
while len(LOBES) < 11:
    th = rng.uniform(0, TAU)
    if all(abs((th - t + math.pi) % TAU - math.pi) > 0.38 for t, _, _ in LOBES):
        # favour the near side (-X, where the day-start camera stands) a little
        ext = rng.uniform(7.0, 26.0) * (1.0 + 0.3 * max(0.0, -math.cos(th)))
        LOBES.append((th, ext, rng.uniform(0.08, 0.22)))


def r_out(th):
    r = 128.5 + 3.0 * _edge_n(th)
    for t, ext, w in LOBES:
        d = (th - t + math.pi) % TAU - math.pi
        r += ext * math.exp(-(d / w) ** 2)
    return max(r, 126.0)


# Soil cover fades from r_in to the outline. r_in wanders and has a few bays, so the soil's edge does not
# coincide with the crease of the field's blend (r ~ 122) and the circle breaks up in colour too.
_in_n = periodic(2, 11, 0.8)
BAYS = [(rng.uniform(0, TAU), rng.uniform(5.0, 12.0), rng.uniform(0.12, 0.3)) for _ in range(4)]


def r_in(th):
    r = 119.0 + 3.0 * _in_n(th)
    for t, depth, w in BAYS:
        d = (th - t + math.pi) % TAU - math.pi
        r -= depth * math.exp(-(d / w) ** 2)
    return min(r, r_out(th) - 5.0)


def cover(x, y):
    """R: 1 inside r_in, linear to 0 at the outline."""
    r = math.hypot(x, y)
    th = math.atan2(y, x)
    ri = r_in(th)
    if r <= ri:
        return 1.0
    return max(0.0, min(1.0, 1.0 - (r - ri) / max(r_out(th) - ri, 0.5)))


SMOOTH_W = 2.5


def smooth01(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def base_height(r):
    return max(surf(r), SKIRT_LIFT)


def ground_at(x, y):
    """Height of the visible base surface: the field on the mound, the skirt lift past it, 0 outside."""
    r = math.hypot(x, y)
    if r < r_out(math.atan2(y, x)) - 0.9:
        return max(surf(r), SKIRT_LIFT)
    return 0.0


# ---------------------------------------------------------------- placement (positions only)
pellets = []   # dicts: x, y, ax, ay, yaw, h, seg, rings, rnd
scraps = []    # dicts: x, y, rad, yaw, curl, rnd


def free(x, y, rad, k=0.85, scrap_pad=0.6):
    for p in pellets:
        if (p["x"] - x) ** 2 + (p["y"] - y) ** 2 < (k * (p["ax"] + rad)) ** 2:
            return False
    for s in scraps:
        if (s["x"] - x) ** 2 + (s["y"] - y) ** 2 < (s["rad"] * 1.2 + rad + scrap_pad) ** 2:
            return False
    return True


def add_pellet(x, y, rad, aspect, h, seg, rings):
    pellets.append(dict(x=x, y=y, ax=rad, ay=rad * aspect, yaw=rng.uniform(0, TAU), h=h, seg=seg,
                        rings=rings, rnd=rng.random(), lump=(rng.uniform(0, TAU), rng.uniform(0, TAU))))


def place():
    # Lip: dug pellets round the entrance, dense at the edge of the hole, thinning outward.
    # Overlapping, so they read as one lumpy ring of spoil rather than dots.
    tries = 0
    lip = 0
    while lip < 330 and tries < 30000:
        tries += 1
        th = rng.uniform(0, TAU)
        if rng.random() < 0.75:
            d = rng.triangular(-0.6, 6.5, 1.5)   # the dense ring at the edge of the hole
            k = 0.5
        else:
            d = rng.uniform(6.5, 16.0)           # sparser beyond it
            k = 0.85
        r = hole_r(th) * 0.97 + d
        rad = rng.uniform(0.6, 1.2) * (1.0 - 0.3 * max(d, 0.0) / 16.0)
        x, y = r * math.cos(th), r * math.sin(th)
        if free(x, y, rad, k=k):
            add_pellet(x, y, rad, rng.uniform(0.7, 1.0), min(0.6, rad * rng.uniform(0.45, 0.6)), 6, (0.6, 1.0))
            lip += 1
    # Leaf scraps: what the colony carries in. (r, angle from +X in degrees); most on the camera side.
    for r, deg in ((31, 200), (54, 152), (77, 236), (97, 176), (63, 292), (86, 40), (138, 206), (150, 128), (143, 254)):
        for _ in range(40):
            rr = r + rng.uniform(-4, 4)
            th = math.radians(deg + rng.uniform(-8, 8))
            rad = rng.uniform(1.8, 3.0)
            x, y = rr * math.cos(th), rr * math.sin(th)
            if free(x, y, rad * 1.2, k=1.0):
                scraps.append(dict(x=x, y=y, rad=rad, yaw=rng.uniform(0, TAU), curl=rng.uniform(0.15, 0.32),
                                   rnd=rng.random(), phase=rng.uniform(0, TAU)))
                break
    # Spoil spread over the mound beyond the lip, more toward the near side.
    n, tries = 0, 0
    while n < 70 and tries < 20000:
        tries += 1
        th = rng.uniform(0, TAU)
        if rng.random() > 0.55 + 0.45 * max(0.0, -math.cos(th)):
            continue
        r = hole_r(th) + 11.0 + 34.0 * rng.random() ** 1.3
        rad = rng.uniform(0.45, 0.9)
        x, y = r * math.cos(th), r * math.sin(th)
        if free(x, y, rad, k=1.1):
            add_pellet(x, y, rad, rng.uniform(0.65, 1.0), min(0.45, rad * rng.uniform(0.4, 0.55)), 6, (0.6, 1.0))
            n += 1
    # Edge clumps: clusters at the lobes and a few more, breaking the circle; crumbs round them.
    # A clump is a pile of overlapping lumps in an elongated patch, not one smooth dome (that read as a pebble).
    angles = [t for t, _, _ in LOBES] + [rng.uniform(0, TAU) for _ in range(6)]
    for th0 in angles:
        ro = r_out(th0)
        rc = rng.uniform(117.0, max(118.0, min(ro - 4.0, 142.0)))
        cx, cy = rc * math.cos(th0), rc * math.sin(th0)
        along = th0 + math.pi / 2 + rng.uniform(-0.6, 0.6)
        ca, sa = math.cos(along), math.sin(along)
        half_l, half_w = rng.uniform(4.0, 9.0), rng.uniform(2.0, 4.0)
        for _ in range(rng.randint(8, 18)):
            for _try in range(30):
                u, v = rng.uniform(-1, 1), rng.uniform(-1, 1)
                if u * u + v * v > 1.0:
                    continue
                x, y = cx + ca * u * half_l - sa * v * half_w, cy + sa * u * half_l + ca * v * half_w
                rad = rng.uniform(0.7, 1.8) * (1.0 - 0.35 * (u * u + v * v))
                if free(x, y, rad, k=0.55):
                    seg, rings = (8, (0.45, 0.8, 1.0)) if rad > 1.2 else (6, (0.6, 1.0))
                    add_pellet(x, y, rad, rng.uniform(0.6, 1.0), min(0.8, rad * rng.uniform(0.38, 0.5)), seg, rings)
                    break
        for _ in range(rng.randint(6, 14)):
            for _try in range(20):
                x, y = cx + rng.uniform(-14, 14), cy + rng.uniform(-14, 14)
                rad = rng.uniform(0.35, 0.8)
                if free(x, y, rad, k=1.0):
                    add_pellet(x, y, rad, rng.uniform(0.6, 1.0), min(0.45, rad * rng.uniform(0.45, 0.6)), 6, (0.6, 1.0))
                    break
    # Loose crumbs along the whole rim, mostly on the soil.
    n, tries = 0, 0
    while n < 90 and tries < 20000:
        tries += 1
        th = rng.uniform(0, TAU)
        r = rng.uniform(110.0, 142.0)
        if r > r_out(th) + 6.0:
            continue
        rad = rng.uniform(0.35, 0.75)
        x, y = r * math.cos(th), r * math.sin(th)
        if free(x, y, rad, k=1.0):
            add_pellet(x, y, rad, rng.uniform(0.6, 1.0), min(0.4, rad * rng.uniform(0.45, 0.6)), 6, (0.6, 1.0))
            n += 1


# ---------------------------------------------------------------- base surface points
def ring_step(r):
    """(radial step, arc spacing) in cm of the jittered rings."""
    def lerp(a, b, t):
        t = min(1.0, max(0.0, t))
        return a + (b - a) * t
    if r < 28.0:
        return 0.95, 0.95
    if r < 45.0:
        s = lerp(0.95, 3.6, (r - 28.0) / 17.0)
        return s, s
    if r < 100.0:
        return 3.6, 3.6
    return lerp(3.6, 0.6, (r - 100.0) / 19.0), lerp(3.6, 3.2, (r - 100.0) / 19.0)


BAND = [119.0 + 0.5 * i for i in range(4)] + [121.0 + 0.25 * i for i in range(10)] + [123.5, 124.0, 124.5]
BAND_N = 180
SKIRT_S = 2.5


def base_points():
    pts = [(0.0, 0.0)]
    r = 0.0
    while True:
        dr, da = ring_step(r)
        r += dr
        if r >= BAND[0] - 0.3:
            break
        n = max(6, int(round(TAU * r / da)))
        off = rng.uniform(0, TAU)
        for i in range(n):
            a = off + TAU * (i + rng.uniform(-0.2, 0.2)) / n
            rr = r + rng.uniform(-0.2, 0.2) * dr
            pts.append((rr * math.cos(a), rr * math.sin(a)))
    for rb in BAND:  # aligned rings across the steep part of the blend: triangles span one step only
        for i in range(BAND_N):
            a = TAU * i / BAND_N
            if rb < r_out(a) - 1.5:
                pts.append((rb * math.cos(a), rb * math.sin(a)))
    r = BAND[-1] + SKIRT_S * 0.7
    rmax = max(r_out(TAU * i / 720) for i in range(720))
    while r < rmax:
        n = int(round(TAU * r / SKIRT_S))
        off = rng.uniform(0, TAU)
        for i in range(n):
            a = off + TAU * (i + rng.uniform(-0.25, 0.25)) / n
            rr = r + rng.uniform(-0.25, 0.25) * SKIRT_S
            if rr < r_out(a) - 1.8:
                pts.append((rr * math.cos(a), rr * math.sin(a)))
        r += SKIRT_S
    # outline (tucked under the ground) and an inset ring at the skirt height
    outline = []
    th, k = 0.0, 0
    while th < TAU - 1e-6:
        outline.append(th)
        th += 1.4 / r_out(th)
    for th in outline:
        ri = r_out(th) - 0.9
        pts.append((ri * math.cos(th), ri * math.sin(th)))
    first_out = len(pts)
    for th in outline:
        ro = r_out(th)
        pts.append((ro * math.cos(th), ro * math.sin(th)))
    return pts, first_out


# ---------------------------------------------------------------- mesh building
bm = None
attrs = {}   # BMVert -> (col4, uv0)


def V(x, y, z, col, uv):
    v = bm.verts.new((x * CM, y * CM, z * CM))
    attrs[v] = (col, uv)
    return v


def face(vs):
    try:
        return bm.faces.new(vs)
    except ValueError:
        return None


def build_base(spoil_grid):
    pts, first_out = base_points()
    vin = [Vector((x, y)) for x, y in pts]
    outline_face = list(range(first_out, len(pts)))
    out = geometry.delaunay_2d_cdt(vin, [], [outline_face], 1, 1e-4, True)
    verts2, _edges, faces, orig_verts = out[0], out[1], out[2], out[3]
    bverts, uvals, tucked = [], [], []
    for i, p in enumerate(verts2):
        x, y = p.x, p.y
        is_out = any(o >= first_out for o in orig_verts[i])
        r = math.hypot(x, y)
        z = TUCK if is_out else max(surf(r), SKIRT_LIFT)
        g, u = entrance(x, y)
        col = (cover(x, y) if not is_out else 0.0, g, 0.0, spoil(spoil_grid, x, y))
        bverts.append(V(x, y, z, col, (x / 240.0 + 0.5, y / 240.0 + 0.5)))
        uvals.append(u)
        tucked.append(is_out)
    tris = []
    for f in faces:
        a, b, c = f[0], f[1], f[2]
        pa, pb, pc = verts2[a], verts2[b], verts2[c]
        if (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x) < 0:
            b, c = c, b
        fc = face((bverts[a], bverts[b], bverts[c]))
        if fc:
            fc.material_index = 0
            tris.append((a, b, c))
    return bverts, uvals, tucked, verts2, tris


def spoil(grid, x, y):
    d = 0.0
    gx, gy = int(math.floor(x / 8.0)), int(math.floor(y / 8.0))
    for i in range(gx - 1, gx + 2):
        for j in range(gy - 1, gy + 2):
            for p in grid.get((i, j), ()):
                rr = (p["x"] - x) ** 2 + (p["y"] - y) ** 2
                s = 1.8 * p["ax"]
                d += math.exp(-rr / (s * s))
    return min(1.0, 0.6 * d)


def build_dome(p):
    """A low draped dome on the base surface: the base ring is buried, the top stands p['h'] proud."""
    seg, rings = p["seg"], p["rings"]
    c, s = math.cos(p["yaw"]), math.sin(p["yaw"])
    g, _ = entrance(p["x"], p["y"])
    col = (1.0, g, 1.0, p["rnd"])
    bury = 0.25
    l1, l2 = p["lump"]

    def place(lx, ly, prof):
        x = p["x"] + c * lx - s * ly
        y = p["y"] + s * lx + c * ly
        z = ground_at(x, y) - bury + (p["h"] + bury) * prof
        return V(x, y, z, col, (x / 240.0 + 0.5, y / 240.0 + 0.5))

    apex = place(rng.uniform(-0.15, 0.15) * p["ax"], rng.uniform(-0.15, 0.15) * p["ay"], 1.0)
    ring_vs = []
    for rho in rings:
        ring = []
        for i in range(seg):
            ph = TAU * i / seg
            edge = 1.0 + 0.14 * math.sin(2 * ph + l1) + 0.09 * math.sin(3 * ph + l2)
            prof = 0.0 if rho >= 0.999 else (1.0 - rho * rho) ** 0.7 * rng.uniform(0.9, 1.08)
            ring.append(place(math.cos(ph) * p["ax"] * rho * edge, math.sin(ph) * p["ay"] * rho * edge, prof))
        ring_vs.append(ring)
    for i in range(seg):
        j = (i + 1) % seg
        face((apex, ring_vs[0][i], ring_vs[0][j]))
    for k in range(len(ring_vs) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            face((ring_vs[k][i], ring_vs[k + 1][i], ring_vs[k + 1][j], ring_vs[k][j]))


def build_scrap(sc):
    """A bitten-out piece of leaf lying on the surface: round where it was cut, one straight leaf margin."""
    seg = 16
    R = sc["rad"]
    c, s = math.cos(sc["yaw"]), math.sin(sc["yaw"])
    outline = []
    for i in range(seg):
        a = TAU * i / seg
        rr = R * (1.0 + 0.07 * math.sin(3 * a + sc["phase"]) + 0.035 * math.sin(7 * a))
        lx, ly = math.cos(a) * rr * 1.15, math.sin(a) * rr
        if ly > 0.6 * R:  # the leaf's own margin, faintly serrated
            ly = 0.6 * R + 0.03 * R * abs(math.sin(lx / R * 9.0))
        outline.append((lx, ly))
    # UV0 spans the scrap's bounding box 0..1 (U along, V across); the straight margin side makes the shape
    # asymmetric, so V = 0.5 runs along the middle of the box, a little off the round part's centre.
    lx0, lx1 = min(p[0] for p in outline), max(p[0] for p in outline)
    ly0, ly1 = min(p[1] for p in outline), max(p[1] for p in outline)
    col = (1.0, sc["rnd"], 0.0, 0.0)

    def place(lx, ly):
        x = sc["x"] + c * lx - s * ly
        y = sc["y"] + s * lx + c * ly
        rho2 = (lx / (1.15 * R)) ** 2 + (ly / R) ** 2
        z = ground_at(x, y) + 0.15 + sc["curl"] * rho2
        uv = ((lx - lx0) / (lx1 - lx0), (ly - ly0) / (ly1 - ly0))
        return V(x, y, z, col, uv)

    centre = place(0.0, 0.0)
    mid = [place(0.55 * lx, 0.55 * ly) for lx, ly in outline]
    rim = [place(lx, ly) for lx, ly in outline]
    for i in range(seg):
        j = (i + 1) % seg
        f1 = face((centre, mid[i], mid[j]))
        f2 = face((mid[i], rim[i], rim[j], mid[j]))
        for f in (f1, f2):
            if f:
                f.material_index = 1


def surface_error(verts2, tris, tucked):
    """Max |mesh - field| (cm) at face centroids and edge midpoints of the mound surface, r < 124: overall (the
    skirt lift dominates it) and where the field is above SKIRT_LIFT (interpolation error alone)."""
    worst, where, worst_in, where_in = 0.0, 0.0, 0.0, 0.0
    for a, b, c in tris:
        if tucked[a] or tucked[b] or tucked[c]:
            continue
        P = [verts2[a], verts2[b], verts2[c]]
        Z = [max(surf(p.length), SKIRT_LIFT) for p in P]
        for w in ((1 / 3, 1 / 3, 1 / 3), (0.5, 0.5, 0), (0, 0.5, 0.5), (0.5, 0, 0.5)):
            x = sum(w[i] * P[i].x for i in range(3))
            y = sum(w[i] * P[i].y for i in range(3))
            r = math.hypot(x, y)
            if r >= 124.0:
                continue
            e = abs(sum(w[i] * Z[i] for i in range(3)) - surf(r))
            if e > worst:
                worst, where = e, r
            if min(Z) > SKIRT_LIFT + 1e-6 and e > worst_in:
                worst_in, where_in = e, r
    return {"all": (round(worst, 4), round(where, 2)), "interp": (round(worst_in, 4), round(where_in, 2))}


def make_object():
    global bm
    bm = bmesh.new()
    attrs.clear()
    place()
    grid = {}
    for p in pellets:
        grid.setdefault((int(math.floor(p["x"] / 8.0)), int(math.floor(p["y"] / 8.0))), []).append(p)
    bverts, uvals, tucked, verts2, tris = build_base(grid)
    n_base = len(bverts)
    err = surface_error(verts2, tris, tucked)
    for p in pellets:
        build_dome(p)
    for sc in scraps:
        build_scrap(sc)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for f in bm.faces:
        f.smooth = True
    bm.verts.index_update()
    base_index = {v: i for i, v in enumerate(bverts)}

    me = bpy.data.meshes.new("SM_Nest")
    uv_layer = bm.loops.layers.uv.new("UV0")
    col_layer = bm.loops.layers.float_color.new("Col")
    for f in bm.faces:
        for lp in f.loops:
            col, uv = attrs[lp.vert]
            lp[uv_layer].uv = uv
            lp[col_layer] = col
    vert_order = [v for v in bm.verts]
    bm.to_mesh(me)
    tri = len(me.polygons)
    bm.free()
    me.color_attributes.active_color_name = "Col"
    me.color_attributes.render_color_index = me.color_attributes.find("Col")

    # Funnel: inside the entrance the vertex normals lean toward the centre, so the light reads a hollow.
    normals = [Vector(n.vector) for n in me.vertex_normals]
    for idx, v in enumerate(vert_order):
        bi = base_index.get(v)
        if bi is None or tucked[bi]:
            continue
        u = uvals[bi]
        t = (u - 0.25) / 0.85
        if 0.0 < t < 1.0:
            co = me.vertices[idx].co
            radial = Vector((co.x, co.y, 0.0))
            if radial.length > 1e-6:
                tilt = math.radians(FUNNEL_DEG) * math.sin(math.pi * t)
                n = normals[idx]
                normals[idx] = (n - radial.normalized() * math.tan(tilt)).normalized()
    # Crease: the field's blend bends sharply near r = 122, which drew a hard ring of light. Over that band the
    # normals follow the profile smoothed over +-SMOOTH_W cm (shading only; the positions stay on the field).
    for idx, v in enumerate(vert_order):
        bi = base_index.get(v)
        if bi is None or tucked[bi]:
            continue
        co = me.vertices[idx].co
        r = math.hypot(co.x, co.y) / CM
        w = smooth01(111.0, 116.0, r) * (1.0 - smooth01(125.0, 127.5, r))
        if w <= 0.0 or r < 1e-3:
            continue
        slope = (base_height(r + SMOOTH_W) - base_height(r - SMOOTH_W)) / (2.0 * SMOOTH_W)
        ns = Vector((-slope * co.x / (r * CM), -slope * co.y / (r * CM), 1.0)).normalized()
        normals[idx] = normals[idx].lerp(ns, w).normalized()
    me.normals_split_custom_set_from_vertices([tuple(n) for n in normals])

    soil = preview_soil_material()
    leaf = bpy.data.materials.new("M_Leaf")
    leaf.use_nodes = True
    b = leaf.node_tree.nodes.get("Principled BSDF")
    if b:
        b.inputs["Base Color"].default_value = (0.09, 0.26, 0.035, 1.0)
        b.inputs["Roughness"].default_value = 0.6
    me.materials.append(soil)
    me.materials.append(leaf)
    ob = bpy.data.objects.new("SM_Nest", me)
    bpy.context.scene.collection.objects.link(ob)
    counts = {"base_vertices": n_base, "pellets_and_clumps": len(pellets), "leaf_scraps": len(scraps)}
    return ob, tri, err, counts


def preview_soil_material():
    """Blender-only stand-in for M_NestSoil, so the previews show the vertex colours."""
    mat = bpy.data.materials.new("M_NestSoil")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(vc.outputs["Color"], sep.inputs["Color"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    sock = lambda node, name, kind, out=False: [s for s in (node.outputs if out else node.inputs)
                                                if s.name == name and s.type == kind][0]
    sock(mix, "A", "RGBA").default_value = (0.20, 0.16, 0.06, 1.0)   # ground stand-in
    sock(mix, "B", "RGBA").default_value = (0.075, 0.048, 0.03, 1.0)  # fresh soil
    nt.links.new(sep.outputs[0], sock(mix, "Factor", "VALUE"))
    inv = nt.nodes.new("ShaderNodeMath")
    inv.operation = "SUBTRACT"
    inv.inputs[0].default_value = 1.0
    nt.links.new(sep.outputs[1], inv.inputs[1])
    pw = nt.nodes.new("ShaderNodeMath")
    pw.operation = "POWER"
    pw.inputs[1].default_value = 2.5
    nt.links.new(inv.outputs[0], pw.inputs[0])
    tint = nt.nodes.new("ShaderNodeMath")
    tint.operation = "MULTIPLY_ADD"
    tint.inputs[1].default_value = 0.35
    tint.inputs[2].default_value = 1.0
    nt.links.new(sep.outputs[2], tint.inputs[0])
    s1 = nt.nodes.new("ShaderNodeVectorMath")
    s1.operation = "SCALE"
    nt.links.new(sock(mix, "Result", "RGBA", out=True), s1.inputs[0])
    nt.links.new(pw.outputs[0], s1.inputs["Scale"])
    s2 = nt.nodes.new("ShaderNodeVectorMath")
    s2.operation = "SCALE"
    nt.links.new(s1.outputs[0], s2.inputs[0])
    nt.links.new(tint.outputs[0], s2.inputs["Scale"])
    if bsdf:
        nt.links.new(s2.outputs[0], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.95
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.2
    return mat


# ---------------------------------------------------------------- scene / render / export
def clear_scene():
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for d in list(coll):
            coll.remove(d)


def look_at(ob, target):
    d = Vector(target) - ob.location
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def render_previews(path_top, path_3q, extra=None):
    sc = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    sc.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in engines else "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1000, 700
    sc.render.film_transparent = False
    world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.55, 0.62, 0.70, 1)
        bg.inputs["Strength"].default_value = 1.0
    temp = []
    # ground stand-in (flat, like the game's ground disk) and the ant for scale, standing on the entrance
    bpy.ops.mesh.primitive_plane_add(size=4.0, location=(0, 0, 0))
    plane = bpy.context.active_object
    gm = bpy.data.materials.new("PreviewGround")
    gm.use_nodes = True
    gb = gm.node_tree.nodes.get("Principled BSDF")
    gb.inputs["Base Color"].default_value = (0.20, 0.16, 0.06, 1.0)
    gb.inputs["Roughness"].default_value = 1.0
    plane.data.materials.append(gm)
    temp.append(plane)
    ant_path = os.path.join(OUT_DIR, "SM_Ant.fbx")
    if os.path.exists(ant_path):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=ant_path)
        for o in set(bpy.data.objects) - before:
            if o.parent is None:
                o.location = (0.0, 0.0, surf(0.0) * CM)
            temp.append(o)
    ld = bpy.data.lights.new("PreviewSun", "SUN")
    ld.energy = 4.0
    lo = bpy.data.objects.new("PreviewSun", ld)
    sc.collection.objects.link(lo)
    lo.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    cd = bpy.data.cameras.new("PreviewCam")
    co = bpy.data.objects.new("PreviewCam", cd)
    sc.collection.objects.link(co)
    sc.camera = co
    temp += [co, lo]
    shots = [("ORTHO", (0.0, 0.0, 5.0), (0.0, 0.0, 0.0), path_top),
             ("PERSP", (-2.5, -1.45, 1.05), (0.05, 0.0, 0.0), path_3q)]
    if extra:
        shots.append(("PERSP", (-0.62, -0.38, 0.30), (0.03, 0.0, 0.06), extra))
    for kind, loc, tgt, path in shots:
        sc.render.resolution_y = 1000 if kind == "ORTHO" else 700
        cd.type = kind
        cd.ortho_scale = 3.3
        cd.lens = 50
        co.location = loc
        look_at(co, tgt)
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
    for o in temp:
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials):
        for d in list(coll):
            if d.users == 0:
                coll.remove(d)


def export_fbx(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                             add_leaf_bones=False, bake_anim=False, path_mode="STRIP", colors_type="LINEAR")


def main():
    clear_scene()
    ob, tri, err, counts = make_object()
    bb = [tuple(round(c / CM, 2) for c in (min(v.co[i] for v in ob.data.vertices),
                                           max(v.co[i] for v in ob.data.vertices))) for i in range(3)]
    info = {"triangles": tri, "bbox_cm_min_max_xyz": bb, "max_surface_error_cm_r_lt_124": err}
    info.update(counts)
    if EXPORT:
        os.makedirs(OUT_DIR, exist_ok=True)
        if RENDER_PREVIEWS:
            render_previews(os.path.join(OUT_DIR, "nest_preview_top.png"),
                            os.path.join(OUT_DIR, "nest_preview_3q.png"), EXTRA_SHOT)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "Nest.blend"))
        export_fbx(ob, os.path.join(OUT_DIR, "SM_Nest.fbx"))
    print("make_nest:", info)
    return info


result = main()
