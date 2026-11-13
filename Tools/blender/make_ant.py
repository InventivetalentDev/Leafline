"""Leafline: leafcutter worker ant generator (Blender 5.x).

Run inside Blender (Scripting tab, or exec() it).  It clears the scene, builds the ant
as ONE mesh object, renders two previews, saves Art/Blender/Ant.blend and exports
Art/Blender/SM_Ant.fbx.  Everything is procedural; no textures, no rig.

Coordinates: metres, +X forward, +Z up, +Y is the ant's LEFT, origin on the ground
under the middle of the thorax, feet at Z = 0.  Total length is scaled to BODY_LENGTH.

UV channels (all per-vertex values, see Art/Blender/README.md):
  UV0  U leg group (0 body, 0.25 tripod A, 0.75 tripod B)   V distance along leg 0..1
  UV1  U part id (0 body, .2 head, .4 antennae, .6 mandibles, .8 legs)   V side 0 L / 1 R
  UV2  U leg pair (0 front, .5 middle, 1 rear)               V 0
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

# ---------------------------------------------------------------- parameters
BODY_LENGTH = 0.20          # metres, mandible tip to gaster tip
JOINT_RES = (10, 6)         # joint balls
SIDES = 8                   # ring resolution of tubes
SPHERE_U, SPHERE_V = 16, 10  # ellipsoid resolution
RENDER_PREVIEWS = True
EXPORT = True

try:
    _ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
except NameError:  # executed via exec() without __file__
    _ROOT = "D:/UnrealProjects/AntTreeGame"
OUT_DIR = os.path.join(_ROOT, "Art", "Blender")

# part ids (UV1.u)
P_BODY, P_HEAD, P_ANT, P_MAND, P_LEG = 0.0, 0.2, 0.4, 0.6, 0.8

# Leg joints for the left side (+Y): hip, knee, ankle, foot.  Neutral mid-stride pose.
LEGS = {
    0: [(0.024, 0.010, 0.042), (0.046, 0.036, 0.068), (0.062, 0.050, 0.026), (0.067, 0.055, 0.0020)],
    1: [(0.000, 0.011, 0.041), (0.004, 0.042, 0.070), (0.010, 0.062, 0.027), (0.012, 0.068, 0.0020)],
    2: [(-0.022, 0.010, 0.042), (-0.036, 0.040, 0.068), (-0.054, 0.056, 0.026), (-0.059, 0.062, 0.0020)],
}
LEG_RADII = [0.0060, 0.0052, 0.0040, 0.0024]
PAIR_U = {0: 0.0, 1: 0.5, 2: 1.0}

# ---------------------------------------------------------------- mesh helpers
bm = None
attrs = {}  # BMVert -> (uv0, uv1, uv2)


def new_vert(p, uv0, uv1, uv2):
    v = bm.verts.new(p)
    attrs[v] = (uv0, uv1, uv2)
    return v


def rot_y(vec, ang):
    c, s = math.cos(ang), math.sin(ang)
    return Vector((c * vec.x + s * vec.z, vec.y, -s * vec.x + c * vec.z))


def ellipsoid(center, radii, uv, pitch=0.0, deform=None, res=None):
    """Closed ellipsoid. pitch rotates about Y (positive tips the front down)."""
    SU, SV = res or (SPHERE_U, SPHERE_V)
    c = Vector(center)
    rings = []

    def place(u):
        # u: unit-sphere point
        if deform:
            u = deform(u)
        q = Vector((u.x * radii[0], u.y * radii[1], u.z * radii[2]))
        return c + rot_y(q, pitch)

    top = new_vert(place(Vector((0, 0, 1))), *uv)
    bot = new_vert(place(Vector((0, 0, -1))), *uv)
    for j in range(1, SV):
        th = math.pi * j / SV
        ring = []
        for i in range(SU):
            ph = 2 * math.pi * i / SU
            u = Vector((math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th)))
            ring.append(new_vert(place(u), *uv))
        rings.append(ring)
    n = SU
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((top, rings[0][j], rings[0][i]))
        bm.faces.new((bot, rings[-1][i], rings[-1][j]))
    for r in range(len(rings) - 1):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((rings[r][i], rings[r][j], rings[r + 1][j], rings[r + 1][i]))


def basis(d):
    d = d.normalized()
    ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
    u = d.cross(ref).normalized()
    w = d.cross(u).normalized()
    return u, w


def tube(p0, p1, r0, r1, uv_a, uv_b, apex=False):
    """Tapered cylinder from p0 to p1. uv_a/uv_b are the (uv0,uv1,uv2) triples at each end.
    apex=True closes the p1 end to a point (cone)."""
    p0, p1 = Vector(p0), Vector(p1)
    u, w = basis(p1 - p0)
    r_a, r_b = [], []
    for i in range(SIDES):
        a = 2 * math.pi * i / SIDES
        off = u * math.cos(a) + w * math.sin(a)
        r_a.append(new_vert(p0 + off * r0, *uv_a))
        if not apex:
            r_b.append(new_vert(p1 + off * r1, *uv_b))
    if apex:
        tip = new_vert(p1, *uv_b)
        for i in range(SIDES):
            j = (i + 1) % SIDES
            bm.faces.new((r_a[i], r_a[j], tip))
    else:
        for i in range(SIDES):
            j = (i + 1) % SIDES
            bm.faces.new((r_a[i], r_a[j], r_b[j], r_b[i]))
    # flat cap on the start end (hidden inside a joint ball or the body)
    c = new_vert(p0, *uv_a)
    for i in range(SIDES):
        j = (i + 1) % SIDES
        bm.faces.new((c, r_a[j], r_a[i]))
    if not apex:
        c2 = new_vert(p1, *uv_b)
        for i in range(SIDES):
            j = (i + 1) % SIDES
            bm.faces.new((c2, r_b[i], r_b[j]))


def chain(points, radii, uvs, ball_scale=1.0):
    """Tubes between successive points with a ball on every joint (rounded, capped ends)."""
    for k in range(len(points) - 1):
        tube(points[k], points[k + 1], radii[k], radii[k + 1], uvs[k], uvs[k + 1])
    for k in range(len(points)):
        r = radii[k] * ball_scale
        ellipsoid(points[k], (r, r, r), uvs[k], res=JOINT_RES)


# ---------------------------------------------------------------- body parts
def heart(u):
    """Heart-shaped head: narrow toward the front (+X), wide and notched at the back."""
    x, y, z = u.x, u.y, u.z
    width = 0.80 + 0.32 * (-x)
    y *= max(width, 0.45)
    if x < 0:
        x += 0.30 * (-x) * math.exp(-((u.y / 0.38) ** 2))  # rear notch
    return Vector((x, y, z))


def uvset(u0, v0, part, side, pair=0.0):
    return ((u0, v0), (part, side), (pair, 0.0))


def build():
    # head ---------------------------------------------------------------
    ellipsoid((0.070, 0, 0.054), (0.024, 0.021, 0.019), uvset(0, 0, P_HEAD, 0), pitch=0.12, deform=heart)
    # thorax: neck, pronotum + mesonotum/propodeum, narrow two-segment waist, gaster
    b = uvset(0, 0, P_BODY, 0)
    ellipsoid((0.017, 0, 0.054), (0.018, 0.015, 0.016), b)
    ellipsoid((-0.010, 0, 0.053), (0.022, 0.012, 0.014), b)
    # neck: the pronotum ends at x 0.035 and the notched back of the head at 0.050..0.053, so without it the head
    # floated in front of the thorax (owner's playtest, 13 Nov). Both ends are buried; nothing else moves, so the
    # scale, the bounds and the mandible attach point stay as they were.
    ellipsoid((0.044, 0, 0.0548), (0.016, 0.0070, 0.0080), b)
    ellipsoid((-0.040, 0, 0.054), (0.0075, 0.0055, 0.0075), b)   # petiole
    ellipsoid((-0.050, 0, 0.056), (0.0075, 0.0060, 0.0080), b)   # postpetiole
    ellipsoid((-0.084, 0, 0.056), (0.032, 0.023, 0.022), b, pitch=-0.10)  # gaster
    # dorsal spines: three pairs on the thorax, pointing up, back and out
    for x, z, ln in ((0.024, 0.066, 0.011), (0.004, 0.065, 0.010), (-0.018, 0.064, 0.011)):
        for sy in (1, -1):
            base = (x, sy * 0.0065, z)
            tip = (x - 0.006, sy * 0.011, z + ln)
            tube(base, tip, 0.0032, 0.0, b, b, apex=True)
    # mandibles (open, tips curve inward) --------------------------------
    for sy, side in ((1, 0.0), (-1, 1.0)):
        pts = [(0.080, sy * 0.009, 0.050), (0.092, sy * 0.016, 0.047),
               (0.104, sy * 0.014, 0.046), (0.113, sy * 0.007, 0.047)]
        rad = [0.0065, 0.0052, 0.0034, 0.0014]
        chain(pts, rad, [uvset(0, 0, P_MAND, side)] * 4)
    # antennae: scape, elbow, funiculus, club -----------------------------
    for sy, side in ((1, 0.0), (-1, 1.0)):
        pts = [(0.082, sy * 0.008, 0.064), (0.098, sy * 0.016, 0.083),
               (0.113, sy * 0.026, 0.078), (0.121, sy * 0.034, 0.066)]
        rad = [0.0022, 0.0018, 0.0015, 0.0027]
        chain(pts, rad, [uvset(0, 0, P_ANT, side)] * 4)
    # legs ---------------------------------------------------------------
    for pair, pts in LEGS.items():
        for sy, side in ((1, 0.0), (-1, 1.0)):
            tripod = 0.25 if (pair + int(side)) % 2 == 0 else 0.75
            P = [(x, sy * y, z) for x, y, z in pts]
            lengths = [(Vector(P[i + 1]) - Vector(P[i])).length for i in range(3)]
            total = sum(lengths)
            vs, acc = [0.0], 0.0
            for ln in lengths:
                acc += ln
                vs.append(acc / total)
            uvs = [uvset(tripod, v, P_LEG, side, PAIR_U[pair]) for v in vs]
            chain(P, LEG_RADII, uvs, ball_scale=1.12)
            # coxa ball at the hip (body-side end of the leg, v = 0)
            ellipsoid(P[0], (0.0075, 0.0075, 0.0075), uvs[0], res=JOINT_RES)


def make_object():
    global bm
    bm = bmesh.new()
    attrs.clear()
    build()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for f in bm.faces:
        f.smooth = True

    # transform: scale to length, drop feet to Z=0
    xs = [v.co.x for v in bm.verts]
    s = BODY_LENGTH / (max(xs) - min(xs))
    mand_tips = [Vector((0.113, sy * 0.007, 0.047)) for sy in (1, -1)]
    attach = sum(mand_tips, Vector()) / 2
    # bring the socket slightly inside the tips so a carried fragment sits between them
    attach.x -= 0.004
    minz = min(v.co.z for v in bm.verts) * s
    for v in bm.verts:
        v.co = v.co * s
        v.co.z -= minz
    attach = attach * s
    attach.z -= minz

    me = bpy.data.meshes.new("SM_Ant")
    layers = [bm.loops.layers.uv.new(n) for n in ("UV0", "UV1", "UV2")]
    for f in bm.faces:
        for lp in f.loops:
            for k, layer in enumerate(layers):
                lp[layer].uv = attrs[lp.vert][k]
    bm.to_mesh(me)
    tri = len(me.polygons)
    bm.free()

    mat = bpy.data.materials.new("M_Ant")
    mat.diffuse_color = (0.30, 0.10, 0.04, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (0.30, 0.10, 0.04, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.45
    me.materials.append(mat)
    ob = bpy.data.objects.new("SM_Ant", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, tri, attach


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


def render_previews(path_a, path_b):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1000, 700
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
    lo.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    cd = bpy.data.cameras.new("PreviewCam")
    cd.lens = 70
    co = bpy.data.objects.new("PreviewCam", cd)
    sc.collection.objects.link(co)
    sc.camera = co
    tgt = (0.0, 0.0, 0.045)
    for loc, path in (((0.42, -0.42, 0.28), path_a), ((0.0, -0.65, 0.05), path_b)):
        co.location = loc
        look_at(co, tgt)
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
    for o in (co, lo):
        bpy.data.objects.remove(o, do_unlink=True)


def export_fbx(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                             add_leaf_bones=False, bake_anim=False, path_mode="STRIP")


def main():
    clear_scene()
    ob, tri, attach = make_object()
    bb = [tuple(round(c, 5) for c in (min(v.co[i] for v in ob.data.vertices),
                                      max(v.co[i] for v in ob.data.vertices))) for i in range(3)]
    info = {"triangles": tri, "bbox_min_max_xyz": bb, "mandible_attach": tuple(round(c, 5) for c in attach)}
    if EXPORT:
        os.makedirs(OUT_DIR, exist_ok=True)
        if RENDER_PREVIEWS:
            render_previews(os.path.join(OUT_DIR, "ant_preview_3q.png"), os.path.join(OUT_DIR, "ant_preview_side.png"))
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "Ant.blend"))
        export_fbx(ob, os.path.join(OUT_DIR, "SM_Ant.fbx"))
    print("make_ant:", info)
    return info


result = main()
