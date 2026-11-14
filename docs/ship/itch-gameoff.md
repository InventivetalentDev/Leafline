# itch page text: Game Off 2026

<!-- Sunday draft, refreshed 14 Nov 2026. Resolve owner placeholders and each one-line [IF ... SHIPS] item against the final package; remove its prefix if kept, or delete the whole line if cut. See open-questions.md. -->

**Title:** Leafline [OWNER: confirm final title]
**Tagline / short description:** You are a leafcutter ant on a giant tree. Send a loaded ant home and shift to the next worker.
**Genre / tags (suggested):** Singleplayer, 3D, Procedural Generation, Ants, Unreal Engine, AI-assisted
**Platform:** Windows
**Download:** `inventivetalent_Leafline.zip` ([FINAL ZIP SIZE]; unzip and run `Leafline/AntTreeGame.exe`)
**Gameplay video:** [VIDEO URL]
**This page:** [PAGE URL]
**Source:** [PUBLIC REPO URL — owner must make the repo public before Game Off submission; expected https://github.com/InventivetalentDev/Leafline]

---

## Pitch

You are a leafcutter ant on an enormous tree. Climb, lay a scent trail and cut a piece from a leaf. A bigger piece is worth more, but it is slower to carry and catches the wind like a sail. Send the loaded ant home and shift to the next scout while recruits harvest along your trails.

## SHIFT

- **Shift between ants:** sending a loaded scout home hands you the next worker while the first follows its trail home.
- **Work a shift:** three scout trips make a day, with recruits carrying on along the lines you lay. The sun steps from morning to evening with each scout shift.
- **Shift orientation:** walk up a trunk, round a limb or under a branch while the camera eases behind you.
- **Shift trees:** at the second and third milestones, the colony moves to a taller tree on the next day.

## How to play

1. Leave the nest at the base of the trunk. The ant lays a scent trail as it walks; climb to a leafy branch and sprint while stamina lasts.
2. At a leaf's base, hold Cut to choose the piece's size, then release. The arc's colour shows the wind risk for that size. Bigger pieces are worth more, slow the ant and catch more wind; pale young leaves higher up are richer.
3. Send the loaded ant home. It follows the scent network home by itself, cutting out loops, while you shift to the next worker at the nest. Three scouts go out each day. You can also walk a piece home yourself.
4. Buy Brood at the nest to add recruits. Once you have recruits, a scout's piece on its way home makes a line: recruits walk your route, cut the same size from nearby leaves and carry the pieces home while you scout. Patches run dry, so keep laying lines. Hold Follow near a trail to ride it; movement input leaves it.
5. When a gust approaches, hold Grip or reach shelter. Hold Tab / LB for the wind map: dots on nearby bark run from green (sheltered), through amber (partly exposed), to red (exposed). A blown-off ant loses its piece, falls and walks home empty-handed; nobody dies. Unattended ants never grip, so their safety depends on the route and cut size you chose.
6. When the last scout is home or heading home, watch the harvest at five times speed, or skip it. With no recruit trips left, the last workers' walk home also fast-forwards and can be skipped.
7. Read the day's cut, then spend leaf mass on Mandibles, Strength, Grip, Stamina and Brood.
8. The colony keeps one tree across days. Cut leaves stay cut and never grow back, so the low boughs run out and you climb higher. Yesterday's trails last one more day, faint and still ridable; the wind changes direction each day.
9. At the second and third colony milestones, the next day starts on a new, taller tree with richer leaves and stronger gusts. The third milestone also shows an end card; you can keep playing afterwards.

Progress, the day's leaf cuts and trails are saved at day end. Quitting mid-day restarts that day from dawn without its haul or cuts.

<!-- Optional Saturday copy: each feature below occupies one physical line. -->

[IF BIRD SHIPS] A bird hunts on the wind, choosing laden ants on exposed bark. It never touches an ant: its wing gust knocks the ant off, the piece flutters away and the ant walks home unharmed. Reach shelter or hold Grip when it dives; gripping holds.

[IF BAIT SHIPS] Drop your carried piece on exposed bark to draw the bird away for a while; a piece in shelter is ignored. Keyboard: [BAIT KEY]; pad: [BAIT BUTTON]. <!-- Requires the bird and tier 2; fill both bindings from the landed controller and PromptIcons.cpp before keeping this line. -->

[IF WIND CUES SHIP] Streaming specks and streaks show an approaching gust and how exposed the ant's own spot is, alongside the wind map and HUD.

[IF ANT LOOK SHIPS] The ant has a richer copper-rust colour and broad shell highlights, keeping its colour on shaded bark without an outline-like rim light.

[IF FOREST FLOOR SHIPS] Fallen leaves and sparse grass surround the colony's clearing, giving the forest floor an ant-sized sense of scale while keeping the nest and its trails clear.

## Controls

| Action | Keyboard and mouse | Gamepad (Xbox names) |
|---|---|---|
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Sprint (hold) | Left Shift | Left trigger (LT) |
| Cut (hold, release to finish) | E or left mouse button | X (left face button) |
| Grip (hold) | Space | A (bottom face button) |
| Send home | Q | Y (top face button) |
| Follow a trail (hold) | F | Right bumper (RB) |
| Cycle worker view (tap, switches on release) | Tab | Left bumper (LB) |
| Wind map (hold during scouting) | Tab | Left bumper (LB) |
| Skip harvest / last workers' walk home | Space, E or left mouse button | A or X |
| Next harvest view (when lines exist) | Tab | Left bumper (LB) |
| Pause / resume | Esc or P | Start / Menu |
| Choose a menu row | W / S or mouse hover | Left stick up / down |
| Confirm / continue / buy | E, Space or left mouse button | A or X |
| Back in menus | Q | Y |
| Change an option | A / D or click its − / + control | Left stick left / right |

Prompts follow the device used last. Every pad shows Xbox-style icons, including PlayStation pads (X = left face button, A = bottom, Y = top). Xbox, PS4 and PS5 pads are supported. The right stick's vertical look is inverted by default; the pause menu's invert option flips vertical look for both mouse and stick.

Movement follows the direction of the branch on screen. The camera direction is read when you start moving and held while you keep moving; release briefly to read it again. Diagonals work on twigs. The pause menu also has mouse sensitivity, volume and graphics quality.

Q / Y quits from the main pause page; from Options it goes back. Tapping Tab / LB changes the worker you watch; it does not cancel an unattended return.

## What is procedural

The tree, bark, leaves, ground, sky, scent trail and wind effects are generated by code and shaders, with no textures in the world and no hand-made tree models. The ant and nest mound are meshes generated by Blender Python scripts; the ant's legs animate in its material. The ant has a neck joining head and thorax, and its feet sit on the bark.

The look passes added warm ridged bark and moss, a closer camera with more room to look up, a bark cutaway that opens when wood hides the ant and keeps a faint dithered layer, a dug-soil nest mound, and larger HUD text on backed panels. Scent trails are a soft cyan haze; their brightness shows how much their patch has left.

Sounds and music are synthesised by script. During fast-forward, effects are quieter and repeated starts are limited; the end jingle plays afterwards at full volume. The only sourced images are the CC0 button prompts in the HUD.

## Tools and AI use

One person directs AI agents to build the game; most programming is agent-written. Concept and key art are AI-generated too.

- Unreal Engine 5.8 (development machine: 5.8.4).
- Claude (Anthropic): planning and most programming, driving the Unreal editor through Epic's Model Context Protocol plugin.
- Codex (OpenAI): concept and key art, code reviews, and programming contributions including saves, upgrades, runtime audio and the HUD restyle.
- Blender 5.1: scripted mesh generation.
- Python scripts: procedural asset generation and audio synthesis; audio checked with the earshot analysis tool.

## Sourced assets

Button prompt icons: [“Xelu's FREE Controller Prompts”](https://thoseawesomeguys.com/prompts/) by Nicolae (Xelu) Berbece, updated by Paul Paun (2021), CC0 1.0 (public domain). Resized light keyboard/mouse keys and Xbox Series buttons, used only in the HUD. See `docs/sourced-assets.md`.

## Earlier prototype

On 1–2 November the author made a quick prototype of a tree-climbing ant game in a separate project (`TreeAntGame`). No files from it are used here: no code, assets, materials or models. This project was created on 12 November 2026; its game content was made from that day on, apart from the disclosed CC0 prompt icons. What carried over was knowledge: the author's idea notes (`concept-main.md`), and workflow and engine lessons from the prototype's notes. The planning agent read its `CLAUDE.md` and to-do list, not its source code, and used those lessons in the plan; it also read the commit list and file listing. <!-- Owner: confirm this disclosure against docs/devlog.md and docs/design.md before publishing. -->

Pre-jam tooling is disclosed in the dev log: `Scripts/unreal-lock.sh` and `Scripts/RunGame.bat` were adapted from another project and contain no game content. The level began as the engine's default template map with floor and props removed, retaining sky and lighting. AI-generated content is not counted as sourced content under the MegaJam rules recorded in the design notes.

## Updates

The Sunday build is also entered in the MegaJam, whose page and build are frozen at submission. This Game Off page may receive dated updates until Tue 1 Dec 2026, 22:37 CET. [OWNER: choose which later work, if any, to announce; no roadmap is promised here.]

## Links and credits

- Design and direction: inventivetalent (solo).
- Dev log: [DEV LOG URL — public repo file or a copy on itch].
- MegaJam page: [MEGAJAM PAGE URL].
- Repository licence: [LICENCE — owner to choose and add to the repo].
- Windows Development build. [OWNER: confirm the final download and fresh-folder launch.]

<!-- [IF VIDEO KIT SHIPS] Recording only: after the kit passes its packaged checks, use Leaf.Cinematic 0|1, Leaf.Shot <name> [seconds] (orbit/crane/dolly/reveal/follow), Leaf.ShotList <name> <name> ... and Leaf.Slomo <factor> (1 = normal); confirm final syntax in docs/tech.md. This is an editor note, not itch page copy. -->
