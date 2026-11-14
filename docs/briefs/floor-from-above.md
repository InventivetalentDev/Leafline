# Brief: the forest floor seen from the tree

For the main session to hand to the `look-floor` agent. Written Sat 14 Nov, 17:35, from an owner screenshot (build 3bbb4ed, the old giant, seed 675856, day 7, `Leaf.Goto 504 1008 5966 -37 -14`: 60 m up a limb, looking down). The owner: "the ground just looks bad and very obviously like random noise."

In the last package before the freeze, or it waits for Game Off. Commits start `Look:`.

## What the shot shows

- The floor is pale yellow-green and olive blotches 10 to 30 m across, with dark crack-like lines between them. Nothing in the world is that size, so the eye reads it as noise.
- The blotches are the ground material, not today's scatter. The scatter had probably not spawned in this shot (the owner reports it sometimes does not; that bug is being fixed). **Retake the pose first, with the scatter confirmed in the log.** Expect the grass to show (2,850 tufts of 2 to 4 m within 70 m) and the litter hardly at all (about 3,000 leaves on a disc of 140 m radius, one per 20 m²). Then judge how much of step 2 is still needed; the owner expects the ground to look wrong either way.
- The floor is brighter and more saturated than the bark and the ant in front of it.
- The tree's shadow on the floor is the best thing in the frame. Keep it.

## Changes, in this order

1. **Haze by distance (tuning only, do it first).** The floor seen from the tree should sit behind soft haze, lower in contrast and a little cooler, so the limb and the ant stand in front of it. Use the fog's start distance, so ground closer than about 15 m stays clear: the nest view must not get foggier (the owner found it too flat already).
2. **A far look for the ground material.** Beyond about 20 m from the camera, blend to:
   - no crack lines and no hard cell edges;
   - a carpet of leaf-sized flecks (about 1 m, the size of the fallen leaves), each with its own tone from the litter palette (dull brown, ochre, olive), so the pattern has the size of something that exists;
   - only faint variation at larger sizes;
   - a darker, less saturated average than now, darker than sunlit bark.
   The near look, as seen from the nest, stays as it is.
   `M_Ground` has no script source. Build the new one from a script and a source file under a new name, as with `M_Grass` and `M_NestSoil`, and keep `M_Ground` as the fallback behind `Leaf.Ground 0|1` (registered before a full build). The main session runs the script.
3. **Depth in the grass, if time.** Tufts cast no shadows, so from above they are flat specks. Darken each blade toward its foot (the height fraction is already in the blade's UVs). Try shadows on the near tufts only if the frame time allows.

4. **The flat plain (owner, 17:45: "it being a completely flat plain isn't helping").** Two parts; the first is cheap, the second is the first thing to cut.
   - **Shading only, where ants walk.** In the far look, tilt the normal with a slow undulation a few metres wide, so the low sun shades the floor as gentle rises and hollows. No geometry, no silhouette, nothing an ant can float over. Fade it out near the camera so the nest view stays flat and true.
   - **Real relief, only where no ant goes.** The ground in the walking field is a plane and stays one. The visible ground (`ATreeActor::BuildGround`, rings out to 5 km) can rise and fall beyond the ants' reach: low banks and swells from about 90 m out, growing to a few metres, so the horizon stops being a ruled line and the view down has slopes that catch light.
     - One height function, zero inside the start radius with a smooth ramp, used by the ground rings, the grove trunks' feet, and the far grass tufts.
     - The ant is turned back at `BoundaryRadius` (70 m, plus 8 m). First check in the unattended runs' logs how far from the trunk a blown-off ant can land; the relief starts beyond that, with a margin.
     - The rings are coarse far out (each 12% wider than the last), so keep the swells broad (40 m and more) or add rings.
     - `Leaf.Relief 0|1`, registered before a full build.

## Not now

- Paths cleared through the litter, radiating from the nest (real leafcutter colonies keep such trails; from above they would read as the colony's lines). A good Game Off item.
- Shrubs or saplings with crowns above the ground.
- More scatter. Covering 60,000 m² with leaf meshes is the material's job at that distance.

## Acceptance tests

Slot `RefShot`, current build.

1. The owner's pose above, before and after, and the same view from 20 m and 120 m up.
2. The nest from the day-start camera and from an ant standing on the mound: unchanged or better, and not foggier.
3. A slow climb of the trunk from the ground to 30 m, as a short capture or five stills: the change from the near look to the far look is not visible as a line or a pop.
4. The trail on the ground near the nest and the wind map's dots on the roots still read.
5. Frame times at the nest and at the owner's pose, with and without ray tracing. Packaged smoke test.
