# MegaJam submission form: draft answers

Fields are the ones listed in `docs/plan.md`, M7. **The owner submits in person** (MegaJam Official Rules §17); nothing here is sent by an agent. Deadline: our Sun 15 Nov 22:00 CET; hard deadline Mon 16 Nov 02:59 CET.

| Field | Answer |
|---|---|
| Full name | ______________________ (owner fills in) |
| Epic account email | ______________________ (owner fills in) |
| Game name | Leafline |
| Team / entrant name | inventivetalent (solo) |
| Build | `inventivetalent_Leafline.zip`, packaged Windows Development build (size to confirm after the final zip: 463 MB on 13 Nov 11:28; limit 1 GB) |
| Gameplay video | ______________________ (owner records and uploads: 30-60 s, 1920x1080 or higher, English) |
| Dev log link | ______________________ (`docs/devlog.md` in the repo if it is public by then, otherwise the dev log posted on the itch page) |
| itch page | ______________________ |
| Engine | Unreal Engine 5.8 |

## Short description

You are a leafcutter ant on an enormous tree. You climb, lay a scent trail and cut a piece out of a leaf. A bigger piece is worth more, but it is slower to carry and catches the wind like a sail. Send the loaded ant home and it walks back along your trail by itself while you take over the next worker. Recruits follow your route and carry more pieces home. Leaf mass buys upgrades between days.

## How the game uses the theme (Director's Cut)

You decide how much to cut from each leaf: that size is the central trade-off of every trip. Your scent trail is the direction you give the colony: when a piece is on its way home, recruits walk your route without you, cut pieces from the leaves around your cut and carry them home. Where you lay the route (the sheltered side or the windy side) and how big you cut decide how many of them get home. In the words of the theme post: the route is the vision, sending workers along it is delegating, and the size of each cut is deciding what goes in.

## Tools used

- Unreal Engine 5.8.
- Claude (Anthropic): planning and most of the programming, driving the Unreal editor through Epic's Model Context Protocol plugin.
- Codex (OpenAI): concept art, key art and code reviews.
- Blender 5.1: a Python script generates the ant mesh.
- A Python script synthesises all audio; the earshot audio analysis tool was used to check it.

All game content except the CC0 button prompt icons (see Sourced assets) was created by the owner directing these tools on or after 12 November 2026. The tree, bark, leaves, ground, sky, trail and effects are procedural geometry and shaders, with no textures and no hand-made tree models. <!-- Owner: confirm the MegaJam rules accept a pre-existing CC0 asset that is listed. -->

## Sourced assets

Button prompt icons: "Xelu's FREE Controller Prompts" by Nicolae (Xelu) Berbece, updated by Paul Paun, CC0 (https://thoseawesomeguys.com/prompts/). 26 icons (keyboard keys, mouse, Xbox-style buttons), resized. No marketplace content, no other third-party samples, textures or models. (Content made with AI tools is not counted as sourced.)

## Note about the earlier prototype

Word for word from `docs/devlog.md`:

> **Earlier prototype.** On 1–2 November I made a quick prototype of a tree-climbing ant game in a separate project (`TreeAntGame`). No files from it are used here: no code, assets, materials or models. This project was created on 12 November and all game content in it was made from that day on. What did carry over is knowledge: my idea notes (`concept-main.md`), and workflow and engine lessons from that prototype's notes, which the planning agent read (its `CLAUDE.md` and to-do list, not its source code) and used when writing the plan.

The dev log marks this note as drafted by Claude in the owner's voice, with "Owner: check every statement, especially 'Earlier prototype'". It is unchecked.

The dev log also has a "Pre-jam tooling" note, in case the form has room for it (word for word):

> **Pre-jam tooling.** Two helper scripts come from another of my projects and predate the jam, as the rules allow for tooling: `Scripts/unreal-lock.sh` (keeps one Unreal process running on the machine at a time) and `Scripts/RunGame.bat` (starts the game standalone). They were adapted to this project; they contain no game content. The level `L_Main` started as a copy of the engine's own default template map (sky, sun, sky light, fog, clouds); its floor and props were removed.

## Checks before submitting

- Video is 1920x1080 or higher, English, shows no trademarks or logos.
- Zip is named `inventivetalent_Leafline.zip` and is under 1 GB.
- Game text, audio and video are English; content is suitable for teens.
- No trademarks or logos in the game, video or page art, other than the Unreal Engine logo used per Epic's branding guidelines.
