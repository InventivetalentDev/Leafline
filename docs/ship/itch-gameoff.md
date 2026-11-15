# itch page text: Game Off 2026

<!-- Owner: fill the bracketed placeholders. This page can keep getting dated updates until Tue 1 Dec 2026, 22:37 CET; the MegaJam page stays frozen. -->

**Title:** Leafline
**Short description:** You're a leafcutter ant on a giant tree. Send your loaded ant home and shift to the next worker.
**Tags (suggested):** Singleplayer, 3D, Procedural Generation, Ants, Unreal Engine
**Platform:** Windows
**Theme colours (owner, 15 Nov, "Evening bark"):** BG `#16110D`, BG 2 `#211A14`, Text `#EFE6D2`, Link `#9BD36A`
**Download:** `inventivetalent_Leafline.zip` (0.44 GB). Unzip it and run `Leafline/AntTreeGame.exe`.
**Video:** [VIDEO URL]
**Source code:** [REPO URL]

---

## About

You're a leafcutter ant on an enormous tree. Climb, lay a scent trail and cut a piece from a leaf. A bigger piece is worth more, but it slows you down and catches the wind like a sail. Send your ant home, take over the next one, and let your recruits follow your trails and harvest along them.

The colony keeps its tree from day to day. Cut leaves don't grow back, so you climb higher and into stronger wind until the colony moves to a new, taller tree.

## The jam theme: SHIFT

Leafline is my entry for Game Off 2026, whose theme is "SHIFT":

- **You shift between ants.** When you send a loaded ant home, it walks back by itself and you take over the next worker.
- **A day is a set of shifts.** Three scouts go out each day, and the light moves from morning to evening with each one.
- **The ground keeps shifting.** You walk up the trunk, round branches and underneath them, and the camera follows.
- **The colony shifts trees.** At its milestones it moves to a new, taller tree.

## How to play

1. Climb from the nest to a leafy branch. Your ant leaves a scent trail as it walks.
2. Hold Cut at a leaf to choose the size of the piece, then let go.
3. Send the ant home. It follows the trail by itself while you take over the next worker.
4. When a gust comes, hold Grip or get into shelter. An ant that's blown off loses its piece and walks home.
5. At the end of the day, spend leaf mass on upgrades. Brood adds recruits who work your trails.

Hold Tab (LB on a pad) for the wind map: green bark is sheltered, red is exposed.

A bird hunts on the wind. It goes for ants carrying leaves on exposed bark. It never touches them; its wing gust knocks them off. Grip or get into shelter when it dives.

Drop your piece in the open (G, or B on a pad) to draw the bird away for a while.

## Controls

| Action | Keyboard and mouse | Gamepad (Xbox names) |
|---|---|---|
| Move / look | WASD / mouse | left / right stick |
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

The full source code is on GitHub: [REPO URL].

## Sourced assets

Button prompt icons: ["Xelu's FREE Controller Prompts"](https://thoseawesomeguys.com/prompts/) by Nicolae (Xelu) Berbece, updated by Paul Paun, CC0 1.0. They're the only content in the game that wasn't made for it.

## Updates

This game is also my entry for the Epic AI MegaJam, whose page stays as it was submitted on 15 November. This page gets the updates.

<!-- Owner: add dated update notes here after the jam, newest first. -->

## Credits and links

- Made by inventivetalent
- Playtesting and design feedback: [FRIEND A]
- Dev log: [DEV LOG URL]
- Epic AI MegaJam version: [MEGAJAM PAGE URL]
- Licence: [LICENCE]
