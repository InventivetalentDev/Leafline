# MegaJam submission form: draft answers

Refreshed Sat 14 Nov 2026. Fields follow `docs/plan.md`, M7. **The owner submits in person** (MegaJam rules §17). Our deadline is Sun 15 Nov 22:00 CET; the hard deadline is Mon 16 Nov 02:59 CET. Resolve placeholders and conditional lines against the final package before copying answers.

| Field | Answer |
|---|---|
| Full name | [OWNER'S FULL NAME] |
| Epic account email | [OWNER'S EPIC ACCOUNT EMAIL] |
| Game name | Leafline [OWNER: confirm final title] |
| Team / entrant name | inventivetalent (solo) |
| Build | `inventivetalent_Leafline.zip`, Windows Development build; [FINAL ZIP SIZE], under 1 GB |
| Gameplay video | [VIDEO URL — 30–60 s, 1920×1080 or higher, English] |
| Dev log link (optional) | [DEV LOG URL — public repo file or a copy on itch] |
| MegaJam itch page | [MEGAJAM PAGE URL] |
| Game Off itch page | [GAME OFF PAGE URL — separate page, updated independently] |
| Source | [PUBLIC REPO URL — expected https://github.com/InventivetalentDev/Leafline; owner must authorise publication] |
| Repository licence | [LICENCE — owner to choose; add LICENSE] |
| Engine | Unreal Engine 5.8 (developed with 5.8.4) |

## Short description

You are a leafcutter ant on an enormous tree. Climb, lay a scent trail and choose how much to cut from a leaf. A bigger piece is worth more, but it slows the ant and catches the wind like a sail. Send the loaded ant home and shift to the next scout while recruits follow your route, cut pieces from nearby leaves and carry them home. Patches run dry; leaf mass buys upgrades between days. The colony keeps its tree until it moves at a milestone, and cut leaves never grow back. The third milestone shows an end card; play continues afterwards.

## How the game uses Director's Cut

You choose the size of every cut, trading leaf mass against carrying speed and wind risk. Your scent trail directs the colony: once a scout's piece is on its way home, recruits walk your route without you and harvest the patch around your cut. Where you lay the route and how big you cut decide how many pieces come home. The recap is the day's cut.

The theme post's vision, delegation and final choices are the route, the workers following it and the size of each piece. The author directs the AI agents making the game too. This is an idea from the author's notes that had not found a home; the earlier prototype and what carried over are disclosed below.

## Current build details (use if the form has room)

Three scouts work each day. Brood buys the recruit pool; recruits never grip, so your route and cut size determine their wind risk. The harvest runs at five times speed and can be skipped. When no recruit trips remain, the last workers' walk home also fast-forwards and can be skipped. Effects are quieter during fast-forward and the end jingle plays afterwards.

Hold Tab / LB for the wind map; tap it to cycle worker views. Movement follows the branch's direction on screen, with working diagonals on twigs. Key and pad prompt icons follow the last device used and show Xbox-style buttons for every pad. Full keyboard and gamepad tables are in the README and itch drafts, checked against `LeaflinePlayerController.cpp` and `PromptIcons.cpp`.

The look passes added warm ridged bark and moss, a closer camera with a wider look-up range and a conditional dithered bark cutaway, a scripted dug-soil nest mound, and larger HUD text on backed panels. The ant's neck joins its head to its thorax, and its feet sit on the bark. Junction fillets are off for the jam unless a verified fix lands today; do not claim smooth filleted junctions.

The tree persists across days; yesterday's trails stay for one more day. At the second and third milestones, the next day starts on a new, taller tree with richer leaves and stronger gusts. Saves are written at day end; quitting mid-day loses that day's haul and cuts.

## Saturday additions: one line per feature, keep only if shipped

[IF BIRD SHIPS] A bird hunts on the wind, choosing laden ants on exposed bark. It never touches an ant: its wing gust knocks the ant off, the piece flutters away and the ant walks home unharmed. Reach shelter or hold Grip when it dives; gripping holds.

[IF BAIT SHIPS] Drop your carried piece on exposed bark to draw the bird away for a while; bait in shelter is ignored. Keyboard: [BAIT KEY]; pad: [BAIT BUTTON]. <!-- Requires bird tier 1 and bait tier 2; verify both bindings against landed code and prompt icons. -->

[IF WIND CUES SHIP] Streaming specks and streaks show an approaching gust and how exposed the ant's own spot is, alongside the wind map and HUD.

[IF ANT LOOK SHIPS] A richer copper-rust ant with broad shell highlights keeps its colour in shade without an outline-like rim light.

[IF FOREST FLOOR SHIPS] Fallen leaves and sparse grass surround the colony's clearing while keeping the nest and its trails clear.

## Tools and AI use

Leafline is made by one person directing AI agents. Most programming is agent-written; concept and key art are AI-generated too.

- Unreal Engine 5.8.
- Claude (Anthropic): planning and most programming, driving the Unreal editor through Epic's Model Context Protocol plugin.
- Codex (OpenAI): concept and key art, code reviews, and programming contributions including saves, upgrades, runtime audio and the HUD restyle.
- Blender 5.1: scripted mesh generation (the ant and nest mound).
- Python scripts: asset generation and audio synthesis, checked with the earshot audio analysis tool.

The game's code, procedural geometry, shaders and scripted assets were created during the jam, starting 12 November 2026. Disclosed exceptions are the sourced CC0 HUD prompts, pre-jam tooling and the engine template map described below. The tree, bark, leaves, ground, sky, trail and wind effects use code and shaders, with no textures in the world and no hand-made tree models. Ant leg motion is shader-driven. AI-generated content is disclosed as AI use; the rules recorded in `docs/design.md` do not classify it as sourced content.

## Sourced assets

[“Xelu's FREE Controller Prompts”](https://thoseawesomeguys.com/prompts/) by Nicolae (Xelu) Berbece, updated by Paul Paun (2021), CC0 1.0 (public domain). Resized light keyboard/mouse keys and Xbox Series buttons. They are used only for HUD prompts; every pad shows Xbox-style buttons. See `docs/sourced-assets.md`.

No marketplace content or third-party sound samples, world textures or models are listed in the current sourced-assets record.

## Earlier prototype

The following is word for word from `docs/devlog.md`, whose owner-review comment is still present:

> **Earlier prototype.** On 1–2 November I made a quick prototype of a tree-climbing ant game in a separate project (`TreeAntGame`). No files from it are used here: no code, assets, materials or models. This project was created on 12 November and all game content in it was made from that day on. What did carry over is knowledge: my idea notes (`concept-main.md`), and workflow and engine lessons from that prototype's notes, which the planning agent read (its `CLAUDE.md` and to-do list, not its source code) and used when writing the plan.

`docs/design.md` additionally records that the planning session read the prototype's commit list and file listing. No source was read or adapted. The CC0 prompt icons, added on 13 Nov, are the disclosed sourced exception to the dev log's “all game content” timing statement. [OWNER: confirm the disclosure before publishing.]

## Pre-jam tooling and engine template

Two helper scripts predate the jam, as recorded in the dev log: `Scripts/unreal-lock.sh` and `Scripts/RunGame.bat`, adapted from another project and containing no game content. The lock now allows two Unreal processes (14 Nov); the dev log's older “one process” wording is historical. `L_Main` began as a copy of the engine's default template map; the floor and props were removed, leaving sky and lighting.

## Owner's recording and submission checklist

[IF VIDEO KIT SHIPS] Recording only: after packaged verification, use `Leaf.Cinematic 0|1`, `Leaf.Shot <name> [seconds]` (orbit/crane/dolly/reveal/follow), `Leaf.ShotList <name> <name> ...` and `Leaf.Slomo <factor>` (1 = normal); confirm final syntax in `docs/tech.md`. These are capture aids, not claims about the shipped gameplay or a video already recorded.

- Confirm the final title, licence, optional features, archive name and actual zip size. The latest recorded package is 13 Nov 22:28; the Sunday package has not been made. Do not reuse the old 463 MB figure as the final size.
- Disable `bInputVizKey` and `bShowBuildLine` as M7 requires, then verify the final Development package from a fresh folder and a path with spaces, with no local save included.
- Record 30–60 s at 1920×1080 or higher, in English; fill the video URL. No trademarks or logos in game, video or page art except the Unreal Engine logo used per Epic's branding guidelines.
- Publish two itch pages. Freeze the MegaJam page and build at submission; date later Game Off updates separately.
- Owner confirms the prototype disclosure and AI/tool credits. Check the sourced-assets record again after Saturday work, especially any new bait key icon.
- Owner authorises repo publication, chooses a licence and checks links while signed out. Game Off needs a public source repo. If the repo remains private, an itch dev log can serve MegaJam, but it does not meet Game Off's public-repo requirement.
- Owner fills their full name and Epic email, submits personally by Sun 15 Nov 22:00 CET, and tags the submitted commit `megajam-submission`.
