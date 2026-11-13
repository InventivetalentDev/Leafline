# Brief: look pass (visual polish for Sunday)

For the main session. Written Fri 13 Nov, 16:00. It is based on Codex's art review of three owner screenshots (`Saved/Screenshots/Haylee/04f1416932…`, `44198f2537…`, `da2e0b8bca…`, called shots 1, 2 and 3 below), checked against the code by the planning session.

The jam's judges mostly see a 30 to 60 second video. The parts below are ordered by how much of that video they touch.

This brief is the specification. When the work lands, update `tech.md`, `plan.md` (a milestone "M5d: Look pass"; it replaces the open "Lighting pass" item in M5) and `devlog.md` yourself.

## Before you start

- Tag the current commit `pre-look`.
- Keep this work in its own commits, with messages starting `Look:`.
- **The three screenshots are from 11:33 today.** The smoke trail (12:52, 13:33) and the new controls came after. Retake the views on the current build before acting on anything said about the trail.
- **Nothing here changes tree geometry, the walking field, or `LeafSet`.** Saved trees depend on the leaf count.
- **Owner's verdict** on parts 1 to 4: Friday night or Saturday morning, from a package. Stills cannot judge the cutaway or the camera; only the owner playing can.

## What colour already means

Do not blur any of these while making things prettier:

| Signal | Meaning |
|---|---|
| Pale young green leaf | Rich (up to 3×) |
| Brown leaf, or stalk only | Cannot be cut any more |
| Trail brightness | Patch remaining on that line |
| Green / amber / red on the HUD and the wind map | Sheltered / partly exposed / exposed. The two must stay identical. |
| Cut arc colour | Wind risk of the piece |

## Part 0: views to judge by

Add views to `Tools/ref_shots.sh` and take a "before" set labelled `look0`:

- `junction`: the ant on top of a limb of 60 to 110 cm radius, near where it leaves the trunk, with the camera pitched up about 45°. This should reproduce the black wedges of shots 2 and 3. If it does not, find the pose that does and use that.
- `sunward`: on the sun-facing side of the trunk, looking toward the sun, in whichever shift reproduces shot 1. This should reproduce the amber wash of shot 1.
- Keep `trunk`, `canopy` and `base`.

Every part below ends with the same set under a new label, placed beside `look0`.

## Part 1: lighting and haze (tuning only, do it first)

It needs no rebuild (`Leaf.Tune look`, `LOOK=` in `ref_shots.sh`), so it is the cheapest part.

Targets:

- In `sunward`, bark and the ant within about 10 m keep their own colour. Haze separates the distant grove and the ground far below, not the surface the ant is on. Candidates: `FogStartDistance`, `VolumetricFogScattering`, `SunVolumetricScattering`, `FogSunGlowColor` and its exponent.
- Shadowed bark has a cool fill and is never near-black. Candidates: sky light intensity and tint.
- The warm sun stays. The leaf shadows on the trunk in shot 2 are the look to keep.
- In shots 2 and 3 the sky is storm-dark while the bark is in full sun. Check whether that is intended for that shift or an exposure side effect, and say which.
- Depth of field stays mild: the route ahead must be readable.

## Part 2: the cutaway and the camera

**Symptom** (shots 2 and 3): a large black wedge under the ant, a hard-edged view into the inside of the limb, and a strip of bark left hanging between two holes. In shot 3 a lit, bark-covered stub with a rounded end also shows inside the wedge. It looks like the base of a child branch buried in its parent.

**What the code says.** `PitchMax` is 60°, so looking up puts the camera below the surface the ant stands on. `AntCamera.cpp` (around line 317) moves the camera out of wood only when the branch radius is at least `ThickWoodRadius` (120 cm). Inside a thinner limb it stays inside. The 35 cm near-camera fade and the cutaway then open the limb from within, and what shows is its far inner wall. Shadows ignore the cutaway (`ShadowReplace`), so that wall gets no direct light.

That is a likely cause, not a confirmed one. Reproduce it first. Then, before any material work, lower `ThickWoodRadius` with `Leaf.Tune` at the `junction` pose and see whether the wedge goes. That one number may settle the cause.

**Goal.** Looking up stays possible and gets better. Do not fix this by limiting the pitch: the open playtest note asks for more looking up, and concept image 01 is a look-up shot.

In this order:

1. **Never show the inside of a branch.** Through the hole the player should see the world behind the limb, not its interior. If the bark is two-sided, mask its back faces out; if it can be one-sided, make it so. Check that the tree still casts its shadows. Masking back faces will not hide the buried base of a child branch, because that faces the camera: check for such geometry inside parents and hide it too (for example by fading any bark that lies inside another branch).
2. **Soften the edge.** A wider dither band and less contrast at the rim. Keep the bark right under the ant.
3. **Tune live.** Move the cutaway's distances (near-camera fade, band width, narrowing toward the ant) into `MPC_View` and `Leaf.Tune`, so later tuning needs no material rebuild.
4. **Look-up assist, behind a flag, only if 1 to 3 are done and time allows.** When pitching up would put the camera under the ant's own surface, keep the camera just above that surface and shorten the boom, and turn the view upward from there. The ant slides toward the bottom of the frame and the canopy fills the rest. This would also close the open playtest note about looking up from the trunk. Off by default until the owner has tried it.

**Framing.** The ant is about 3% of the screen width in the shots (`BoomLength` 220, field of view 75°, `DefaultPitch` −20°). Try a shorter boom and a slightly higher default angle. The new controls read the camera, and the owner approved them in the 15:37 package, so these stay tuning values with their own verdict.

## Part 3: bark

**One material rebuild for parts 2 and 3**, under a new name per the material rules in `CLAUDE.md`. The cutaway lives in `M_TreeBark`, so do both in the same graph. Carry over the cutaway, `ShadowReplace`, the cylinder coordinates and the moss. Keep `M_TreeBark` as the fallback with a console switch, registered before a full build.

**Symptom.** The fissures are thin, near-black, looping outlines on smooth single-colour wood; they read as drawn on (shots 2 and 3). From 30 cm they stretch into long black bands (shot 1). Moss lies in horizontal bands round the trunk (shot 2).

Targets:

- **No black lines.** A fissure is darker wood with a soft edge, not an outline.
- **Plates.** Each plate has a slightly different tone, and plates are longer along the branch than across it.
- **Relief from the normal only.** Perturb the normal from the same height value that makes the fissures, so the sun catches plate edges. No displacement and no change to the mesh or the field.
- **Fine grain up close**, faded out with distance so it does not shimmer.
- **Roughness varies**: plates, fissures and moss differ.
- **Moss in patches and creases**, not rings.
- It has to hold at three distances: 30 cm (shot 1), 2 m, and 30 m.

**Then re-check part 1.** New bark tone and roughness shift the lighting; run its targets again.

**Cost.** Bark fills most frames. Re-run `Tools/perf_packaged.sh` (trunk and canopy) and compare with the table in `tech.md`, on Epic and on Low. Stay within half a millisecond of GPU time on this machine; if a noise is too expensive, drop the fine grain first.

## Part 4: HUD (C++ only)

It touches only `UI/LeaflineHUD.cpp`, so Codex can do it in a worktree while the main session holds the editor. Keep the canvas drawing; no Slate. The HUD has no off switch, so give its commits their own prefix (`HUD:`); a rejected HUD can then be reverted by itself.

Start with one small style table (two text sizes, margins, bar thickness, panel colour, the existing signal colours) and make every draw call use it. Then:

- **Text size.** The smallest text ("Hauled 0.0", "Here: sheltered") is about 1.2% of the screen height: 13 pixels in a 1080p video. Set a minimum for secondary text of about 1.8% and use a clearly larger primary size.
- **Backings.** One translucent dark panel behind each group, with the same margin everywhere.
- **Top left:** day and tree name, worker pips, hauled and carried load, as one group. The pips should show each worker's state, using the states the roster already has, not just three squares.
- **Top right:** wind arrow, gust bar and "Here: …" as one group. Keep its three colours exactly.
- **Stamina:** the bare bar at the bottom of shot 3 has no label and no anchor. Give it an icon or a word, and hide it while full.
- **Line rows, patch read-out, prompts and banners** use the same panel and sizes.
- **Second step, first to cut:** the same treatment on the title, recap, nest, move card, end card and pause.
- **Font:** keep the engine font. A new font needs the owner's yes, must be CC0, and goes in `docs/sourced-assets.md`.

Check in screenshots at 1280×720 and 3840×2160, with keyboard and with pad icons.

## Part 5: only if time remains

- **Trail.** Judge the current smoke trail from the retaken views first. If the trail still fills the frame as a wide band when the camera is centimetres from it (shot 3), fade it near the camera.
- **Leaves.** Softer backlight, and a little variation in tone between leaves. The variation must stay clearly smaller than the pale-to-deep range that shows richness, and brown must stay unmistakable.
- **Framing foliage** belongs on the distant grove only. Leaves on the play tree that cannot be cut would mislead the player.

## Order, time boxes and what to cut

| When | Work |
|---|---|
| Fri, now | Part 0, then part 1 |
| Fri evening | Parts 2 and 3 in one material rebuild (main session). Part 4 in parallel (Codex), reviewed and merged. |
| Fri night | Package for the owner |
| Sat | Fixes from the owner's verdict, HUD style in the menus, part 5 if time |
| Sat 23:00 | Freeze. No visual additions after this. |

Everything that needs Unreal runs one at a time behind the lock.

Cut in this order, first to go first: part 5, the HUD style in the menus, the look-up assist, the bark's fine grain.

## Acceptance tests

1. Before and after sets of the five views, side by side, for each part.
2. `junction`, and a pitch sweep at that spot from −80° to 60° in five steps: no black area and no view of the inside of a limb in any shot. The same sweep on a bough of 20 to 40 cm radius.
3. `sunward`: the ant and the bark under it are clearly distinct from the haze.
4. Frame times after the bark change, on Epic and on Low, against the `tech.md` table.
5. One shot with rich, plain and brown leaves together, and one with the wind map up: every signal in "What colour already means" still reads.
6. HUD screenshots at both resolutions and both input modes; no text below the minimum size.
7. `Leaf.MeshGap` unchanged, as proof that no geometry moved.
8. With one tree on, a save from before this work loads with its bites.
9. Packaged smoke test as usual, with ray tracing and with `-noraytracing`.

## Not in this work

- Bark displacement, or any change to the tree generator, the field or `LeafSet`.
- Slate screens or a UI framework.
- New creatures, particle systems, new sourced assets.
- A lower pitch limit.
