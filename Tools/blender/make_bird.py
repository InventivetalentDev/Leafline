"""Leafline: the bird generator (Blender 5.x).

Run inside Blender (Scripting tab, or exec() it).  It clears the scene, builds a plain small songbird gliding with its
wings spread flat as ONE mesh object, renders three previews, saves Art/Blender/Bird.blend and exports
Art/Blender/SM_Bird.fbx.  Everything is procedural; no textures, no rig.  No random numbers: rerunning gives the same
mesh.

Coordinates: metres, +X forward (beak), +Z up, +Y is the bird's LEFT.  Nominal size: 1.00 m wingspan (tip to tip),
0.62 m from beak tip to tail tip; the game scales it (BirdScale).  The origin is the middle of the shoulder line: the
shoulder lines run along X at y = +-SHOULDER_Y, z = 0, and each wing is a flat plate (upper skin above its middle
plane, under skin below) tilted up DIHEDRAL_DEG about its shoulder line, and the material's flap bends it about that
line.

UV channels (all per-vertex values, see Art/Blender/README.md):
  UV0  U 0 body (and head, beak, eyes, tail), 0.25 left wing, 0.75 right wing
       V distance from the shoulder line along the wing / WING_LEN, 0 there (and inside the body) to 1 at the wing
       tip; 0 off the wings
  UV1  U part id: 0 body, 0.2 head, 0.4 beak, 0.6 wing, 0.8 tail, 1.0 eye
       V body and head: 0 back to 1 belly; wings and tail: 0 upper skin, 1 under skin; beak and eyes: 0
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector, geometry

# ---------------------------------------------------------------- parameters
SHOULDER_Y = 0.05           # shoulder line, |y| (inside the body)
WING_LEN = 0.45             # shoulder line to wing tip: span = 2 * (SHOULDER_Y + WING_LEN) = 1.00 m
ROOT_IN = 0.025             # the wing root starts this far inside the shoulder line (buried in the body)
DIHEDRAL_DEG = 5.0          # each wing tilts up this much about its shoulder line, so the side view shows a shallow V
SPHERE_U, SPHERE_V = 16, 10
RENDER_PREVIEWS = True
EXPORT = True
CHECK_DIR = None            # set to a folder to also render a view from below and two flap poses (see flap_offset)

try:
    _ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
except NameError:  # executed via exec() without __file__
    _ROOT = "D:/UnrealProjects/AntTreeGame"
OUT_DIR = os.path.join(_ROOT, "Art", "Blender")

# part ids (UV1.u)
P_BODY, P_HEAD, P_BEAK, P_WING, P_TAIL, P_EYE = 0.0, 0.2, 0.4, 0.6, 0.8, 1.0

# ---------------------------------------------------------------- mesh helpers
bm = None
attrs = {}      # BMVert -> (uv0, uv1)
sharp = set()   # frozensets of two BMVerts: the rim of the wing and tail skins (hard edges)


def new_vert(p, uv0, uv1):
    v = bm.verts.new(p)
    attrs[v] = (uv0, uv1)
    return v


def rot_y(vec, ang):
    c, s = math.cos(ang), math.sin(ang)
    return Vector((c * vec.x + s * vec.z, vec.y, -s * vec.x + c * vec.z))


def ellipsoid(center, radii, part, pitch=0.0, deform=None, res=None, belly=True):
    """Closed ellipsoid. pitch rotates about Y (positive tips the front down). UV1.v = belly (0 top, 1 underneath)."""
    SU, SV = res or (SPHERE_U, SPHERE_V)
    c = Vector(center)

    def make(u):
        b = min(1.0, max(0.0, (0.25 - u.z) / 0.8)) if belly else 0.0
        b = b * b * (3 - 2 * b)
        if deform:
            u = deform(u)
        q = Vector((u.x * radii[0], u.y * radii[1], u.z * radii[2]))
        return new_vert(c + rot_y(q, pitch), (0.0, 0.0), (part, b))

    top = make(Vector((0, 0, 1)))
    bot = make(Vector((0, 0, -1)))
    rings = []
    for j in range(1, SV):
        th = math.pi * j / SV
        rings.append([make(Vector((math.sin(th) * math.cos(2 * math.pi * i / SU),
                                   math.sin(th) * math.sin(2 * math.pi * i / SU), math.cos(th)))) for i in range(SU)])
    for i in range(SU):
        j = (i + 1) % SU
        bm.faces.new((top, rings[0][j], rings[0][i]))
        bm.faces.new((bot, rings[-1][i], rings[-1][j]))
    for r in range(len(rings) - 1):
        for i in range(SU):
            j = (i + 1) % SU
            bm.faces.new((rings[r][i], rings[r][j], rings[r + 1][j], rings[r + 1][i]))


def cone(p0, p1, r0, part, sides=8):
    """Cone from a base ring at p0 (radius r0, capped) to a point at p1."""
    p0, p1 = Vector(p0), Vector(p1)
    d = (p1 - p0).normalized()
    u = d.cross(Vector((0, 0, 1))).normalized()
    w = d.cross(u).normalized()
    ring = [new_vert(p0 + (u * math.cos(2 * math.pi * i / sides) + w * math.sin(2 * math.pi * i / sides)) * r0,
                     (0.0, 0.0), (part, 0.0)) for i in range(sides)]
    tip = new_vert(p1, (0.0, 0.0), (part, 0.0))
    cap = new_vert(p0, (0.0, 0.0), (part, 0.0))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((ring[i], ring[j], tip))
        bm.faces.new((cap, ring[j], ring[i]))


def catmull(pts, n_per):
    """Points along a Catmull-Rom curve through pts (2D tuples), n_per samples per span, ends included once."""
    P = [Vector(p) for p in pts]
    P = [P[0] * 2 - P[1]] + P + [P[-1] * 2 - P[-2]]
    out = []
    for k in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[k - 1], P[k], P[k + 1], P[k + 2]
        for i in range(n_per):
            t = i / n_per
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    out.append(P[-2])
    return [(p.x, p.y) for p in out]


def segment(a, b, n):
    """n points from a towards b, a included, b excluded."""
    return [(a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n) for i in range(n)]


def slab(outline, inner, to3d, thick, uv0_of, part):
    """A thin closed plate from a 2D outline (list of points, any winding) plus interior points: the upper skin at
    +0.65 * thick, the under skin at -0.35 * thick, and a wall round the rim. to3d(a, b) maps a 2D point to (x, y);
    thick(a, b) is the plate's thickness there; uv0_of(x, y) gives UV0. Rim edges are marked sharp."""
    n_out = len(outline)
    pts2 = [Vector(p) for p in outline] + [Vector(p) for p in inner]
    out = geometry.delaunay_2d_cdt(pts2, [], [list(range(n_out))], 1, 1e-6, True)
    verts2, faces, orig = out[0], out[2], out[3]
    top, bot, rim = [], [], {}
    for i, p in enumerate(verts2):
        x, y = to3d(p.x, p.y)
        t = thick(p.x, p.y)
        uv0 = uv0_of(x, y)
        top.append(new_vert((x, y, 0.65 * t), uv0, (part, 0.0)))
        bot.append(new_vert((x, y, -0.35 * t), uv0, (part, 1.0)))
        for o in orig[i]:
            if o < n_out:
                rim[o] = i
    for f in faces:
        a, b, c = f[0], f[1], f[2]
        bm.faces.new((top[a], top[b], top[c]))
        bm.faces.new((bot[a], bot[c], bot[b]))
    ring = [rim[k] for k in range(n_out)]
    for k in range(n_out):
        i, j = ring[k], ring[(k + 1) % n_out]
        if i == j:
            continue
        bm.faces.new((top[i], bot[i], bot[j], top[j]))
        sharp.add(frozenset((top[i], top[j])))
        sharp.add(frozenset((bot[i], bot[j])))


# ---------------------------------------------------------------- the wing (2D in (s, x): s spanwise from the shoulder)
# Leading edge from the root (inside the body) out to the wrist and back to the tip.
LEADING = [(-ROOT_IN, 0.050), (0.0, 0.055), (0.08, 0.074), (0.17, 0.087), (0.26, 0.074), (0.34, 0.046),
           (0.405, 0.016)]
# Primaries: tips T0 (the wing tip) .. T4 and the slots between them; one smooth curve runs through them, so the
# tips and the slot bottoms come out rounded, like separate feathers.
TIPS = [(0.4495, -0.004), (0.438, -0.048), (0.420, -0.088), (0.392, -0.119), (0.357, -0.140)]
SLOTS = [(0.368, -0.026), (0.355, -0.064), (0.335, -0.094), (0.303, -0.107)]
# Secondaries: the trailing edge from the inner primaries back to the root, scalloped by feather tips.
TRAIL_OUT, TRAIL_IN = (0.300, -0.139), (-ROOT_IN, -0.104)
HAND = (0.27, -0.035)          # the primaries fan out from about here
N_SECONDARIES = 6


def wing_outline():
    chain = LEADING[:]
    for k in range(len(TIPS)):
        # a rounded tip about 2.4 cm wide: two shoulders on either side of the tip, across the feather's axis
        ax = Vector(TIPS[k]) - Vector(HAND)
        ax.normalize()
        pr = Vector((-ax.y, ax.x))
        t = Vector(TIPS[k])
        # the outline comes in on the feather's outer side (toward the wing tip, +pr) and leaves on its inner side
        outer, inner = t - ax * 0.011 + pr * 0.012, t - ax * 0.011 - pr * 0.012
        chain += [tuple(outer), TIPS[k], tuple(inner)]
        if k < len(SLOTS):
            chain.append(SLOTS[k])
    chain.append(TRAIL_OUT)
    pts = catmull(chain, 4)[:-1]              # root, leading edge, the primaries, up to TRAIL_OUT
    # secondaries from TRAIL_OUT inward: a shallow notch between feathers, a rounded tip on each
    for i in range(N_SECONDARIES):
        for f, dx in ((0.0, 0.004), (0.25, -0.0015), (0.5, -0.0035), (0.75, -0.0015)):
            a = (i + f) / N_SECONDARIES
            pts.append((TRAIL_OUT[0] + (TRAIL_IN[0] - TRAIL_OUT[0]) * a,
                        TRAIL_OUT[1] + (TRAIL_IN[1] - TRAIL_OUT[1]) * a + dx))
    pts.append(TRAIL_IN)
    pts += segment(TRAIL_IN, LEADING[0], 4)[1:]  # across the buried root
    return pts


def leading_x(s):
    best = None
    for (s0, x0), (s1, x1) in zip(LEADING, LEADING[1:]):
        if s0 <= s <= s1:
            best = x0 + (x1 - x0) * (s - s0) / (s1 - s0)
    return best if best is not None else LEADING[-1][1]


def wing_inner():
    """Interior points: spanwise stations, so the flap bends smoothly."""
    pts = []
    for i in range(1, 13):
        s = -ROOT_IN + (0.30 + ROOT_IN) * i / 12.5
        xl = leading_x(s) - 0.012
        xt = TRAIL_IN[1] + (TRAIL_OUT[1] - TRAIL_IN[1]) * (s + ROOT_IN) / (TRAIL_OUT[0] + ROOT_IN) + 0.016
        for j in range(4):
            pts.append((s, xl + (xt - xl) * (j + 0.5) / 4))
    # the hand, between the leading edge and the slots
    for s, x_slot in ((0.31, -0.104), (0.33, -0.084), (0.35, -0.056)):
        xl = leading_x(s) - 0.010
        for j in range(3):
            pts.append((s, xl + (x_slot - xl) * (j + 0.5) / 3))
    return pts


def wing_thick(s, x):
    tmax = 0.016 * (1.0 - 0.72 * max(0.0, min(1.0, s / WING_LEN)))
    d = max(0.0, leading_x(max(-ROOT_IN, min(s, WING_LEN))) - x)
    return 0.0022 + tmax * math.exp(-d / 0.05)


def build_wings():
    outline, inner = wing_outline(), wing_inner()
    cd, sd = math.cos(math.radians(DIHEDRAL_DEG)), math.sin(math.radians(DIHEDRAL_DEG))
    for sy, u in ((1.0, 0.25), (-1.0, 0.75)):
        n0 = len(bm.verts)
        def to3d(s, x, sy=sy):
            return x, sy * (SHOULDER_Y + s)

        def uv0_of(x, y, u=u):
            return (u, max(0.0, min(1.0, (abs(y) - SHOULDER_Y) / WING_LEN)))

        slab(outline, inner, to3d, wing_thick, uv0_of, P_WING)
        # the dihedral: turn the plate rigidly about the shoulder line (UV0.v stays the distance along the wing)
        for v in list(bm.verts)[n0:]:
            span, h = abs(v.co.y) - SHOULDER_Y, v.co.z
            v.co.y = sy * (SHOULDER_Y + span * cd - h * sd)
            v.co.z = span * sd + h * cd


# ---------------------------------------------------------------- the tail (2D in (x, y)), fanned
TAIL_Z = -0.016
TAIL_PIVOT = (-0.115, 0.0)
TAIL_R, TAIL_NOTCH_R, TAIL_HALF_ANGLE, TAIL_FEATHERS = 0.245, 0.235, math.radians(31), 9


def tail_outline():
    px, py = TAIL_PIVOT
    pts = [(-0.10, -0.030)]
    # right edge out to the fan, then the scalloped fan from right (-y) to left (+y), then the left edge back
    e_r = (px - (TAIL_R - 0.004) * math.cos(TAIL_HALF_ANGLE), py - (TAIL_R - 0.004) * math.sin(TAIL_HALF_ANGLE))
    pts += segment((-0.10, -0.030), e_r, 4)[1:]
    step = 2 * TAIL_HALF_ANGLE / TAIL_FEATHERS

    def at(a, r):
        return (px - r * math.cos(a), py + r * math.sin(a))

    # each feather: a rounded tip between two notches; the outer corners sit on the fan's edges
    pts.append(at(-TAIL_HALF_ANGLE, TAIL_R - 0.004))
    for i in range(TAIL_FEATHERS):
        a1 = -TAIL_HALF_ANGLE + step * (i + 0.5)
        pts += [at(a1 - 0.28 * step, TAIL_R + 0.001), at(a1, TAIL_R + 0.004), at(a1 + 0.28 * step, TAIL_R + 0.001)]
        if i < TAIL_FEATHERS - 1:
            pts.append(at(a1 + 0.5 * step, TAIL_NOTCH_R))
    e_l = at(TAIL_HALF_ANGLE, TAIL_R - 0.004)
    pts += segment(e_l, (-0.10, 0.030), 4)
    pts.append((-0.10, 0.030))
    return pts


def tail_inner():
    px, py = TAIL_PIVOT
    pts = []
    for r in (0.07, 0.13, 0.19):
        n = 3 if r < 0.1 else 5
        for i in range(n):
            a = -TAIL_HALF_ANGLE * 0.8 + 1.6 * TAIL_HALF_ANGLE * (i + 0.5) / n
            pts.append((px - r * math.cos(a), py + r * math.sin(a)))
    return pts


def build_tail():
    def to3d(x, y):
        return x, y

    def thick(x, y):
        return 0.0028 + 0.0040 * max(0.0, min(1.0, (x + 0.33) / 0.22))

    outline, inner = tail_outline(), tail_inner()
    n0 = len(bm.verts)
    slab(outline, inner, to3d, thick, lambda x, y: (0.0, 0.0), P_TAIL)
    for v in list(bm.verts)[n0:]:
        v.co.z += TAIL_Z


# ---------------------------------------------------------------- body, head, beak, eyes
def breast(u):
    """Fuller breast in front, slimmer toward the tail."""
    k = 1.0 + 0.10 * u.x
    return Vector((u.x, u.y * k, u.z * k - 0.06 * max(0.0, u.x) * (1 - u.z * u.z)))


def build():
    ellipsoid((-0.015, 0, -0.022), (0.178, 0.068, 0.066), P_BODY, pitch=0.05, deform=breast)
    ellipsoid((0.163, 0, 0.002), (0.056, 0.050, 0.050), P_HEAD, res=(14, 9))
    # neck and nape: fills the waist between the body's tapered front and the head, about as wide as the head,
    # so the outline from below and from the side is one body-head shape (the head read as a ball stuck on)
    ellipsoid((0.125, 0, 0.000), (0.065, 0.052, 0.050), P_BODY, res=(14, 9))
    cone((0.205, 0, -0.006), (0.258, 0, -0.013), 0.016, P_BEAK)
    for sy in (1, -1):
        d = Vector((0.52, sy * 0.66, 0.40)).normalized()
        c = Vector((0.163, 0, 0.002)) + Vector((d.x * 0.056, d.y * 0.050, d.z * 0.050)) * 0.93
        ellipsoid(tuple(c), (0.0085, 0.0085, 0.0085), P_EYE, res=(8, 5), belly=False)
    build_tail()
    build_wings()


# ---------------------------------------------------------------- the flap (a Python copy of Tools/materials/bird.hlsl)
FLAP_AMP, FLAP_TIP_GAIN, FLAP_TIP_LAG = 0.61, 0.7, 0.5
DIHEDRAL = math.radians(DIHEDRAL_DEG)


def flap_offset(co, uv0, phase, amount):
    """Offset of one vertex (mesh space, metres here) for a flap phase and amount; the same maths as the material,
    with V as authored (the material reads 1 - V after Unreal's flip)."""
    if uv0[0] < 0.1 or amount <= 0.0:
        return Vector((0, 0, 0))
    v = uv0[1]
    side = 1.0 if co.y >= 0 else -1.0
    A = amount * FLAP_AMP
    b0 = DIHEDRAL + A * math.sin(phase)                        # the middle plane's angle at the shoulder
    a1 = A * FLAP_TIP_GAIN * math.sin(phase - FLAP_TIP_LAG)    # extra turn from shoulder to tip
    phi = b0 + a1 * v

    def along(start, turn):
        if abs(turn) > 1e-3:
            return (WING_LEN * (math.sin(start + turn * v) - math.sin(start)) / turn,
                    WING_LEN * (math.cos(start) - math.cos(start + turn * v)) / turn)
        m = start + 0.5 * turn * v
        return WING_LEN * v * math.cos(m), WING_LEN * v * math.sin(m)

    cs, cu = along(b0, a1)                 # bent
    rs, ru = along(DIHEDRAL, 0.0)          # the modelled pose
    h = (co.z - ru) / math.cos(DIHEDRAL)   # distance from the middle plane
    d_span = (cs - h * math.sin(phi)) - (rs - h * math.sin(DIHEDRAL))
    d_up = (cu + h * math.cos(phi)) - (ru + h * math.cos(DIHEDRAL))
    return Vector((0.0, side * d_span, d_up))


def make_object():
    global bm
    bm = bmesh.new()
    attrs.clear()
    sharp.clear()
    build()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for e in bm.edges:
        e.smooth = frozenset(e.verts) not in sharp
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for f in bm.faces:
        f.smooth = True

    me = bpy.data.meshes.new("SM_Bird")
    layers = [bm.loops.layers.uv.new(n) for n in ("UV0", "UV1")]
    for f in bm.faces:
        for lp in f.loops:
            for k, layer in enumerate(layers):
                lp[layer].uv = attrs[lp.vert][k]
    bm.to_mesh(me)
    tri = len(me.polygons)
    bm.free()

    mat = preview_material()
    me.materials.append(mat)
    ob = bpy.data.objects.new("SM_Bird", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, tri


def preview_material():
    """Stand-in for M_Bird in the previews: colour by part id (UV1.u). Only the slot name reaches the FBX."""
    mat = bpy.data.materials.new("M_Bird")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 0.75
    uvn = nt.nodes.new("ShaderNodeUVMap")
    uvn.uv_map = "UV1"
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uvn.outputs["UV"], sep.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.interpolation = "CONSTANT"
    cols = [(0.0, (0.105, 0.085, 0.065)), (0.1, (0.095, 0.085, 0.075)), (0.3, (0.05, 0.045, 0.04)),
            (0.5, (0.085, 0.07, 0.055)), (0.7, (0.075, 0.062, 0.05)), (0.9, (0.008, 0.007, 0.006))]
    els = ramp.color_ramp.elements
    els[0].position, els[0].color = cols[0][0], (*cols[0][1], 1)
    els[1].position, els[1].color = cols[1][0], (*cols[1][1], 1)
    for pos, c in cols[2:]:
        e = els.new(pos)
        e.color = (*c, 1)
    nt.links.new(sep.outputs["X"], ramp.inputs[0])
    # lighter belly and under skin (UV1.v)
    light = nt.nodes.new("ShaderNodeMix")
    light.data_type = "RGBA"

    def sock(socks, name, typ):
        return [x for x in socks if x.name == name and x.type == typ][0]

    sock(light.inputs, "B", "RGBA").default_value = (0.34, 0.31, 0.26, 1)
    fac = nt.nodes.new("ShaderNodeMath")
    fac.operation = "MULTIPLY"
    fac.inputs[1].default_value = 0.7
    nt.links.new(sep.outputs["Y"], fac.inputs[0])
    nt.links.new(fac.outputs[0], sock(light.inputs, "Factor", "VALUE"))
    nt.links.new(ramp.outputs[0], sock(light.inputs, "A", "RGBA"))
    nt.links.new(sock(light.outputs, "Result", "RGBA"), bsdf.inputs["Base Color"])
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


def render_views(views, size=(1000, 700)):
    """views: list of (camera location, target, path)."""
    sc = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    sc.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in engines else "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = size
    sc.render.film_transparent = False
    world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.55, 0.62, 0.70, 1)
        bg.inputs["Strength"].default_value = 1.0
    ld = bpy.data.lights.new("PreviewSun", "SUN")
    ld.energy = 4.0
    lo = bpy.data.objects.new("PreviewSun", ld)
    sc.collection.objects.link(lo)
    lo.rotation_euler = (math.radians(40), math.radians(10), math.radians(-35))
    cd = bpy.data.cameras.new("PreviewCam")
    cd.lens = 50
    co = bpy.data.objects.new("PreviewCam", cd)
    sc.collection.objects.link(co)
    sc.camera = co
    try:
        for loc, tgt, path in views:
            co.location = loc
            look_at(co, tgt)
            sc.render.filepath = path
            bpy.ops.render.render(write_still=True)
    finally:
        for o in (co, lo):
            bpy.data.objects.remove(o, do_unlink=True)


def preview_views(prefix):
    tgt = (-0.03, 0.0, 0.0)
    return [((1.05, -1.05, 0.75), tgt, prefix + "_3q.png"),
            ((-0.03, 0.0, 2.3), tgt, prefix + "_top.png"),
            ((-0.03, -2.0, 0.02), tgt, prefix + "_side.png")]


def check_renders(ob, folder):
    """A view from below (how the player sees it) and the flap at its top and bottom, through flap_offset."""
    os.makedirs(folder, exist_ok=True)
    me = ob.data
    uv0 = me.uv_layers["UV0"].data
    v_uv = {}
    for lp in me.loops:
        v_uv[lp.vertex_index] = tuple(uv0[lp.index].uv)
    rest = [v.co.copy() for v in me.vertices]
    worst_glide = max(flap_offset(rest[i], v_uv[i], 1.3, 0.0).length for i in range(len(rest)))
    render_views([((-0.03, 0.0, -2.3), (-0.03, 0, 0), os.path.join(folder, "bird_below.png"))])
    for name, phase in (("up", math.pi / 2), ("down", 3 * math.pi / 2)):
        for i, v in enumerate(me.vertices):
            v.co = rest[i] + flap_offset(rest[i], v_uv[i], phase, 1.0)
        me.update()
        render_views([((1.6, -0.5, 0.35), (0, 0, 0.05), os.path.join(folder, "bird_flap_%s.png" % name))])
    for i, v in enumerate(me.vertices):
        v.co = rest[i]
    me.update()
    return worst_glide


def export_fbx(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                             add_leaf_bones=False, bake_anim=False, path_mode="STRIP")


def main():
    clear_scene()
    ob, tri = make_object()
    co = [v.co for v in ob.data.vertices]
    bb = [tuple(round(f(c[i] for c in co), 4) for f in (min, max)) for i in range(3)]
    info = {"triangles": tri, "vertices": len(co), "bbox_min_max_xyz": bb}
    if CHECK_DIR:
        info["max_offset_gliding"] = check_renders(ob, CHECK_DIR)
    if EXPORT:
        os.makedirs(OUT_DIR, exist_ok=True)
        if RENDER_PREVIEWS:
            render_views(preview_views(os.path.join(OUT_DIR, "bird_preview")))
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "Bird.blend"))
        export_fbx(ob, os.path.join(OUT_DIR, "SM_Bird.fbx"))
    print("make_bird:", info)
    return info


result = main()
