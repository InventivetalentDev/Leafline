# Leafline

<!-- Source of the repository's README.md (copied there on 15 Nov). Owner: fill the bracketed placeholders, then copy again. -->

You're a leafcutter ant on an enormous tree. Climb, lay a scent trail and cut a piece from a leaf: a bigger piece is worth more, but it slows you down and catches the wind. Send your ant home, take over the next one, and let your recruits harvest along your trails.

Made by inventivetalent for the [Epic AI MegaJam 2026](https://itch.io/jam/2026-epic-ai-megajam) (theme: Director's Cut) and [GitHub Game Off 2026](https://itch.io/jam/game-off-2026) (theme: SHIFT), directing AI agents that wrote most of the code.

- Play it: [itch.io page (Epic AI MegaJam)](https://inventivetalent.itch.io/leafline-megajam)
- Video: [YouTube](https://www.youtube.com/watch?v=lo9zXlSoQFE)
- Dev log: [`docs/devlog.md`](docs/devlog.md)

## How to play

1. Climb from the nest to a leafy branch. Your ant leaves a scent trail as it walks.
2. Hold Cut at a leaf to choose the size of the piece, then let go.
3. Send the ant home. It follows the trail by itself while you take over the next worker.
4. When a gust comes, hold Grip or get into shelter. Hold Tab (LB on a pad) to see which bark is exposed.
5. At the end of the day, spend leaf mass on upgrades in the nest. Two recruits work your trails from day 1, and Brood adds more.

Cut leaves don't grow back, and leaves higher up are worth more, so the colony climbs into stronger wind until it moves to a new, taller tree.

From day 2, a bird swoops at ants carrying pieces on exposed bark. It never touches them; its wing gust knocks them off. Drop your piece in the open (G, or B on a pad) to draw it away.

The pause menu lists all controls (keyboard and mouse, or any gamepad).

## Build from source

You need:

- Windows with Unreal Engine 5.8 (developed with 5.8.4)
- Visual Studio 2022 Build Tools with MSVC 14.44
- Git and Git LFS
- Python 3 (only for the zip helper)

The project enables these plugins from the engine: ModelContextProtocol, AllToolsets, ProceduralMeshComponent, GameInputWindows, GameplayStateTree and ModelingToolsEditorMode.

1. Clone the repository with Git LFS installed, then run `git lfs pull`.
2. The scripts in `Scripts/` contain the author's paths (`C:\Program Files\Epic Games\UE_5.8` and `D:\UnrealProjects\AntTreeGame`). Change them to match your machine.
3. Close the editor and run `Scripts\build_editor.bat`.
4. Open `AntTreeGame.uproject`. The game starts in `/Game/Leafline/Maps/L_Main`.
5. To make a build, run `Scripts\package_win64.bat` and then `python3 Tools/zip_build.py`. The zip lands in `Build/`. `Tools/release_build.sh` (run from WSL) does both and checks the build ([`docs/tools.md`](docs/tools.md)).

A clean build on another machine hasn't been tested yet.

## How it's made

The tree, bark, leaves, forest floor, sky, scent trails and wind are generated with C++ and shaders; the world has no textures and no hand-made tree models. The ant, the nest mound and the bird are meshes made by Blender Python scripts in `Tools/blender/`. The sounds are synthesised by `Tools/audio/make_audio.py`.

The background music is three synthesised loops, one per day in turn (golden pads, kalimba drift, music-box arpeggio), rendered by `Tools/audio/make_music.py`.

Sounds and HUD icons load at runtime from `Content/Leafline/AudioRaw/` and `Content/Leafline/UIRaw/`. Debug console commands start with `Leaf.`; [`docs/tech.md`](docs/tech.md) lists them and describes the architecture.

| Folder | Contents |
|---|---|
| `Source/AntTreeGame/` | The game's C++ |
| `Content/Leafline/` | Maps, materials, meshes, runtime sounds and HUD data |
| `Tools/`, `Scripts/` | Mesh, material and sound generators; test, build and packaging scripts |
| `Art/` | Generated sounds, Blender files, concept art |
| `docs/` | Design, technical notes, plan, dev log, briefs for the AI agents |

## AI use and credits

One person directed AI agents to make the game. Most of the code is written by AI, and the concept and key art are AI-generated.

- Made by inventivetalent
- UI panel art, the frames and backgrounds of the menu screens and the HUD boxes: [Cookiehero](https://cookiehero.itch.io/). The rest of the interface (layout, text, icons, meters) was made with code and AI tools.
- Playtesting and design feedback: [iCeeker](https://iceeker.itch.io/)
- Claude (Anthropic): planning and most of the programming, including driving the Unreal editor through Epic's Model Context Protocol plugin
- Codex (OpenAI): concept and key art, the HUD icons, code reviews and some programming
- Unreal Engine 5.8, Blender 5.1, Python

The button prompt icons are ["Xelu's FREE Controller Prompts"](https://thoseawesomeguys.com/prompts/) by Nicolae (Xelu) Berbece, updated by Paul Paun, CC0 1.0. They're the only content in the game that wasn't made for it ([`docs/sourced-assets.md`](docs/sourced-assets.md)).

## Made before the jam

On 1 and 2 November I made a quick prototype of a tree-climbing ant game in a separate project. Leafline uses no files from it: no code, assets, materials or models. This repository started on 12 November, and all its game content was made from then on, except the CC0 icons above. What carried over was knowledge: my idea notes ([`docs/concept-main.md`](docs/concept-main.md)), and workflow and engine lessons from the prototype's notes. The planning agent read those notes, its commit list and its file listing, but not its source code.

Two helper scripts are older than the jam and contain no game content: `Scripts/unreal-lock.sh` and `Scripts/RunGame.bat`. The level started as the engine's default template map, with the floor and props removed.

## Licence

MIT, see [`LICENSE`](LICENSE). The button prompt icons are "Xelu's FREE Controller Prompts" by Nicolae (Xelu) Berbece, released under CC0 1.0.
