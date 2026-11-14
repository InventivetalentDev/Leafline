# Brief: in-world wind cues (Saturday)

For a parallel agent. Written Sat 14 Nov by the main session from the owner's playtest note: "try to move as much of what's currently UI text (like 'gust, you're sheltered here') to have an in-world visual indicator (can still keep the UI text as well, but needs some proper in-world feedback)". The owner chose this as one of Saturday's items (with the bird, `docs/briefs/bird.md`, and the video kit). Freeze 23:00.

## What exists

- `AWindSystem` (World/WindSystem.*): direction, gust phases (`EWindPhase`: calm, warning, rising, holding, falling), strength, `GetExposure(pos, normal)`, and 60 wind streaks (`M_WindStreak`, `UpdateStreaks`) round the camera.
- The ant leans away from the wind and shivers with its spot's exposure; the carried piece shakes like a sail; gripping has a pressed-flat pose (Ant/AntPawn.cpp `UpdateCutVisuals`).
- The wind map (hold Tab / LB, World/ExposureOverlay.*) colours exposure on the bark.
- HUD text: "Wind: gust coming", "Gust! Hold Space to grip", "Here: sheltered / partly exposed / exposed", "Gust! You are sheltered here".

## Targets (in this order; each one independently useful)

1. **The gust is visible before it arrives.** During the warning, a gust front rolls in from upwind: streaks thicken, lengthen and speed up on the upwind side first and sweep past; loose specks (pollen, dust, leaf bits) stream through the air. A player who never reads the HUD should see a gust coming 2 to 3 seconds ahead.
2. **The ant's own spot reads at a glance.** Near the ant (within about 2 m), air movement follows the local exposure: on exposed bark specks and streaks rush past the ant during a gust; in shelter they pass beside it and the air round the ant stays still. The existing lean, shiver and sail shake stay.
3. **Gripping reads as holding on.** If the existing pose is weak at game distance, strengthen it (claws digging in, a brief scrape of specks blown off the ant), visual only.
4. Optional if time remains: nearby leaves and twigs flutter harder in the gust than at calm, clearly enough to see (the leaf material's sway is driven through `MPC_Wind`; read how before changing anything).

## Constraints

- Math only: procedural geometry and existing materials (`M_WindStreak`, `M_Leaf`, `M_Trail*`), no textures, no particle system assets. New materials can only be built by the main session: if one is needed, write its HLSL as a file in `Tools/materials/` (like `bark2_bark.hlsl`) and say what inputs it needs.
- Visual only: nothing changes the wind, exposure values, the walking field or the economy. Everything behind one switch (`Leaf.Cues 0|1`, tuning in one struct with `Leaf.Tune`), default on; off must look exactly like today.
- The HUD keeps its text (the owner said so).
- Keep it cheap: under 0.3 ms of game thread and no visible GPU cost (`Tools/perf_packaged.sh`).
- Colour meanings stay: green / amber / red is sheltered / partly / exposed and must match the HUD and the wind map.
- The bird (another agent, in parallel) adds its own in-world cues; do not build bird visuals.

## Acceptance

- Stills and, where it matters, a few frames in sequence: a gust approaching from calm (warning to rising) on exposed bark and in shelter, from the normal camera; the gripping ant.
- `Leaf.Cues 0` gives today's picture.
- Frame times unchanged within noise.
