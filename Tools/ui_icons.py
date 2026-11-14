#!/usr/bin/env python3
"""HUD status icons for Leafline's icon HUD (Leaf.HudIcons): white silhouettes on transparent, tinted by the game.

python3 Tools/ui_icons.py            # writes Content/Leafline/UIRaw/Icon_*.pngraw (64 x 64)
Drawn here from simple shapes (made for the game on 14 Nov 2026; no third-party art, no text). Needs Pillow.
Each icon is drawn at 512 px and scaled down, so the edges are smooth.
"""
import math
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Leafline", "UIRaw")
N = 64   # final size: like the prompt icons, so they stay clean when drawn small (no mipmaps at runtime)
SS = 4   # drawing units (0..128) to supersampled pixels
W = 128 * SS
WHITE = (255, 255, 255, 255)


def canvas():
    im = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def s(*pts):
    """Points in 0..128 icon units to supersampled pixels."""
    return [(x * SS, y * SS) for x, y in pts]


def save(im, name):
    im = im.resize((N, N), Image.LANCZOS)
    im.save(os.path.join(OUT, name + ".pngraw"), format="PNG")
    print("wrote", name)


def thick_polyline(d, pts, width):
    d.line(s(*pts), fill=WHITE, width=int(width * SS), joint="curve")
    r = width * SS / 2
    for x, y in s(*pts):
        d.ellipse((x - r, y - r, x + r, y + r), fill=WHITE)


def bezier(p0, p1, p2, n=24):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in (i / n for i in range(n + 1))]


def bird_glide():
    # A gliding bird seen from below: long wings in a shallow M, small body and tail.
    im, d = canvas()
    left = bezier((64, 62), (38, 30), (4, 54)) + bezier((4, 54), (34, 56), (64, 82))[1:]
    right = [(128 - x, y) for x, y in left]
    d.polygon(s(*left), fill=WHITE)
    d.polygon(s(*right), fill=WHITE)
    d.ellipse(s((55, 52), (73, 90)), fill=WHITE)            # body
    d.polygon(s((57, 84), (71, 84), (76, 102), (52, 102)), fill=WHITE)  # tail
    d.ellipse(s((57, 44), (71, 60)), fill=WHITE)            # head
    save(im, "Icon_BirdGlide")


def bird_dive():
    # A stooping bird head-down: body pointing down, wings swept back and up in a narrow V.
    im, d = canvas()
    d.polygon(s((58, 62), (20, 10), (34, 8), (64, 52), (94, 8), (108, 10), (70, 62), (64, 78)), fill=WHITE)  # wings
    d.ellipse(s((53, 40), (75, 104)), fill=WHITE)           # body
    d.polygon(s((56, 104), (72, 104), (64, 122)), fill=WHITE)  # head and beak, at the bottom
    d.polygon(s((57, 44), (71, 44), (74, 24), (54, 24)), fill=WHITE)  # tail, at the top
    save(im, "Icon_BirdDive")


def leaf():
    # A leaf blade with a stalk, tip to the upper right; midrib and veins cut out.
    im, d = canvas()
    a, b = (22, 106), (110, 18)
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    length = math.dist(a, b)
    ux, uy = math.cos(ang), math.sin(ang)
    vx, vy = -uy, ux
    pts = []
    for i in range(41):
        t = i / 40
        w = 30 * math.sin(math.pi * t) ** 0.8 * (1 - 0.25 * t)
        pts.append((a[0] + ux * length * t + vx * w, a[1] + uy * length * t + vy * w))
    for i in range(40, -1, -1):
        t = i / 40
        w = 30 * math.sin(math.pi * t) ** 0.8 * (1 - 0.25 * t)
        pts.append((a[0] + ux * length * t - vx * w, a[1] + uy * length * t - vy * w))
    d.polygon(s(*pts), fill=WHITE)
    thick_polyline(d, [(8, 120), (24, 104)], 7)             # stalk
    clear = (0, 0, 0, 0)
    d.line(s((26, 102), (100, 28)), fill=clear, width=4 * SS)  # midrib
    for t in (0.3, 0.5, 0.7):
        cx, cy = a[0] + ux * length * t, a[1] + uy * length * t
        for sgn in (1, -1):
            ex = cx + (ux * 0.55 + vx * sgn) * 16
            ey = cy + (uy * 0.55 + vy * sgn) * 16
            d.line(s((cx, cy), (ex, ey)), fill=clear, width=3 * SS)
    save(im, "Icon_Leaf")


def grip():
    # Three hooked claws closing over a bar of bark.
    im, d = canvas()
    d.rounded_rectangle(s((8, 92), (120, 112)), radius=8 * SS, fill=WHITE)
    for cx in (34, 64, 94):
        pts = bezier((cx - 6, 18), (cx + 22, 40), (cx + 4, 90))
        thick_polyline(d, pts, 11)
        tip = bezier((cx + 4, 90), (cx - 2, 96), (cx - 10, 88))
        thick_polyline(d, tip, 8)
    save(im, "Icon_Grip")


def ant_dot(d, cx, cy, r):
    d.ellipse(s((cx - r, cy - r), (cx + r, cy + r)), fill=WHITE)
    d.ellipse(s((cx - r * 1.5 - 4, cy - r * 0.6), (cx - r * 0.5, cy + r * 0.6)), fill=WHITE)  # head side


def wind_lines(d, x0, x1, ys, width=8):
    for y in ys:
        thick_polyline(d, bezier((x0, y), ((x0 + x1) / 2, y - 6), (x1, y)), width)


def exposure():
    # The ant (a dot) and the wind (lines from the left); a wall shelters it.
    im, d = canvas()
    d.rounded_rectangle(s((50, 12), (66, 116)), radius=6 * SS, fill=WHITE)  # full wall
    wind_lines(d, 6, 40, (40, 64, 88))
    ant_dot(d, 96, 64, 16)
    save(im, "Icon_Sheltered")

    im, d = canvas()
    d.rounded_rectangle(s((50, 64), (66, 116)), radius=6 * SS, fill=WHITE)  # half wall
    wind_lines(d, 6, 40, (76, 98))
    thick_polyline(d, bezier((6, 36), (50, 24), (80, 48)), 8)  # one line over the top
    ant_dot(d, 98, 78, 15)
    save(im, "Icon_Partly")

    im, d = canvas()
    wind_lines(d, 6, 70, (36, 64, 92))
    ant_dot(d, 100, 64, 16)
    save(im, "Icon_Exposed")


def leaf_poly(a, b, width):
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    length = math.dist(a, b)
    ux, uy = math.cos(ang), math.sin(ang)
    vx, vy = -uy, ux
    side = lambda sgn, t: (a[0] + ux * length * t + sgn * vx * width * math.sin(math.pi * t) ** 0.8 * (1 - 0.25 * t),
                           a[1] + uy * length * t + sgn * vy * width * math.sin(math.pi * t) ** 0.8 * (1 - 0.25 * t))
    return [side(1, i / 30) for i in range(31)] + [side(-1, i / 30) for i in range(30, -1, -1)]


def leaf_pile():
    # Three leaf blades heaped on a low mound: the colony's haul.
    im, d = canvas()
    clear = (0, 0, 0, 0)
    d.rounded_rectangle(s((10, 100), (118, 118)), radius=8 * SS, fill=WHITE)  # the heap's base
    for a, b, w in (((24, 104), (70, 30), 18), ((60, 106), (112, 44), 18), ((14, 98), (84, 62), 17)):
        poly = s(*leaf_poly(a, b, w))
        d.line(poly + poly[:1], fill=clear, width=7 * SS)   # a gap around each blade, so they read as separate
        d.polygon(poly, fill=WHITE)
        d.line(s(a, ((a[0] * 0.2 + b[0] * 0.8), (a[1] * 0.2 + b[1] * 0.8))), fill=clear, width=3 * SS)  # midrib
    save(im, "Icon_LeafPile")


def ant():
    # A worker seen from above, head up (heading up the tree): head, thorax, abdomen, six legs, antennae.
    im, d = canvas()
    for side in (1, -1):
        for (sx, sy), (kx, ky), (ex, ey) in (((64, 44), (86, 30), (100, 18)), ((64, 50), (90, 52), (106, 58)),
                                             ((64, 56), (86, 72), (98, 92))):
            f = lambda x: 64 + side * (x - 64)
            thick_polyline(d, [(f(sx), sy), (f(kx), ky), (f(ex), ey)], 7)
        thick_polyline(d, [(64 + side * 6, 18), (64 + side * 16, 6), (64 + side * 26, 8)], 5)  # antenna
    d.ellipse(s((52, 12), (76, 34)), fill=WHITE)    # head
    d.ellipse(s((55, 34), (73, 62)), fill=WHITE)    # thorax
    d.ellipse(s((60, 60), (68, 70)), fill=WHITE)    # waist
    d.ellipse(s((48, 66), (80, 118)), fill=WHITE)   # abdomen
    save(im, "Icon_Ant")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    bird_glide()
    bird_dive()
    leaf()
    grip()
    exposure()
    leaf_pile()
    ant()
