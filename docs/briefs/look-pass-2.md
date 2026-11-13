# Brief: look pass, round 2 (last art round before the freeze)

For the main session. Written Fri 13 Nov, 18:45. It follows `look-pass.md` and Codex's second review, of the six pairs in `Saved/RefShots/concept-compare/`. The planning session checked each point against the shots and the code.

Three items, in this order, then stop. Lighting, the canopy and the HUD are accepted as they are.

## Before you start

- **Finish and commit what is in the working tree first** (collars, the flare-free bark radius, the buried-distance channel). This brief was written while that work was uncommitted, and some of what Codex saw may already be fixed by it.
- **Then retake the six concept shots** with `concept_shots.sh` and judge the items below from the new set.
- Commits keep the `Look:` prefix.
- **Collars move the mesh.** Tests 7 and 8 of `look-pass.md` (`Leaf.MeshGap`, and a pre-look save loading with its bites) passed before the collars. Run both again.

## 1. The cutaway opens when nothing is in the way

**Symptom** (`02-gathering-and-scent-return-game.png`): a dark teardrop on the bark behind the ant, and a hard curved edge with a dark area at the right. The ant is in plain view on top of a limb; nothing hides it.

**Cause of the teardrop, from `Tools/materials/bark2_cut.hlsl`.** The shader removes every piece of bark within the cutaway radius of the line from the camera to the ant. It never asks whether wood is between them. With the camera low over the surface the ant stands on, that line runs close above the bark, and the shader digs a trench under it that narrows to the ant. Part 1 of the look pass found the same trench and moved the camera away from it (`WoodClearance 60`); on a limb that rises toward the trunk the trench comes back.

**Check first, without a rebuild,** at the gathering pose:

- `CutawayRadius 0`: does the teardrop go?
- Near fade off: does the edge at the right go?

These are probably two defects. The cause of the second is not known; do not assume it.

**Fix.** Open the cutaway only when wood really hides the ant.

- Each frame, test the line from the camera to the ant against the tree field. The camera already has an occlusion probe; reuse it if it fits.
- Scale the cutaway radius by how deep the line goes into wood: zero when the line is clear, full at some tens of centimetres. Ease it over a fraction of a second, with some hysteresis, so the hole does not flicker while walking.
- **Scale the radius, not `CutawayOn`.** In the shader, `On` also switches the near-camera fade, which has to keep working for wood beside the camera.
- **Leaves share `CutawayOn`** (`M_Leaf` reads `MPC_View`). If leaves are not in the field, a wood-only test would stop them fading when they hide the ant. Give the bark its own factor and leave the leaves as they are.

When that is in, `WoodClearance 60` may no longer be needed. `plan.md` notes that it costs sky when looking up from the trunk (19% of the upper frame instead of 49%). Try a smaller value again and report.

## 2. The nest

It is the first frame of the game and of every day (`holes-nest2-base-rt.png` is that frame with the HUD), and the harvest camera sits there.

**Symptom.** A flattened sphere with a flat dark disc lying on top; both are perfect circles. The mound has the ground's material, so it reads as a bump in the lawn. The ground has large, saturated orange blobs. Behind it the root flare is dark cobblestone, unlike the long ridges higher up.

**Constraints.** `Colony/NestActor.cpp` builds the mound from a scaled engine sphere and a disc. The mound is in the walking field and ants start standing on the entrance, so **everything here is visual only**: nothing an ant can visibly float above or sink into. The nest stays static; a nest that grows with the colony is a Game Off idea.

Targets:

- **The entrance reads as a hole.** Smaller, with an irregular outline, darkening to black toward the middle, not a uniform disc.
- **A lip of dug soil** round the entrance: a ring of small procedural clumps or pellets, low enough that an ant walking over them looks right.
- **The mound is its own material:** fresh soil, darker and grainier than the ground, with an uneven edge where it meets it. Clumps along the edge break the circle.
- **A handful of leaf scraps** on and beside the mound, in the leaf green. They are what the colony carries in.
- **Ground:** fewer, smaller, duller litter spots (ochre to brown, not orange), and some slow variation in tone over metres.
- **Root flare:** check the retaken shot first; the flare-free bark radius may already have fixed the cobblestones. If not, lower their contrast.
- **The sharp root wedge** is tree geometry. Leave the generator alone. If soil banked against it hides the knife edge cheaply, do that; otherwise leave it.

## 3. One material adjustment, one hour at most

**Bark.**

- **Triangular seams** (`01-bark-plates-fissures-moss-game.png`; already listed in `plan.md` as fold-like triangles). Softening hides them; find them instead. The along coordinate is already split, so precision of that one is not the cause. Show each interpolated input of the bark node as a colour at that pose (UV1, UV2, UV3, normal, tangent) and see which one follows the triangles.
- **Grooves read as carved wood or folded cloth:** long, smooth, evenly spaced. Reduce the furrow depth a little and break the ridges along their length, so plates end.
- **Moss is too close to leaf green in sunlight** (the lime patches beside the carried piece in the gathering shot). Leaf colour is information. Make moss clearly darker and duller than any leaf.

**Leaves.**

- The carried piece is a flat lime sheet without veins in the gathering shot, and has veins in the gust shot. Both use `Leaf.Carry`. Take one shot of a piece from a real cut before changing anything, then give the carried piece the veins and a little tonal variation.
- Keep any variation between leaves well inside the pale-to-deep range that shows richness.

## Then stop

After these three, package and hand over. No further rounds on still images: the open questions (the camera and the cutaway in motion, the framing, the look-up assist, the HUD in play) can only be answered by the owner playing. The best next thing to review is a 30 second test recording, which is also what the judges will see.

Cut order if time runs short: item 3 first, then the ground and root parts of item 2. Item 1 stays.

## Acceptance tests

1. The six concept shots retaken, beside the current set.
2. Gathering pose: no hole and no hard edge with the ant in plain view.
3. A 20 second walk along that limb with a carried piece, with the bark's cutaway factor in the log: it is zero whenever the line to the ant is clear.
4. The `junction` and `bough` pitch sweeps again: the cutaway still opens when wood hides the ant, with no black area.
5. A canopy pose with a leaf between camera and ant: the leaf still fades.
6. The camera pushed through a thin branch: the near fade still works.
7. The nest with the HUD on, from the day-start camera and from the side with an ant standing on the entrance: the ant neither floats nor sinks.
8. `Leaf.MeshGap`, and a pre-look save loading with its bites, after the collars.
9. Frame times unchanged within noise. Packaged smoke test with and without ray tracing.

## Not in this work

- Lighting, fog, bloom, the canopy, the HUD.
- Bark displacement, the tree generator, a richer forest floor.
- A nest that changes with the colony.
