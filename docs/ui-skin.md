# UI skin: panel and button images for Leafline

Leafline draws its menus and HUD in code. Every panel background and menu button can instead come from an image you
paint in any program. Each image is optional: anything missing keeps today's flat dark panel, so you can deliver one
image at a time.

## The images

Paint everything at **twice the size it appears on a 1920x1080 screen**. It is then pixel-exact at 4K and scaled down
cleanly below that (1280x720 shows it at one third).

| File | Used for | Image size | Corner margins (max) | Smallest it is drawn (at 1080p) |
|---|---|---|---|---|
| `Panel_Card.png` | the big screens: title, pause, options, controls, credits, end of day, nest, new tree, end card | 512 x 512 | 64 px | 1152 x 292 |
| `Panel_Group.png` | HUD boxes: day and workers (top left), colony (top centre), wind (top right), line rows, stamina, banners | 256 x 256 | 32 px | about 200 x 60 |
| `Panel_Tag.png` | one-line hints and prompts, cut labels, the "+N" when a piece arrives | 128 x 128 | 24 px | about 70 x 52 |
| `Button_Selected.png` | the selected or hovered menu row | 512 x 128 | 32 px | 800 x 64 |
| `Button_Idle.png` | the other menu rows (today they have no background, so this one is extra) | 512 x 128 | 32 px | 800 x 64 |

The sizes are what we ask for; other sizes work as long as the margins fit.

## How an image is stretched (nine slices)

Each image is cut into nine pieces by its four margins (left, top, right, bottom, in image pixels):

- The four **corners** are drawn at their painted size: a 64 px corner is 32 px on a 1080p screen, 64 px at 4K.
- The **edges** stretch along their length only.
- The **centre** stretches in both directions.

So put rounded corners and ornaments in the corners, keep the edges a pattern that survives stretching (plain, or a
line), and keep the centre plain. A Card is stretched to more than twice its width, so a centre with texture or
gradients will smear.

## Safe areas and readability

- Text starts **16 px inside the panel edge at 1080p**, which is 32 px in your image. Keep borders and ornaments
  inside that outer 32 px band (24 px for Tag), or text will sit on them.
- Text is near-white cream (about `#F0EFE5`) with light cyan headings, drawn on top of your image. The centre must be
  dark and calm enough to read it over the game's bright bark and leaves; today's panel is a near-black green at 72%
  opacity (it shows as `#1C2320` over black).
- Transparency is welcome (corners, a translucent centre). Fading panels (banners, the "+N") fade your image with them.
- Avoid hairlines: at 1280x720 the image is shown at a third of its size, so keep lines at least 4 px thick in your image.
- No text, logos or trademarks in the images.
- Menu rows sit 12 px apart at 1080p and the selected row is drawn on top of the card, so the button images
  should read clearly against `Panel_Card`.

## File format

- PNG, 8-bit RGBA, sRGB, no other formats.
- Name the files exactly as in the table. In the game's repository they are renamed to `.pngraw` (so the editor does
  not import them); for testing either ending works.
- Margins go in a text file `skin.txt` in the same folder, one line per image, in image pixels:

```
; my skin (lines starting with ; are notes)
scale 2
Panel_Card 64 64 64 64
Panel_Group 32 32 32 32
Panel_Tag 24 24 24 24
Button_Selected 32 32 32 32
Button_Idle 32 32 32 32
```

The four numbers are left, top, right, bottom. `scale 2` means "painted at twice the 1080p size"; leave it at 2.

## Trying it in the game

1. In the game's folder, create `AntTreeGame/Saved/Skin/` and put your PNGs and `skin.txt` there.
2. Start the game. Your images are used wherever they are valid.
3. While playing, open the console (the engine's Tilde binding: `` ` `` / `~` on a US or UK keyboard; on other
   layouts it is the key in that position, ask the owner if unsure) and type:
   - `Leaf.ReloadSkin` to read your files again after you changed them (no restart needed);
   - `Leaf.Skin 0` and `Leaf.Skin 1` to compare with the flat panels;
   - `Leaf.PauseMenu 0`, `Leaf.PauseMenu 1`, `Leaf.PauseMenu 2` and `Leaf.PauseMenu 3` to see the pause menu, options,
     credits and controls;
   - `HighResShot 1` to save a screenshot in `AntTreeGame/Saved/Screenshots/`.
4. If an image does not show, `AntTreeGame/Saved/Logs/AntTreeGame.log` has one line starting with `Skin`: either
   `Skin: none` (no `skin.txt` was found; check the folder name) or `Skin from <folder>` listing, per image, what was
   used or why it fell back to the flat panel (no image, not a readable PNG, no margins line, margins that do not
   parse, or margins that do not fit the image).

## Licence and credit

The game's repository becomes **public** after the jam, and the images go into it. Before anything is added, please
confirm to the owner in writing that:

- the images are made new for this game, on or after 12 November 2026 (a jam rule: nothing older, including your own
  earlier work, can go in);
- they are entirely your own work, with no third-party material such as stock images, fonts or brushes with
  restrictions;
- you agree to them being published in that repository under the licence the owner chooses for the game's assets;
- you tell the owner whether you used AI tools for any part, so it can be disclosed with the entry.

You will be credited in the game's credits and on the game's pages under "UI art", with the name you choose.

## For developers

`Source/AntTreeGame/UI/Skin.*` loads the skin and `ALeaflineHUD::DrawPanel` / `DrawNineSlice` / `DrawButtonBack` draw
it. The folder searched is the one given to `Leaf.ReloadSkin <folder>`, else `Saved/Skin`, else
`Content/Leafline/UIRaw` (shipped files go there, staged with UIRaw). With no `skin.txt` anywhere the HUD is
pixel-identical to the flat UI (checked by a before/after screenshot diff over `Leaf.NoWorld 1`, 14 Nov).
`Tools/ui_test_skin.py <folder> [broken]` writes a placeholder skin (procedural frames, never shipped) for testing.
