# MegaJam submission form: draft answers

<!-- Field by field, in the order of the itch.io form (saved in docs/ship/temp/ on 14 Nov). Everything marked Private is seen only by the organisers. The owner submits in person by Sun 15 Nov 23:00 CET at the latest (the MegaJam's own deadline is Mon 16 Nov 02:59 CET). Square brackets are still open. -->

**Project:** the itch page "Leafline - 2026 Epic AI MegaJam" (publish it first; the form only lists published projects).

**Team Name:** inventivetalent

**Game Name:** Leafline

**How many people on your team?** 1 (solo) <!-- Owner, 15 Nov: the UI art is not in the MegaJam build, so Friend B does not join; solo. -->

**List each team member's name & email address** (private): Haylee Schäfer, mail@inventivetalent.org

**Are you entering as a student or student team?** (private): N/A

**Link to gameplay footage (30-60 sec)** (private): [VIDEO URL]

**Gameplay instructions:**

> Windows 10 or 11. Unzip and run Leafline/AntTreeGame.exe. Keyboard and mouse or any gamepad; the pause menu lists all controls.
>
> You're a leafcutter ant. Climb from the nest to a leafy branch (WASD and mouse, or the sticks); your ant leaves a scent trail. Hold Cut (E or left mouse button, pad X) at a leaf to choose the size of the piece, then let go: bigger pieces are worth more but slower and riskier in the wind. Send the ant home (Q, pad Y). It follows the trail by itself while you take over the next worker, who can ride the trail back up (hold F, pad RB). When a gust comes, hold Grip (Space, pad A) or get into shelter; hold Tab (pad LB) for the wind map, where green bark is sheltered and red is exposed. Cut leaves don't grow back, and leaves higher up are worth more. At the end of the day, spend leaf mass on upgrades in the nest. Two recruits follow your trails from day 1 and harvest around your cuts; Brood adds more.
>
> From day 2, a bird swoops at ants carrying pieces on exposed bark. It never touches them; its wing gust knocks them off. Grip or get into shelter when it dives. Drop your piece in the open (G, pad B) to draw it away.

**What platform is your game built for?** Windows 10 and Windows 11 <!-- the form takes one; pick Windows 11 and mention 10 in the instructions, as above -->

**Private version island code:** leave empty (not a UEFN game).

**Pre-jam content list** (private):

> Game content: none. The project was created on 12 November 2026 and all game content was made from then on, except the CC0 button prompt icons ("Xelu's FREE Controller Prompts" by Nicolae (Xelu) Berbece, updated by Paul Paun, https://thoseawesomeguys.com/prompts/), which are a sourced asset.
>
> Made before 12 November:
> - An earlier prototype of a tree-climbing ant game (1 and 2 November, a separate project). No files from it are used: no code, assets, materials or models. What carried over is knowledge: my idea notes (docs/concept-main.md in the repository, unchanged) and workflow and engine lessons from the prototype's notes. The planning agent read those notes, its commit list and its file listing, not its source code.
> - Two helper scripts adapted from another of my projects, with no game content: Scripts/unreal-lock.sh (lets only one or two Unreal processes run on the machine at a time) and Scripts/RunGame.bat (starts the game from the editor binaries).
> - Agent setup: Claude Code with my global instructions and a few general-purpose skills (writing style, planning interviews, a Codex hand-off workflow), the Codex CLI and its Claude Code plugin, herdr (a terminal multiplexer for running several agents), and the MCP servers blender-mcp (Blender add-on) and earshot (audio analysis). Epic's Model Context Protocol plugin ships with the engine. [OWNER: check this list against your setup; add or remove anything.]
> - The level started as the engine's default template map, with the floor and props removed.

**Dev log link (Glass Box modifier)** (private, optional): [DEV LOG URL] <!-- docs/devlog.md in the public repository, or a copy on itch. Owner: check every statement first; it is written in your voice. -->

**Tell us about your take on the theme and how it influenced your game?** (private):

> Director's Cut works on two levels in Leafline. You direct the colony: you lay the routes with your scent trail, and recruits walk them and harvest around your cuts without you, so where you lay a route decides how much comes home. And you choose the cut: every piece's size trades leaf mass against carrying speed and wind risk, and the recruits copy your choice. The end-of-day recap is titled "today's cut": what your trails brought home and what the wind took. Behind the game, one person directed AI agents through planning, building and review, deciding what went in and what didn't.

**Which modifier categories?** (optional): Solo Driver; Glass Box (only with the dev log link); Render Royalty [OWNER: your call].

**NVIDIA RTX Award:** no. <!-- The game uses hardware ray tracing when available but none of the listed NVIDIA features. -->

**What tools did your team use to make your game?** (private, optional):

> Unreal Engine 5.8 (5.8.4), with Epic's Model Context Protocol plugin and ProceduralMeshComponent. Claude Code (Anthropic Claude models) for planning and most of the programming, driving the Unreal editor through the Model Context Protocol plugin. Codex (OpenAI) for concept and key art, code reviews and some programming. Blender 5.1 with the blender-mcp add-on for the scripted meshes (ant, nest mound, bird). Python for asset scripts and sound synthesis, with earshot for checking the sounds. Visual Studio 2022 Build Tools. Sourced asset: Xelu's CC0 button prompt icons. No Hyper3D Rodin.
>
> [IF MUSIC SHIPS: one sentence on how the background music was made]

**Which OpenAI models or products did you use, and how?** (private, optional):

> Codex (OpenAI) during development only, nothing at runtime. It made the concept art and the itch page key art, reviewed code (for example the bird's timing logic and a diagnosis of why ants got stuck at branch junctions), and wrote some features: the save game and upgrades, runtime audio, the HUD restyle and the video camera tools. Claude orchestrated it alongside Claude agents; Codex's quota and its separate perspective made it useful for reviews and self-contained jobs.

**Contact:** your call.

**Official rules:** Yes (required).

## Before you submit

- Build the submission zip with `Tools/release_build.sh` (it packages, zips and checks the build, and stops at 1 GB). After uploading, check the zip downloaded back from itch with `Tools/release_build.sh --check <zip>`.
- Publish the itch page, upload the final zip, and set the page's Windows download.
- Screenshots: use the JPEG copies from `Tools/itch_shots.sh` (the `jpg/` folder), since itch takes at most 3 MB per screenshot.
- Fill the video link; check the dev log link opens while signed out.
- Submit by Sun 15 Nov 23:00 CET at the latest.
- Tag the submitted commit `megajam-submission`.
