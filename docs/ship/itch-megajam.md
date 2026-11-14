# itch page text: Epic AI MegaJam (frozen at submission)

<!-- Owner: fill the bracketed placeholders. Keep or delete the two bird lines after the 20:00 decision. -->

**Title:** Leafline
**Short description:** You're a leafcutter ant on a giant tree. Cut leaves, lay scent trails and send the colony after you.
**Tags (suggested):** Singleplayer, 3D, Procedural Generation, Ants, Unreal Engine
**Platform:** Windows
**Download:** `inventivetalent_Leafline.zip` ([FINAL ZIP SIZE]). Unzip it and run `Leafline/AntTreeGame.exe`.
**Video:** [VIDEO URL]

---

## About

You're a leafcutter ant on an enormous tree. Climb, lay a scent trail and cut a piece from a leaf. A bigger piece is worth more, but it slows you down and catches the wind like a sail. Send your ant home, take over the next one, and let your recruits follow your trails and harvest along them.

The colony keeps its tree from day to day. Cut leaves don't grow back, so you climb higher and into stronger wind until the colony moves to a new, taller tree.

## The jam theme: Director's Cut

Leafline was made for the Epic AI MegaJam 2026, whose theme is "Director's Cut". In the game:

- **You direct the colony.** Recruits walk the routes you lay and harvest around your cuts without you.
- **You choose the cut.** Each piece's size trades leaf mass against speed and wind risk.
- **The day's cut** is the recap: what your lines brought home and what the wind took.

## How to play

1. Climb from the nest to a leafy branch. Your ant leaves a scent trail as it walks.
2. Hold Cut at a leaf to choose the size of the piece, then let go.
3. Send the ant home. It follows the trail by itself while you take over the next worker.
4. When a gust comes, hold Grip or get into shelter. An ant that's blown off loses its piece and walks home.
5. At the end of the day, spend leaf mass on upgrades. Brood adds recruits who work your trails.

Hold Tab (LB on a pad) for the wind map: green bark is sheltered, red is exposed.

[IF BIRD SHIPS] A bird hunts on the wind. It goes for ants carrying leaves on exposed bark. It never touches them; its wing gust knocks them off. Grip or get into shelter when it dives.

[IF BAIT SHIPS] Drop your piece in the open (G, or B on a pad) to draw the bird away for a while.

## Controls

| Action | Keyboard and mouse | Gamepad (Xbox names) |
|---|---|---|
| Move / look | WASD / mouse | Left / right stick |
| Cut (hold) | E or left mouse button | X |
| Send home | Q | Y |
| Grip (hold) | Space | A |
| Follow a trail (hold) | F | RB |
| Wind map (hold) / switch ant (tap) | Tab | LB |
| Sprint (hold) | Left Shift | LT |
| Pause | Esc | Start |

The pause menu lists all controls and has options for sensitivity, volume and graphics.

## How it's made

The tree, bark, leaves, forest floor, sky, scent trails and wind are generated with code and shaders. The world has no textures and no hand-made tree models. The ant, the nest mound and the bird are meshes made by Blender Python scripts. The sounds and music are synthesised by scripts.

One person directed AI agents to make the game. Most of the code is written by AI, and the concept and key art are AI-generated.

- Unreal Engine 5.8
- Claude (Anthropic): planning and most of the programming, including driving the Unreal editor through Epic's Model Context Protocol plugin
- Codex (OpenAI): concept and key art, code reviews and some programming
- Blender 5.1 and Python: the scripted meshes and the sound synthesis

## Sourced assets

Button prompt icons: ["Xelu's FREE Controller Prompts"](https://thoseawesomeguys.com/prompts/) by Nicolae (Xelu) Berbece, updated by Paul Paun, CC0 1.0. They're the only content in the game that wasn't made for it.

## Made before the jam

On 1 and 2 November I made a quick prototype of a tree-climbing ant game in a separate project. Leafline uses no files from it: no code, assets, materials or models. This project started on 12 November, and all its game content was made from then on, except the CC0 icons above. What carried over was knowledge: my idea notes, and workflow and engine lessons from the prototype's notes. The planning agent read those notes, its commit list and its file listing, but not its source code.

Two helper scripts are older than the jam and contain no game content: `Scripts/unreal-lock.sh` and `Scripts/RunGame.bat`. The level started as the engine's default template map, with the floor and props removed.

<!-- Owner: check this section against docs/devlog.md before publishing. It is written in your voice. -->

## Credits and links

- Made by inventivetalent
- Playtesting and design feedback: [FRIEND A]
- Dev log: [DEV LOG URL]
- Game Off version: [GAME OFF PAGE URL]
- Source code: [REPO URL, once the repository is public]
