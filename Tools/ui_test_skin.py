#!/usr/bin/env python3
"""Throwaway placeholder skin for testing the HUD's skin hook (never shipped, never in Content).

ui_test_skin.py <out-folder> [broken]
Writes Panel_Card/Group/Tag, Button_Selected/Idle as .pngraw (simple procedural frames, painted at 2x the 1080p size)
and skin.txt. With 'broken': Panel_Tag missing, Panel_Group margins too large, Button_Idle not a PNG, Card fine.
"""
import os
import sys
from PIL import Image, ImageDraw

out = sys.argv[1]
if "Content" in os.path.normpath(out).split(os.sep):
    sys.exit("refusing to write a test skin under Content/: use Saved/ or a scratch folder")
broken = len(sys.argv) > 2 and sys.argv[2] == "broken"
os.makedirs(out, exist_ok=True)
# Only this script's own files are replaced; never point it at Content (it is for Saved/ or a scratch folder).
for f in ("Panel_Card", "Panel_Group", "Panel_Tag", "Button_Selected", "Button_Idle"):
    for ext in (".pngraw", ".png"):
        if os.path.exists(os.path.join(out, f + ext)):
            os.remove(os.path.join(out, f + ext))


def frame(w, h, radius, border, fill, edge, inset=0, notch=False):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((inset, inset, w - 1 - inset, h - 1 - inset), radius=radius, fill=fill, outline=edge, width=border)
    if notch:  # a corner mark, so the nine slices are easy to check in screenshots
        r = radius // 2
        for cx, cy in ((radius, radius), (w - 1 - radius, radius), (radius, h - 1 - radius), (w - 1 - radius, h - 1 - radius)):
            d.ellipse((cx - r // 2, cy - r // 2, cx + r // 2, cy + r // 2), fill=edge)
    return im


images = {
    "Panel_Card": frame(512, 512, 48, 6, (18, 30, 16, 215), (214, 168, 74, 255), inset=4, notch=True),
    "Panel_Group": frame(256, 256, 24, 4, (14, 24, 20, 200), (120, 200, 210, 255), inset=2),
    "Panel_Tag": frame(128, 128, 20, 4, (10, 16, 12, 205), (190, 220, 120, 255), inset=2),
    "Button_Selected": frame(512, 128, 28, 5, (70, 120, 120, 170), (240, 210, 120, 255), inset=2),
    "Button_Idle": frame(512, 128, 28, 3, (0, 0, 0, 40), (200, 200, 190, 110), inset=2),
}
margins = {"Panel_Card": 64, "Panel_Group": 32, "Panel_Tag": 24, "Button_Selected": 32, "Button_Idle": 32}

for name, im in images.items():
    if broken and name == "Panel_Tag":
        continue
    path = os.path.join(out, name + ".pngraw")
    if broken and name == "Button_Idle":
        open(path, "wb").write(b"not a png at all")
        continue
    im.save(path, format="PNG")

lines = ["; placeholder test skin (ui_test_skin.py)", "scale 2"]
for name, m in margins.items():
    if broken and name == "Panel_Group":
        lines.append(f"{name} 200 200 200 200")  # wider than the 256 px image
    else:
        lines.append(f"{name} {m} {m} {m} {m}")
open(os.path.join(out, "skin.txt"), "w", newline="\r\n").write("\n".join(lines) + "\n")
print("wrote", out, "(broken)" if broken else "")
