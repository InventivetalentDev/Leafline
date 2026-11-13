# Leafline: build plan

Ordered work from an empty template to two jam submissions. Read `design.md` (what) and `tech.md` (how) first. Tick items here as they land, and add a line to `devlog.md` at each milestone.

## Clock

| Point | Time (CET) |
|---|---|
| Planning finished | Thu 12 Nov, late morning |
| **Feature freeze** | **Sat 14 Nov 23:00** |
| **Our submission deadline** | **Sun 15 Nov 22:00** |
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
- [ ] Lighting pass against concept image 01: sun angle per shift, volumetric shafts, fog, mild depth of field, exposure.
  - Pass 1 (Thu evening): `FLookTuning` on the day director, tunable live with `Leaf.Tune look`. Sun warmth per shift (4600/5600/3800 K), haze based at the ground, stronger shafts, sun glow in the fog. Leaves 1.5 times as dense; the distant grove has leaf crowns. Reference shots with `Tools/ref_shots.sh`. Still open: ground colour (M_Ground, needs the editor), leaves dithering near the camera, exposure.
- [x] Final ant mesh with shader gait; carried fragment mesh (placeholder disc).
- [x] Trail shader (soft-edged translucent cyan; no flow yet).
- [x] Ground material. Distant trunks with leaf crowns (scenery only).
- [x] Audio: runtime WAV loader (no import tool), all sounds hooked up. Mix not yet checked by ear.
- [x] Onboarding prompts for the first day; recap screen titled as the day's cut.
- [ ] Screens moved to Slate if time allows.
- [x] Options in the pause menu (Thu evening): mouse sensitivity, invert look, volume, graphics quality (Low to Epic; Low turns Lumen off). Keys, pad and mouse; saved in GameUserSettings.ini.
- [x] Wind readability, first pass: an arrow by the wind gauge shows which way it blows relative to the view, "Here: sheltered / partly exposed / exposed" under it, and the gust warning says to grip or get to the side away from the arrow (or that you are sheltered).
- [x] Cut sequence, first pass: sawing, a tear line on the leaf, the piece swinging onto the back.
- [x] Small moments (Thu evening): at the nest the ant walks into the entrance and sinks out of sight while its piece drops in; a banner "Worker 2 of 3 sets out" (the day's first also names the tree); when blown off, the piece flutters away on the wind instead of vanishing, and a fall camera watches from outside and above.
- [x] Fix: blown-off ants flew straight through the trunk (on the windward face the wind points into the wood) and the camera showed only bark. The fall now peels off the surface and glances off wood on the way down.
- [x] Tree character (Thu evening): day 1 is the classic tree; later days take turns as a spreading giant (longer, flatter limbs), a tall spire (taller, narrower, more and steeper limbs) and a leaning elder (lean, wobble, six roots). Every tree has one broken limb low down, a bare stub, as a landmark. The name shows in the HUD and on the recap. Walking surface checked on all four (`Leaf.MeshGap` worst 0.00 cm). `Leaf.Tune day TreeShapeOverride <0-3>` forces one.
- [x] Owner playtest 1 (Thu): all seven notes fixed (`docs/playtest-notes.md`). Bark look and the camera cutaway await the owner's verdict; leaves are not dithered yet.

**Done when:** a screenshot from play, set beside concept image 01, clearly shows the same game, and a new player reaches their first cut without being told the controls.

**Feature freeze: Saturday 23:00.** After this, only fixes.

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
- [ ] Acceptance tests (`Tools/line_test.sh`, seed 517192, day 1, pool 4 unless noted; Thu night):
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
- [x] Economy numbers from the measured trees (`Tools/econ_sim.py`, `docs/economy.md`): one-tree thresholds 25 / 155 / 270 with recruits, 5 / 60 / 95 by hand.
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
- [ ] Owner's verdict on all of the above, the line and one tree.
- [x] Key and pad icons: Xelu's CC0 pack, light keys and Xbox-style buttons for every pad (owner's choice), drawn from tokens in HUD text for the device used last (`UI/PromptIcons.*`, `Leaf.PadIcons`).
- [x] Package and zip from 6865416 (13 Nov 13:10, 463.6 MB, 48 files, no saves); packaged smoke test: two days, wind map, icons load from the pak, no fatal lines.
- [x] Package and zip from f9d8b6d (13 Nov 13:33, 463.6 MB, no saves): the stronger smoke ribbon. Packaged smoke test: two days, wind map (one 3.5 ms tick logged), no fatal lines.
- [ ] Still open: trail outlines, PS5 audio check. Aiming stays as it is (owner).

## M6: Stretch (only if M0–M5b are complete before the freeze; the bird waits for the owner to keep the line)

1. Bird: perches, notices exposed ants, swoops; dropping a food item distracts it. Needs a model (Blender script), simple state machine, and a food pickup. Nothing harmful is shown happening to the bird.
2. Taking control of an ant that is already returning.
3. Walking on leaf blades.

## M7: Ship (Sunday)

- [ ] Bug fixing from Saturday's playtest.
- [ ] Final packaged Windows **development** build. Run it from a path with spaces and from a fresh folder.
- [ ] Zip as `inventivetalent_Leafline.zip`; confirm it is under 1 GB.
- [ ] Owner records a 30–60 s gameplay video at 1920x1080 or higher and uploads it.
- [ ] Codex produces the cover image and screenshots are captured for the itch pages.
- [ ] Two itch pages: one for the MegaJam (frozen), one for Game Off.
- [ ] Submission form: full name and Epic account email, game name, video link, dev log link, tools used, sourced assets list, and the note about the earlier prototype (see `devlog.md`).
- [ ] Owner chooses a licence; add `LICENSE` and a `README.md`.
- [ ] Owner says yes, then make the GitHub repo public and add its link to the Game Off page. Do this **before** submitting, because the dev log link must open for the judges. If the repo stays private, post the dev log on the itch page instead.
- [ ] No trademarks or logos anywhere except the Unreal Engine logo.
- [ ] **The owner submits to both jams in person by 22:00 CET.** Agents must not submit (MegaJam rules §17).
- [ ] Tag the commit `megajam-submission`.

## After the MegaJam (to 1 Dec)

Pick from `design.md`, "Out of the MegaJam build", after seeing what players say. Likely order: bird and food, taking over returning ants, the visible fungus garden and skill tree, several trees.

## If time runs short

Cut in this order, last item first to go:

1. Slate screens (keep canvas)
2. Distant trunks, ground detail
3. Bark relief geometry (keep shader-only bark)
4. Day scaling beyond a single number
5. Trail riding at forks (always follow the newest path)
6. Two of the five upgrades (keep Mandibles, Grip, Brood)

Never cut: the feel of movement and camera, the cut-size trade-off, unattended return with the shift, wind, a packaged build that runs, and the video.
