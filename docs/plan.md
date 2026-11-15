# Leafline: build plan

Ordered work from an empty template to two jam submissions. Read `design.md` (what) and `tech.md` (how) first. Tick items here as they land, and add a line to `devlog.md` at each milestone.

## Clock

| Point | Time (CET) |
|---|---|
| Planning finished | Thu 12 Nov, late morning |
| **Feature freeze** | **Sat 14 Nov 23:00**, moved to Sunday by the owner |
| **Our submission deadline** | **Sun 15 Nov 23:00 at the latest** (owner, 15 Nov evening; was 22:00) |
| MegaJam hard deadline | Mon 16 Nov 02:59 |
| Game Off hard deadline | Tue 1 Dec 22:37 |

The owner playtests at the end of each milestone. M1 and M2 may both land on Thursday if quota allows; the order matters more than the day.

## Who does what

| Agent | Owns |
|---|---|
| **Main Claude session (Opus)** | The Unreal project and the editor. All gameplay C++, materials, levels, packaging. The only writer of `.uasset` and `.umap` files. |
| **Parallel Claude agents** | Jobs that never touch the editor: the Blender ant, the audio script, the dev log. Use a cheaper model for mechanical ones. |
| **Codex** | Concept and key art, an independent code review after each milestone, and self-contained C++ on a branch (save game and upgrades). Briefs in `docs/briefs/codex.md`. |
| **Fable** | Brought back only for a hard problem or a milestone review. |
| **Owner** | Playtesting and feel, the video, itch pages, joining the jams, final calls. |

Rules for parallel work:
- One writer for binary assets. Parallel agents deliver source files (`.py`, `.wav`, `.fbx`, `.png`) into `Art/` or `Tools/`; the main session imports them.
- Code branches from parallel agents touch only the files named in their brief, and are worked on in a **separate git worktree or clone**. Never switch branches in the main working tree while the editor or the main session is using it.
- Every agent follows the clean-room rule in `CLAUDE.md`.

## M0: Bootstrap

- [x] `git init`, `.gitignore` for Unreal (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`), Git LFS for `*.uasset`, `*.umap`, `*.png`, `*.jpg`, `*.wav`, `*.fbx`, `*.blend`.
- [x] Run `python3 Tools/strip_image_metadata.py` (see `docs/tools.md`) so no photo metadata is committed.
- [x] Delete the template content: `Content/Characters`, `Content/ThirdPerson`, `Content/LevelPrototyping`, `Content/Input`, and the template's external actors. Fix `DefaultEngine.ini` map and game mode entries that point at them.
- [x] Set `ProjectName=Leafline` in `DefaultGame.ini`.
- [x] Add the C++ module (`Source/AntTreeGame`, the two target files, the `Modules` entry in the `.uproject`). Enable the `ModelContextProtocol` and `ProceduralMeshComponent` plugins.
- [x] `Scripts/build_editor.bat` and `Scripts/package_win64.bat` (see `CLAUDE.md`). First successful build.
- [x] Empty level `/Game/Leafline/Maps/L_Main` with sky, sun, fog; set as default map. `LeaflineGameMode` set as default.
- [x] Editor opens, MCP server answers, a screenshot can be captured.
- [x] First commit (done). Private GitHub repo created and pushed on 2026-11-12: https://github.com/InventivetalentDev/Leafline (SSH remote `origin`). Making it public needs a fresh yes on Sunday.

**Done when:** the editor opens the empty level from a clean clone and build, and the MCP tools respond.

## M1: The pillar (tree, walking, camera)

- [x] `TreeSkeleton`: seeded trunk, limbs, boughs, twigs, root flare and roots.
- [x] `TreeField`: distance, gradient, project, march, with the ground included.
- [x] `TreeMeshBuilder` and `TreeActor`: smooth tubes, flat placeholder bark colour.
- [x] Lighting check by screenshot (see Verify in `tech.md`).
- [x] `AntPawn` with `AntMovement`: walking in any orientation, sprint, stamina.
- [x] `AntCamera`: lazy up vector, orbit, spring follow, field-based avoidance.
- [x] Input created in C++; keyboard, mouse and gamepad.
- [x] Debug commands: `Leaf.AutoMove`, `Leaf.AutoTurn`, `Leaf.Teleport`, `Leaf.NewDay`, `Leaf.DebugField`.
- [x] Minimal canvas HUD: stamina bar.

**Done when:**
- An unattended test (`Leaf.AutoMove 1` with a slow `Leaf.AutoTurn`) runs for five minutes across trunk, junctions, undersides and twig tips with no stall and projection error under 1 cm in the log.
- Circling the trunk takes 20–25 s walking.
- **The owner has played it and says the movement and camera feel right, or has listed what to change.** Do not move on to M3 with open feel problems.

## M2: The loop (cut, carry, trail, shift)

- [x] `LeafSet`: leaf placement on twigs, quads rendered with a placeholder green.
- [x] Cutting: prompt, hold to grow, bite written to the leaf, fragment attached to the ant, speed penalty.
- [x] `TrailNetwork`: path recording, ribbon rendering, riding.
- [x] Send home: unattended replay; roster; next worker spawns; camera glide.
- [x] `NestActor`: deposit on arrival.
- [x] `DayDirector`: day start, shifts, all home, recap screen, new day with a new seed.
- [x] HUD: carried load, worker pips, prompts.
- [x] **First packaged build** with `Scripts/package_win64.bat`. Run it outside the editor.

**Done when:** a full day can be played from the packaged build: three workers out, three fragments home, recap, next day on a different tree.

*Parallel jobs to start once M1 is underway: the ant mesh, audio, Codex key art.*

## M3: Wind

- [x] `WindSystem`: daily direction, gust phases (parameter collection waits for the leaf material in M5).
- [x] Exposure and the blow-off test; `Falling` and the walk home.
- [x] Grip action.
- [x] Risk colour on the cutting arc.
- [x] Leaf sway, streaks, wind audio hooked to strength.
- [x] `Leaf.Gust`.

**Done when:** a large fragment on the exposed side is blown off in a full gust; the same fragment on the sheltered side, or with grip held, survives; and both outcomes are visible in the log.

## M4: Progression

- [x] `LeaflineGameInstance`, `LeaflineSaveGame`, the five upgrades and their effects. (Good Codex branch task once the interfaces in M2 exist.)
- [x] Nest upgrade screen between days.
- [x] Colony meter, three milestones, end card, continue.
- [x] Day scaling: leafy height, gust strength and frequency.
- [x] Title and pause screens.
- [x] Codex code review of M1–M3; fixed (docs/reviews). M4 screens, audio and pause reviewed too; five fixes (docs/reviews/codex-m4-audio.md), four of them checked by reading only.

**Done when:** a fresh save can be played to the end card in about 15 minutes, and quitting and relaunching keeps the colony.

## M5: Presentation

Order by visible impact per hour:

- [x] Bark material (plates, fissures, moss). Rebuilt as `M_TreeBark`: seamless world-scale noise on a cylinder per branch, with the camera cutaway. Bark relief in field and mesh not done (cut list item 3).
- [x] Leaf material: shape, veins, backlight, bite edges, wind sway.
- [x] Lighting pass against concept image 01: sun angle per shift, volumetric shafts, fog, mild depth of field, exposure. Continued as M5d (look pass).
  - Pass 1 (Thu evening): `FLookTuning` on the day director, tunable live with `Leaf.Tune look`. Sun warmth per shift (4600/5600/3800 K), haze based at the ground, stronger shafts, sun glow in the fog. Leaves 1.5 times as dense; the distant grove has leaf crowns. Reference shots with `Tools/ref_shots.sh`. Still open: ground colour (M_Ground, needs the editor), leaves dithering near the camera, exposure.
- [x] Final ant mesh with shader gait; carried fragment mesh (placeholder disc).
- [x] Trail shader (soft-edged translucent cyan; no flow yet).
- [x] Ground material. Distant trunks with leaf crowns (scenery only).
- [x] Audio: runtime WAV loader (no import tool), all sounds hooked up. Mix not yet checked by ear. (The owner set the wind mix on 15 Nov: the calm bed about 4.5 dB lower, the gust loop without its whistle.)
- [x] Onboarding prompts for the first day; recap screen titled as the day's cut.
- [ ] Screens moved to Slate if time allows. Not done: the canvas screens ship (first item of the cut list below).
- [x] Options in the pause menu (Thu evening): mouse sensitivity, invert look, volume, graphics quality (Low to Epic; Low turns Lumen off). Keys, pad and mouse; saved in GameUserSettings.ini.
- [x] Wind readability, first pass: an arrow by the wind gauge shows which way it blows relative to the view, "Here: sheltered / partly exposed / exposed" under it, and the gust warning says to grip or get to the side away from the arrow (or that you are sheltered).
- [x] Cut sequence, first pass: sawing, a tear line on the leaf, the piece swinging onto the back.
- [x] Small moments (Thu evening): at the nest the ant walks into the entrance and sinks out of sight while its piece drops in; a banner "Worker 2 of 3 sets out" (the day's first also names the tree); when blown off, the piece flutters away on the wind instead of vanishing, and a fall camera watches from outside and above.
- [x] Fix: blown-off ants flew straight through the trunk (on the windward face the wind points into the wood) and the camera showed only bark. The fall now peels off the surface and glances off wood on the way down.
- [x] Tree character (Thu evening): day 1 is the classic tree; later days take turns as a spreading giant (longer, flatter limbs), a tall spire (taller, narrower, more and steeper limbs) and a leaning elder (lean, wobble, six roots). Every tree has one broken limb low down, a bare stub, as a landmark. The name shows in the HUD and on the recap. Walking surface checked on all four (`Leaf.MeshGap` worst 0.00 cm). `Leaf.Tune day TreeShapeOverride <0-3>` forces one.
- [x] Owner playtest 1 (Thu): all seven notes fixed (`docs/playtest-notes.md`). Bark look and the camera cutaway await the owner's verdict; leaves are not dithered yet.

**Done when:** a screenshot from play, set beside concept image 01, clearly shows the same game, and a new player reaches their first cut without being told the controls.

**Feature freeze: Saturday 23:00** (moved to Sunday by the owner). After this, only fixes.

## M5b: The line (Thu night to Fri; owner's verdict Friday evening)

Spec: `docs/briefs/the-line.md`; work split and interfaces: `docs/briefs/line-contract.md`. Tagged `pre-line` before it started; commits start with `Line:`. If the owner cuts it, `FDayTuning::bRecruits` ships off and nothing is reverted.

- [x] Lines: a scout's piece on its way home turns its loop-free route (from the cut, `BuildReturnRoute`) into a line; the patch is the walkable leaves around the cut leaf (`ATreeActor::BuildPatch`, `BuildLeafApproach`).
- [x] Recruits (`ALineSystem`): one pool from Brood, three trips each, leave 1.5 s apart, same wind test as scouts, blown-off recruits lose the trip, stripped patches send them home without using one.
- [x] Fixed-step sim: recruits, the wind clock and (in the harvest) returning scouts step in 1/120 s steps, so a skipped harvest is the same simulation.
- [x] Day end: every ant home; harvest phase at 5×, skippable; camera at the nest looking up the trunk, Tab hops to the patches.
- [x] Richer leaves (1× to 3×, paler green), patch read-out and outline before cutting.
- [x] Trail brightness by patch (replaces the age fade).
- [x] Six-level ladders, Brood as the recruit pool, two milestone sets, save version 2 with v1 migration, best day; `-LeafSlot` test slot.
- [x] Number-go-up: "+N" at the nest with a rising chime, colony counter, one row per line, recap counting up line by line.
- [x] Off switch `Leaf.Recruits 0|1`.
- [x] Codex review of the day-end state machine (7 findings, all fixed). Second pass over the later changes: 3 findings (a rescued, possessed scout could leave the player with no ant; an ant handed to the sim clock could move twice in one frame; scout rescue and the wind clock ran on real time), all fixed.
- [x] Acceptance tests (`Tools/line_test.sh`, seed 517192, day 1, pool 4 unless noted; Thu night):
  1. One line 27.3, three lines 45.5. Pass.
  2. Pool 6: the first patch ran dry with two scouts still waiting ("6 idle"), and all six recruits moved to the second line. Pass.
  3. Large pieces lose 8 to 9 of 12 recruit trips, small pieces 0. Pass. Windward versus lee routes: 9 versus 7 and 8 versus 6 lost; the direction holds but the gap is small, because the test routes run along branch tops (exposure at most 0.5) and only differ on the trunk.
  4. Economy model only (`Tools/econ_sim.py`): end card at 16.8 min with the line, 18.5 min without. Needs the owner's play.
  5. Economy model only: day hauls grow 10.1× from day 1 to the end card.
  6. Watched versus skipped harvest, day 2, three large-piece lines with losses: 19.4 both, 4 home, 8 lost. Pass.
  7. Twenty unattended days (`Leaf.AutoPlay`), twice: every day ended. Recruit cuts that missed their leaf (fallback): 144 of 341 before followers stepped onto their route when pinned, 0 of 358 after. Pass.
  8. Editor-binary standalone, 1280x720: two lines with pool 0 game thread 1.9 ms, pool 10 at work 2.5 ms, pool 10 in the 5× harvest 5.6 ms; 60 fps throughout. Pass.
  - Seen in the runs: from about day 6 the gusts (peak +15 % a day, lines over 100 m) blow off nearly every laden recruit unless Grip is high. That is the existing difficulty curve; the owner should decide whether to cap it.
- [x] Leaf colour by richness in `M_Leaf`: base colour blends toward pale young green (0.32, 0.56, 0.11) by vertex alpha × 0.9 (`Constant3Vector_4`, `Multiply_12`, `LinearInterpolate_2`). Subtle in a first look from the trunk; the owner may want it stronger.

## M5c: One tree (Fri; owner's verdict Friday evening, with the line)

Spec: `docs/briefs/one-tree.md`; work split: `docs/briefs/one-tree-contract.md`. Tagged `pre-one-tree` before it started; commits start with `Tree:`. If the owner rejects it, `FDayTuning::bOneTree` ships off and nothing is reverted. It comes before the bird, which moves to Game Off unless this is kept and finished early.

- [x] Persistent tree across days, the flag (`Leaf.OneTree`, latched at dawn), leaves that can no longer be cut (bare stalk, or bitten blade with vertex R = 0 for brown).
- [x] Saving and loading: bites and the day's routes written with the day's haul at day end, save version 3 (versions 1 and 2 load as 0 and migrate), one leaf-mesh build on load, leaf-count check.
- [x] Density by height (sparse ×12 over 55 m on tree 1, ×8 over 45 m on trees 2 and 3), stage scaling (`FDayTuning::Stages`), moves at the second and third milestones (one a day), the move card, the climb hint on each tree's first day, "Stripped. Fresh leaves are higher up.", the colony meter's "new tree" marks.
- [x] Yesterday's trails: faint, ridable, no recruits, gone after the second night and on a move.
- [x] Wind: direction turns 70–290° a day; within a stage peak force ×(1 + 6 % per day on the tree), the stage's gust multiplier on a move.
- [x] Codex review of the save and load paths and the day start: 6 findings (mid-day flag toggle, debug purchases saving mid-day haul, stale tree after switching off, reset then day end, recruits-off migration stage, debug move skipping a tree), then 3 more on the fixes (all debug paths); all fixed.
- [x] Economy numbers from the measured trees (`Tools/econ_sim.py`, `docs/economy.md`): one-tree thresholds 25 / 155 / 270 with recruits, 5 / 60 / 95 by hand. Since 15 Nov the set with recruits is 25 / 190 / 335 (recruits from day 1 and the climb curve, `RichnessHeightScale` 40).
- [x] `M_Leaf`: vertex R = 0 turns a leaf brown: `OneMinus_2` on `VertexColor_0.R` drives `LinearInterpolate_3` (base colour toward 0.20, 0.115, 0.04, `Constant3Vector_5`) and `LinearInterpolate_4` (backlight toward 0.30, 0.15, 0.035, `Constant3Vector_6`). R was unused before (G is colour variation, A richness). Wiring checked over MCP; no close-up screenshot of a brown leaf yet. Screenshots: `Saved/RefShots/tree-movecard.png` (the card over the new trunk), `tree-bare-low-bough.png` (a bare low bough on the first tree).
- [x] Package and zip from 852145d (13 Nov 11:28, `Build/inventivetalent_Leafline.zip`, 463 MB); packaged smoke test in slot `PackSmoke`: two days on a kept tree, no fatal lines.
- Acceptance tests (`Tools/tree_test.sh`, slot `TreeTest`; Fri morning):
  1. Two days, quit, relaunch: same tree (seed 675856), same 6 bites (22,620 pieces left), day 3. Pass; re-run on the final build after the review fixes: pass.
  2. Twenty scripted days (lowest leaves first, bough by bough, Brood first to level 2, save reloaded every morning): on the morning of day 4 the three lowest boughs (33.0, 35.5 and 37.5 m) had no live leaf left, and the 30–40 m band was empty. Pass.
  3. Mean height of the day's cuts on tree 1, days 1–12: 36.7, 36.7, 36.7, 46.6, 46.5, 49.5, 54.2, 57.5, 61.1, 59.1, 59.5, 59.7 m (lowest cut 32.4 m on days 1–3, 41.1 on days 4–5, then 44–59 m); on tree 2, days 1–8: 45.8, 45.8, 53.1, 53.6, 54.6, 58.5, 60.7, 63.5 m. It rises within each stage, with two dips on tree 1 (0.1 m on day 5, 2 m on day 10 after a day of high picks). Pass, with those dips.
  4. Seven days with 100 haul given each morning: milestone 1 on day 1 did not move the colony; milestone 2 on day 2 moved it to the spreading giant on day 3 (card, fresh leaves, no trails); milestone 3 on day 5 showed the end card and moved it to the tall spire on day 6; play continued there. Pass.
  5. Economy model on the measured trees (`Tools/econ_sim.py`, `Tools/tree_bands_measured.txt`): with recruits, thresholds 25 / 155 / 270, four days on tree 1, one on tree 2, end card on day 5 at 17.1 min (15.8 skipping harvests); by hand 5 / 60 / 95, four days on tree 1, end card at 18.6 min. Pass (model; needs the owner's play).
  6. Day start with a kept tree 1–2 ms, with a grown one 300–460 ms. Pass.
  7. Twenty unattended days with the save read again and the tree regrown from it before every day (`Leaf.AutoPlay 20 3 skip buy reload report`), three runs: every day ended; one run moved twice (tree 2 on day 13, the third milestone on day 20). Pass.
  8. A copy of the owner's packaged version-2 save (day 12, Grip 2, Brood 1): migrated to version 3 with day, levels and haul kept, a fresh first tree (its 89 hauled is one milestone), and the day played and saved. Pass.
  9. One tree off, final code: day 1 (seed 517192) 8140 leaves in 21 chunks, richness median 1.74, mean 1.73; day 2 (seed 103900, spreading giant, 27 m) 6731 leaves in 17; day 3 seed 67120 (tall spire, 34 m) and day 4 seed 208038 (leaning elder, 41 m) as before; old milestone sets. Identical to `pre-one-tree`. Pass.
  10. Yesterday's routes load next morning ("3 yesterday's routes", 2839 points after a relaunch), drawn at 0.3, no lines on them, and are replaced the next night and dropped on a move ("0 yesterday's routes" on each move day). Pass (by the log; the faint look is not in a screenshot yet).
  11. Wind direction on one tree, final code: tree 1 days 1–12 at 133, 325, 216, 23, 201, 70, 334, 130, 225, 33, 210, 312 degrees; consecutive days differ by 95 to 193 degrees (an earlier run had 67 then 68, which led to the 70–290 degree daily turn). Pass.
  12. Sparse tree, pool 3 (Brood level 2), lines to the lowest leaves: patches of 1–2 leaves (2–5 pieces); one line 9.0, three lines 36.7. From 60 m up (after `Leaf.Strip 60`): patches of 2–3 leaves (5–7 pieces); one line 19.9, three 41.1. Pass.
  13. Recruits off with one tree on, twenty scripted days: milestone 2 moved the colony to tree 2, milestone 3 showed the end card on day 14 and moved it to tree 3. All four flag combinations, seven days with 100 haul given each morning: every one reached the end card; with one tree on and recruits off both moves fell due on day 1, and the second followed a day later. Pass.
  14. Three cuts mid-day, quit, relaunch: day 3 again with 22,620 pieces and 6 bites, mass unchanged (20.39). Pass; re-run on the final build after the review fixes: pass (mass 18.69 before and after).

## Playtest round, Fri 13 Nov afternoon

From `docs/playtest-notes.md`; each feel change has an off switch for the owner's verdict tonight.

- [x] Wind map on held Tab / LB (`AExposureOverlay`, brief `docs/briefs/exposure-overlay.md`, `M_Exposure`); tap still switches workers. 1,459 to 1,940 dots within 7 m, filled in 0.9 to 1.4 s.
- [x] Steering wheel full up to 200 cm radius, fading by 320 cm (was 90 / 200). `Leaf.Orbit`: the 140 cm limb that turned the ant back now 2.6 laps in 12 s, three runs; 40 cm 9 laps, 90 cm 4.1, trunk unchanged.
- [x] Gait: 36 cm per leg cycle, at most 7 Hz (was 14 cm, 14 to 28 Hz); body rocks 4°, wags 3°, bobs 0.45 cm; `M_Ant` lift 2.2 cm, swing 2.8 cm. Stills only.
- [x] Walk-home fast-forward (`FDayTuning::bFastForwardHome`): seven scripted days with recruits off and seven with recruits on all ended, every recruits-off day and day 1 with recruits on through it.
- [x] Smoke trails (`M_TrailSmoke`, `SmokeHeight` 6 cm, `Leaf.Smoke 0|1`). Stills only.
- [x] One tree off still identical (seed 517192: 8140 leaves; 103900: 6731).
- [x] Package and zip from f92ca3b (13 Nov 12:52, 463.5 MB); packaged smoke test in slot `PackSmoke`: two days with the fast-forward and the wind map, no fatal lines.
- [x] Owner's verdict on all of the above, the line and one tree. No separate verdict is recorded; settled by what ships: the line (recruits), one tree, the wind map, the walk-home fast-forward and the smoke trails are on by default. The steering wheel was replaced the same afternoon by the screen-direction controls below.
- [x] Key and pad icons: Xelu's CC0 pack, light keys and Xbox-style buttons for every pad (owner's choice), drawn from tokens in HUD text for the device used last (`UI/PromptIcons.*`, `Leaf.PadIcons`).
- [x] Package and zip from 6865416 (13 Nov 13:10, 463.6 MB, 48 files, no saves); packaged smoke test: two days, wind map, icons load from the pak, no fatal lines.
- [x] Package and zip from f9d8b6d (13 Nov 13:33, 463.6 MB, no saves): the stronger smoke ribbon. Packaged smoke test: two days, wind map (one 3.5 ms tick logged), no fatal lines.
- [x] Controls: the stick is a screen direction everywhere with a camera snapshot (owner's design, 13 Nov afternoon); input view F3 / `Leaf.InputViz`; control trace in the log. Explainer page for the owner: https://claude.ai/artifact/GiZG9aHE4z9zALjSjgUvSB. Package and zip from 8563559 (13 Nov 14:40, 463.6 MB); packaged smoke test: input view, two days, no fatal lines.
- [x] Controls follow-ups (owner): twig snap off by default, the stick read against the branch's on-screen direction, one lap a second; package and zip from 4f356c2 (13 Nov 15:37); packaged smoke test passed.
- [x] Owner on the new controls (13 Nov, 15:37 package): "that feels pretty good now".
- [ ] Still open: PS5 audio check (needs the owner's pad). Aiming stays as it is (owner).

## M5d: Look pass (Fri evening; brief `docs/briefs/look-pass.md`; owner's verdict from a package)

Tagged `pre-look` before it started. Commits start `Look:` (and `HUD:` for the HUD, which has no off switch).

- [x] Part 0: views `junction` (limb 21 at -80°, the black wedge) and `sunward` (shift 1, lee side, toward the sun) in `Tools/ref_shots.sh`, plus `bough`, pitch sweeps, `SHIFT=`, `--probe`, and every run in the slot `RefShot` with a check that the owner's save is untouched. Before set `look0` (old bark, both modes). By a Claude agent; report `docs/reviews/look-shots.md`.
- [x] Part 1 (tuning only): sky light 3 with a cool tint, exposure bias 0.2, `WoodClearance` 60 (the "amber wash" on near bark was the cutaway digging a trench under a camera 12 cm above the bark). Fog unchanged. The dark morning sky is the low sun plus metering, not a bug.
- [x] Part 2, the wedge: bark buried inside the neighbouring branch, seen through the cutaway, black because nothing lights it. The mesh now marks buried bark in vertex alpha and `M_TreeBark2` hides it; gone at the `junction` pose (still there with `Leaf.Bark 0`). `ThickWoodRadius` made no difference anywhere. Cutaway band, taper and near fade are live in `MPC_View` and `Leaf.Tune cam`. Depth of field focuses on the ant's real distance.
- [x] Part 2, look-up assist behind `bLookUpAssist` (off; the owner tries `Leaf.Tune cam bLookUpAssist True`).
- [x] Framing: `BoomLength` 170 (was 220), `DefaultPitch` -25 (was -20). Needs the owner's verdict.
- [x] Part 3: `M_TreeBark2`, code in `Tools/materials/` (long ridges and soft furrows, relief from the normal, fine grain up close, moss in patches). `Leaf.Bark 0` returns to `M_TreeBark`.
- [x] Part 4: HUD restyle by Codex in a worktree (style table, panels, larger text, worker states, stamina label, the screens too); checked at 1280x720 and 3840x2160 with key and pad icons.
- [x] After set `look1` (both modes), pitch sweeps at the junction and on a 30 cm bough (no black area or inside view at any pitch), wind map and brown stalks readable (a single shot with rich, plain and brown leaves together still missing), `Leaf.MeshGap` worst 0.00 cm (no geometry moved), a pre-look save loads with its 12 bites, packaged smoke test with and without ray tracing (two days each, no fatal lines). Frame times (packaged, 1080p): Epic 4.0 to 4.3 ms GPU, Low 1.2 to 1.6 ms; nothing measurable from the new bark.
- [x] Shadowed new bark read slate grey-green (its colour was too grey; the sky tint made no difference): ridges warmed to a red/blue ratio of about 2.2, a little less moss.
- Still for the owner (from `docs/reviews/look-shots.md`): wood within half a metre of the camera blurs into streaks; fold-like light and dark triangles on bark at grazing angles; cutaway holes still visible at some pitches (soft now, never black); a cut-highlight outline that outlasts a faded leaf; looking up from the trunk shows less sky (19% of the upper frame against 49%) since the camera keeps 60 cm from thick wood.
- [x] Owner's verdict. No separate verdict is recorded; settled by what ships: the look-pass camera and sky ship (`BoomLength` 170, `DefaultPitch` -25, `ExposureBias` 0.2, `SkyLightIntensity` 3, `WoodClearance` 30 since look pass 2); the bark is `M_TreeBark3` (`Leaf.Bark 2`, default since 15 Nov); collars are off in favour of fillets. Veto list and undo as it was: bark `Leaf.Bark 0`; collars `Leaf.Collars 0`; camera `Leaf.Tune cam WoodClearance 12`, `BoomLength 220`, `DefaultPitch -20`, `ExposureBias -0.4`; sky `Leaf.Tune look SkyLightIntensity 1`, `SkyLightColor (R=1,G=1,B=1,A=1)`; HUD: revert its commit.
- [x] Owner playtest of the 17:28 package: holes where roots and big limbs meet the trunk. Caused by the buried-bark mask (clamped distances across 300 cm trunk triangles; parents masked inside children whose open base ring pokes out). Fixed: unclamped distance in UV3.y, parents masked only inside sealed bases. A/B with `Leaf.Bark 0` on the owner's tree (the root at the nest) by a Claude agent.
- [x] Faint bottom-left build and position line with a pasteable `Leaf.Goto` (owner's idea); `Tools/write_buildinfo.sh` before packaging.
- [x] Branch collars (owner's polish note, smoother junctions): `bBranchCollars`, `Leaf.Collars 0|1`; no twig collars, leaf count unchanged. Bark ridges follow a bark radius without flare and collars, so the trunk flare no longer reads as paving stones.
- [x] Package and zip from 8e4b36e (13 Nov 18:42, 463.8 MB, no saves): packaged smoke test with and without ray tracing (two days each, no fatal lines); the corner line shows the commit. Concept comparison shots on this build: `Saved/RefShots/concept-compare/` (six pairs and a README, for a Codex review).
- [x] Roots: cobble-like plates gone on open root (14 Nov night, ui-text, 70f3cf2): each root's bark radius is held near where it leaves the trunk (`Leaf.RootBark`, default on). The arch where a root rises into the trunk keeps roundish plates (cause not found; needs the bark material's radius debug view in the editor).
- [ ] Still open from the shots: dark cutaway views can still happen near junctions while walking (a frame from the 02 walk).
- [ ] Part 5 (trail near the camera, leaf tone variation, framing foliage on the grove) only if time remains before the freeze.
  - Framing foliage done 14 Nov night (ui-text, 1b5713b): an understory of 16 smaller leafy trees in front of the grove's giants (`Leaf.GroveUnder`, on). The trail fades near your ant since bb587e0. Leaf tone variation still open.

## M5e: Look pass 2 (Fri evening; brief `docs/briefs/look-pass-2.md`)

- [x] Item 1: the bark's cutaway opens only when the line from the camera to the ant runs through wood (`BarkCut`, `Leaf.CutLog`); leaves and the near-camera fade unchanged.
- [x] The hard-edged dark area at the gathering pose was not the cutaway but a hole from the buried-bark mask; hiding a parent's bark inside a child is now off (`bMaskParentInChild`).
- [x] Item 2: nest generated in Blender (owner's OK) with `M_NestSoil`; ground litter spots rarer, smaller, ochre. Root flare: long ridges since the bark radius.
- [x] Item 3: bark furrows narrower and shallower, plates that end, moss olive grey; the triangular seams were the relief (F2 gradient switches), found with debug views.
- [x] Item 3, carried piece veins: done 14 Nov night (ui, b80a20d): the piece takes the UVs, tone and richness of the leaf it was cut from; `Leaf.PieceVeins 0` gives the old look back.
- [x] Re-tests after collars: `Leaf.MeshGap` worst 0.00 cm (collars on and off); a pre-look save loads with all 12 bites.
- [x] Collars off by default: the owner saw square intersections and wants fillets (the walking surface's smooth blend shown on the mesh). Fillets: a Claude agent in the worktree `leafline-fillet` (branch `fillets`).
- [x] Acceptance (shots agent, `Saved/RefShots/cutaway2/`): test 2 passes at the concept pose (the 6 s pose failed only on the mask hole, since fixed), test 3 passes (factor 0 while the line is clear, up to 1.00 on two stretches 26 and 31 cm deep, back to 0 within about 1 s), test 4 passes; tests 5 and 6 not isolated in a still (leaf and near fade code unchanged). `WoodClearance` 60 no longer needed: 30 now.
- [x] Nest checks (test 7, a Claude agent): the ant neither floats nor sinks from four sides. First import had dropped the vertex colours (black mound, bleached ground through exposure, glowing scraps): fixed with SM_Nest2, lighter soil, plain green scraps.
- [x] Package and zip from 0dfba20 (13 Nov 20:16): packaged smoke test with and without ray tracing (two days each, no fatal lines); Epic 4.0 to 4.2 ms GPU (unchanged).
- [x] Owner playtest notes (13 Nov evening): the broken limb cut back to a stump after growth (it was 5 to 12 m, "a weird huge stump"); camera pitch range -65 to 80 (was -80 to 60: looking up stopped too soon on big wood, looking nearly straight down went through the wood above the ant). The `junction` reference view's -80 is now clamped to -65.
- [x] Fillets (owner): junction fillets in the walking field and skirts on the mesh, on by default (`bFillets`, `Leaf.Fillets`), by a Claude agent in a worktree. Leaf counts equal with and without (leaves are placed against a field without fillets), `Leaf.MeshGap` unchanged, 60 to 100 ms more per tree build.
- [ ] Fillets, still open:
  - **Seam lines.** The bark pattern switches with a hard line where the crease was (child to parent), and bark switches to ground at the foot. A shader cross-fade would need the second branch's coordinates in spare vertex channels (UV0 is unused by M_TreeBark2; vertex alpha is free). **Done 14 Nov night (ui, 005f1b8):** M_TreeBark3 cross-fades the two barks across the old crease and grades bark into soil at the foot (`Leaf.Bark 2`; on by default since the owner's decision of 15 Nov; no measurable GPU cost).
  - **Ground fillet (off by default).** With `Leaf.Fillets 1 -1 -1 -1 80`, ground columns are dropped when they do not start on the tree, end on the ground and stay within 3 radii of their outline point. Neighbours are not joined across sharp outline corners (seed 675856: 17 of 489 columns dropped round a small island of root at the ground; 517192 and 377994: 1 each; 2 to 5 unjoined pairs per tree). The lip where a root meets the trunk at the ground (shots `ground-artifact-*`) is smaller since 14 Nov: runs of failed columns are filled from their neighbours and the columns at open ends of the strip are laid onto the wall and ground (`leaf.GroundFill`, `leaf.GroundClose`); on seed 675856 a dark band remains in that corner (6 of 17 failed columns filled). On 517192 a few grey "slats" stand on the trunk wall at the root corner with or without this; they are not ground-skirt strip ends (probably a root's junction skirt near the ground), not diagnosed. With fillets walked, the ant floats over the closed end columns by up to the fillet's lift.
  - **Skirts are looks only (14 Nov).** The ant walks on the field without fillets, up to about 40 cm under the skirt in the crease of a big limb; ants, trail ribbons and exposure spots are drawn lifted onto the skirts (display only). `Leaf.SurfaceGap` and `Leaf.MeshGap` check the walking field and the tubes as before fillets; `Leaf.FilletCheck` checks the skirts against the skirt field.
  - **Walking on the fillets again (`fil-walk`, branch `fillet-walk`, 14 Nov evening).** The stuck recruits were the follower creeping at 1 cm/s along route steps that run through the wood, not the field. `bFilletWalk` (default on, `Leaf.FilletWalk 0` gives the looks-only split back): auto20 with fillets walked 487.9 hauled, no rescues; with `Leaf.Fillets 0` identical to the baseline. Merged 14 Nov, on by default since the 14:56 package; this replaces the looks-only split above.
- [x] Owner playtest notes, evening: sounds ducked during the fast-forward and the end jingle on its own (a Claude agent); the ant's head joined by a neck and the mesh lowered 1.2 cm onto the bark (a Claude agent, SM_Ant2); the bark cutaway keeps a faint dithered layer (`CutawayFloor` 0.35).
- [x] Package and zip from 763b3db (13 Nov 22:28) with fillets, the stump, the camera range, the sound, the ant and the cutaway floor: packaged smoke test with and without ray tracing (two days each, no fatal lines); Epic 4.1 to 4.3 ms GPU.
- [x] Carried-piece veins: merged 14 Nov night (b80a20d), auto20 unchanged.

## M6: Stretch (only if M0–M5b are complete before the freeze; the bird waits for the owner to keep the line)

1. Bird: perches, notices exposed ants, swoops; dropping a food item distracts it. Needs a model (Blender script), simple state machine, and a food pickup. Nothing harmful is shown happening to the bird.
2. Taking control of an ant that is already returning.
3. Walking on leaf blades.

## M6a: Saturday 14 Nov (owner chose: the bird, in-world wind cues, the video kit; freeze 23:00)

Agents in herdr tab `sat`; the main session merges, builds materials and imports, and packages.

- [x] Baseline before the bird (bird brief test 1): `Tools/tree_test.sh auto20` twice, fillets on (Sonnet `bd-base`). Results in `Saved/RefShots/bird-baseline/summary.md`.
- [x] Bird tier 1 with a placeholder shape, tests 2 to 5 (Opus `bd-core`, worktree `leafline-bird`, brief `docs/briefs/bird.md`). Codex reviews the bird's clock.
- [x] Bird mesh `Tools/blender/make_bird.py` and `Tools/materials/bird.hlsl` (`lk-nest`); main session imports `SM_Bird` and builds `M_Bird` (wing flap through a MID, like the ant's gait).
- [x] Bird sounds `bird_call`, `bird_pass`, checked with earshot (Sonnet `bd-sound`).
- [x] Package for the owner by 14:00 (tier 1). Second package by 17:00 (model, sounds, tier 2 if tier 1 is clean). Packages: 13:30 (98bf4e0), 14:14 (130e235), 14:56 (84c59a9), 15:47 (e8958d7), 16:54 (3bbb4ed), the 20:00 candidate at 18:25 (c5d301b), and the night packages at 23:55 (bf1e875: veins, root bark, grove understory, review fixes; relief, seams and the climb curve in but off) and 00:15 (4f2a74a: plus `Leaf.Milestones`); auto20 unchanged, 208 fps trunk and canopy with ray tracing on a quiet machine.
- [x] Owner's go or no-go on the bird, tier by tier: both ship, on by default (owner, 15 Nov: "keep em on, haven't noticed the bird too much so it at least shouldn't be a bad thing to have").
- [x] In-world wind cues (Opus `cues`, worktree `leafline-cues`, brief `docs/briefs/in-world-cues.md`): gust front, the ant's own spot, gripping.
- [x] Video kit (Codex `video`, worktree `leafline-video`, brief `docs/briefs/video-kit.md`): `Leaf.Cinematic`, `Leaf.Shot`, `Leaf.ShotList`, `Leaf.Slomo`; built and tested by `bd-base`. The reveal follows the step-4 path and ends on its 6 s pose.

- [x] Package of 14 Nov 13:30 (98bf4e0): the bird (tiers 1 and 2, MaxSwoopsPerDay 2, camera nudge during a dive at the player's ant), in-world wind cues, the ant's new look (`Leaf.AntLook`, default A) and the forest floor (`Leaf.Floor`), the video kit (reveal ending still being simplified). Fillets off. Twenty `auto20` days on merged main identical to the fillets-off baseline; packaged smoke test with and without ray tracing and DX11 passed (GPU 4.2 to 4.6 ms). Zip 0.44 GB.
- [x] Bird: wing flap visible across frames; shadow seen on days whose windward bark is sunlit (78319b2); during a dive at your ant the camera turns toward the bird and pulls the boom back ×1.8 (`NudgeKeepAnt` 1.1); circle brought in to 20–25 m; the bird prefers the player's laden ant (`PlayerPreference` 1). Economy rerun with PassHeight 150 and BirdScale 4 (hold and grip): 504.9 hauled with the bird against 469.5 without, end card day 16 against 17; the pair is noisy (one gust day decides it), so it shows only that the bird costs little against a gripping player.
- [x] Fillets display-only lift (`lk-fillet`), credits screen (`ui`), video reveal ending (Codex).
- [x] Fillets walked again (`fil-walk`, `bFilletWalk`, `Leaf.FilletWalk`), merged and on by default.
- [x] Pause menu Controls and Credits pages (credits from `Content/Leafline/UIRaw/credits.txt`); skins hook (`Leaf.Skin`, `Leaf.ReloadSkin`, `docs/ui-skin.md`; flat panels without a skin).
- [x] HUD icons (`Leaf.HudIcons`): wind, exposure, bird, carrying; the colony counter (hauled in all, bar to the next milestone) and the worker row.
- [x] UI text pass (`docs/ui-text.md`): every player-facing line in plain words; the cut arc and the carry read-out show weight with the value multiplier beside it.
- [x] Trails: one colour per line; fade with distance from your ant (`MPC_View` TrailFade*, checked in real runs 14 Nov 18:49).
- [x] Forest floor: grass from the clearing edge (2,850 tufts, `M_Grass2` backlit, dark foot), litter (`M_Litter`), NaN-vertex fix; floor seen from the tree (brief `docs/briefs/floor-from-above.md`): haze from 15 m, `M_Ground2` far look (`Leaf.Ground`), relief beyond the ants' reach behind `Leaf.Relief` (off at first; rolling hills from 90 m out, on by default since 15 Nov).
- [x] 20:00 candidate (c5d301b, 18:25): auto20 0 rescues, 487.77 hauled, end card after 16 days (same as the three runs before); packaged smoke test with and without ray tracing and DX11 (average fps: trunk 115, canopy 95 with ray tracing); zip 0.44 GB.

## M6b: Sunday 15 Nov (the owner's decisions before the release candidate)

- [x] Recruits from day 1: Brood level 0 has two recruits, the ladder is 2 / 3 / 4 / 6 / 8 / 10 / 12; with recruits on the bird first comes on day 2, with one gentle swoop and the hint (brief `docs/briefs/recruits-day-1.md`, 6eec0b0).
- [x] Bird and bait ship, on by default (9437179; the go above).
- [x] Climbing pays: `RichnessHeightScale` 40 and one-tree milestones 25 / 190 / 335 with recruits (variant B of `docs/economy.md`, 5be2add).
- [x] Relief (rolling hills on the far ground) and the seam-blended bark `M_TreeBark3` (`Leaf.Bark 2`) on by default (cd5b935).
- [x] Wind mix: the calm bed about 4.5 dB lower, gusts as loud as before, the gust loop without its whistle (volumes in `FWindTuning`, ef61b1b).
- [x] Background music: three tracks from `Tools/audio/make_music.py`, one per day in turn (golden pads, kalimba drift, music-box arpeggio, then the first again), on by default (`FWindTuning::bMusic`; 1d8d468, 86c1ff0).
- [x] UI polish: nest, recap, title, pause, options, move card and end card sized to their content; nest costs coloured by affordability, maxed in gold (5b47275).
- [x] Build line off in the submission build; the F3 input view stays (9a0c609, 4331542).
- [x] Credits: no UI art credit; Friend A commented out until their feedback comes in (4333c2d).
- [x] Tools: `Tools/release_build.sh` (package and check the submission build, d1a1e40) and `Tools/itch_shots.sh` (itch screenshots from the packaged build, with JPEG copies, efe9783 and 05f0b71).

## M7: Ship (Sunday)

- [x] Set `bShowBuildLine=False` under `[/Script/AntTreeGame.LeaflineHUD]` in `Config/DefaultGame.ini` (the faint build and position line). `Tools/release_build.sh` refuses to package while it is on. Done: off in the submission build; `Leaf.BuildLine 1` shows it while playing.
- [ ] Bug fixing from Saturday's playtest.
- [x] F3 input view: stays in the submission build (owner, 15 Nov: obscure enough that players won't stumble on it; useful for testing). `bInputVizKey=True`.
- [ ] Final packaged Windows **development** build. Run it from a path with spaces and from a fresh folder (`Tools/release_build.sh` does both).
- [ ] Zip as `inventivetalent_Leafline.zip`; confirm it is under 1 GB (0.44 GB in every package of 15 Nov).
- [ ] A 30–60 s gameplay video at 1920x1080 or higher, uploaded by the owner. Filmed and cut with `Tools/video_capture.sh` and `Tools/video_edit.py` (final cut list of 15 Nov, `docs/ship/video.md`); the upload is still to do.
- [ ] Codex produces the cover image and screenshots are captured for the itch pages. Key art banner with the title in both sizes (7374d38); screenshots from the packaged build with `Tools/itch_shots.sh` (JPEG copies for itch's 3 MB limit). Retake them from the final build.
- [ ] Two itch pages: one for the MegaJam (frozen), one for Game Off. Drafts: `docs/ship/itch-megajam.md` and `docs/ship/itch-gameoff.md`, with `.html` copies to paste.
- [ ] Submission form: full name and Epic account email, game name, video link, dev log link, tools used, sourced assets list, and the note about the earlier prototype (see `devlog.md`). Draft: `docs/ship/submission-form.md`.
- [x] Owner chooses a licence: MIT (15 Nov). `LICENSE` added; `README.md` copied from `docs/ship/README-draft.md` (its URL placeholders are filled once the pages exist).
- [ ] Owner says yes, then make the GitHub repo public and add its link to the Game Off page. Do this **before** submitting, because the dev log link must open for the judges. If the repo stays private, post the dev log on the itch page instead.
- [ ] No trademarks or logos anywhere except the Unreal Engine logo.
- [ ] **The owner submits to both jams in person by 23:00 CET at the latest** (the MegaJam's own deadline is Mon 16 Nov 02:59). Agents must not submit (MegaJam rules §17).
- [ ] Tag the commit `megajam-submission`.

## After the MegaJam (to 1 Dec)

Pick from `design.md`, "Out of the MegaJam build", after seeing what players say. Likely order: taking over returning ants, the visible fungus garden and skill tree, several trees. The bird already shipped in the MegaJam build.

## If time runs short

Cut in this order, last item first to go:

1. Slate screens (keep canvas)
2. Distant trunks, ground detail
3. Bark relief geometry (keep shader-only bark)
4. Day scaling beyond a single number
5. Trail riding at forks (always follow the newest path)
6. Two of the five upgrades (keep Mandibles, Grip, Brood)

Never cut: the feel of movement and camera, the cut-size trade-off, unattended return with the shift, wind, a packaged build that runs, and the video.
