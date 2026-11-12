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
- [ ] First commit (done). Create the **private** GitHub repo `Leafline` and push. The owner has approved a private repo; making it public needs a fresh yes on Sunday.

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
- [x] Codex code review of M1–M3; fixed (docs/reviews). M4 screens not yet reviewed.

**Done when:** a fresh save can be played to the end card in about 15 minutes, and quitting and relaunching keeps the colony.

## M5: Presentation

Order by visible impact per hour:

- [x] Bark material (plates, fissures, moss). Bark relief in field and mesh not done (cut list item 3).
- [x] Leaf material: shape, veins, backlight, bite edges, wind sway.
- [ ] Lighting pass against concept image 01: sun angle per shift, volumetric shafts, fog, mild depth of field, exposure.
- [x] Final ant mesh with shader gait; carried fragment mesh (placeholder disc).
- [x] Trail shader (soft-edged translucent cyan; no flow yet).
- [x] Ground material. Distant trunks not done.
- [x] Audio: runtime WAV loader (no import tool), all sounds hooked up. Mix not yet checked by ear.
- [x] Onboarding prompts for the first day; recap screen titled as the day's cut.
- [ ] Screens moved to Slate if time allows.

**Done when:** a screenshot from play, set beside concept image 01, clearly shows the same game, and a new player reaches their first cut without being told the controls.

**Feature freeze: Saturday 23:00.** After this, only fixes.

## M6: Stretch (only if M0–M5 are complete before the freeze)

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
