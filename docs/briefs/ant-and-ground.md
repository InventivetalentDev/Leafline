# Brief: the ant's look and the forest floor

For the main session to hand out. Written Sat 14 Nov, 10:30, from two owner screenshots (the ant on shaded bark; the nest on day 18 of the tall spire) and suggestions from Codex and the planning session. Two small visual jobs that run beside the bird. Neither changes the walking surface, the save or `LeafSet`'s leaf count.

Commits start `Look:`. Both jobs are in before the 17:00 package or they wait for Game Off.

## Who does what

- **A Claude agent in a worktree:** all the C++ (the scatter component, the colour values, the console commands).
- **The main session, between bird packages:** the `M_Ant` change, since it owns the editor and `.uasset` files. The agent writes the material script; the main session runs it.

## 1. The ant

**Problem.** The ant is dark red-brown (`BodyColor` 0.32, 0.10, 0.04) and close in brightness to shaded bark. The rim light added for video (`Multiply_11` in `M_Ant`, strength 0.35) traces every edge and reads as a selection highlight. The owner wants it gone.

**Direction.** Contrast that sits on the ant, not around it:

- **Rim light off.**
- **Colour:** a richer copper-rust on head and thorax, bright enough to keep its colour in shade. Legs a medium reddish brown, not darker than the body: they are thin and vanish first.
- **Gaster:** a little deeper than the thorax (leafcutters have a darker abdomen). The camera sits behind the ant, so the gaster is the largest part on screen: it must not go dark. Give it the broad highlight.
- **Shell:** glossier, with broad soft highlights, not a tight sparkle.
- **Fill:** a small brightness floor so the ant keeps its colour in deep shade. Keep it under the bloom threshold; it must not read as a glow.

**Build.**

- The mesh has part ids in UV1 (body, head, antennae, mandibles, legs) but none for the gaster. Mask the gaster by position along the body in the material; do not regenerate the mesh.
- Expose rim strength, roughness, fill, and the head-thorax, gaster and leg colours as material parameters, set from C++ on the dynamic instance, so later tuning needs no material work. If the graph needs new nodes, build it as `M_Ant2` from a script and keep `M_Ant` as the fallback.
- `Leaf.AntLook 0|1` switches between the old and the new look. Register it before a full build.
- Recruits and the player's ant: keep whatever difference there is today.

**Two variants for the owner,** side by side: A with a clearly darker mahogany gaster (Codex's palette), B with the gaster only slightly deeper than the thorax.

**Follow-up, not in this pass (owner's note):** a soft contact shadow under the ant may still help it stand out. Judge the new colours first. If it is still wanted, try the cheap form before touching the bark: a small soft dark disc carried by the ant, lying on the surface along its normal, faded out on thin twigs where it would overhang. It needs one small translucent material from the main session and no bark rebuild.

## 2. The forest floor

**Problem.** Beyond the nest the ground is an empty plane to the horizon. Nothing rises above it, so nothing shows the ant's scale.

**Direction (Codex's, agreed):** a leaf-litter clearing kept open by the colony, with sparse undergrowth round it.

In this order:

1. **Fallen leaves.** The existing leaf shapes in dull brown, ochre and olive: whole leaves overlapping, with smaller fragments. Clusters round the edge of the clearing and against the roots, with irregular bare gaps. A fallen leaf is many ant lengths across, which is the point.
2. **Grass tufts and seedlings.** Clumps of three to five tall curved blades, dark olive, tapering to a point. Some far enough back to break the flat horizon against the fog.
3. **Twigs,** only if 1 and 2 are in: a handful of thin tapered sticks lying at angles.

**Rules.**

- **Its own component, generated once per tree from the tree's seed.** Not through `LeafSet`: saves check the leaf count. If you reuse `AddDecorLeaf` or the leaf builder, show that the count check is unaffected.
- **Visual only.** The walking field does not change. The ant can walk about 70 m from the trunk (`BoundaryRadius`), and blown-off ants walk home across the floor, so:
  - within the clearing (the nest, the way from the nest to the trunk, a band round the trunk and roots where trails run), nothing at all;
  - in a ring round the clearing, leaves lie flat, a few millimetres above the ground and below the trail ribbons, so an ant walking over one looks right;
  - curled or tilted leaves, grass and twigs start farther out. An ant walking home through a tuft is acceptable; a tuft in the day-start or harvest camera's view of the nest is not, if it hides ants or trails.
- **Colour.** Litter is dull brown to olive: brown already means "cannot be cut". Grass is a dark olive or blue-green, clearly darker than any leaf on the tree, so it never reads as something to cut. No orange: look pass 2 removed the orange spots.
- **Material.** Use the plain colour material that the nest scraps use (`BasicMaterial`); `M_Leaf` made small ground scraps glow. No new asset for the first version.
- **Grass that sways with the wind** is a second step: it needs a material with a bend, built by the main session from a script. Static grass ships if there is no time.
- **Also try, tuning only:** less fog close to the ground near the nest. In the owner's shot everything beyond a few metres is one beige tone.
- `Leaf.Floor 0|1` turns the scatter off. Register it before a full build.

## Acceptance tests

Run in the `RefShot` slot, not the owner's save. Fillets went off by default at 09:52 today; take all shots on the current build.

1. The owner's two poses, before and after. The nest pose is on the tall spire: seed 15161, `Leaf.Goto -547 1550 7 -165 -7`. For the ant, one pose on sunlit tan bark and one on shaded green-grey bark, with variants A and B and the old look.
2. The ant with the wind map up: it stays distinct from the amber and red dots.
3. A file of recruits on the trunk and at the nest: they read as ants, and the player's ant can still be told from them as before.
4. The nest from the day-start camera and the harvest camera: entrance, trails and the file of ants are not hidden.
5. A blown-off ant's walk home, followed by the camera: it does not sink into or float over anything in the clearing or the flat ring.
6. A save from before this work loads with its bites (the leaf count is unchanged).
7. Frame times at the nest and on the trunk looking down, with and without ray tracing, against the `tech.md` table. Packaged smoke test.

## Not in this work

- Any change to the walking field, the ground's shape or the nest.
- Outlines or a stylised look for the ant.
- Mushrooms, stones, flowers or anything that needs a new model.
