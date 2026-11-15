# Leafline: game design

Working title **Leafline**. Unreal project name stays `AntTreeGame`. Team name `inventivetalent` (solo).

This file records every design decision from the planning session on 2026-11-12. The original idea notes are in `concept-main.md` and are kept unchanged. How it is built is in `tech.md`; the order of work is in `plan.md`.

## One-paragraph pitch

You are a leafcutter ant on an enormous tree. You climb, lay a scent trail, and cut a piece out of a leaf: a bigger piece is worth more but is slower to carry and catches the wind like a sail. When you send the loaded ant home it retraces your trail by itself and you shift to the next worker at the nest, who can ride the trails already laid. Each day is a handful of worker shifts on one tree. Leaves feed the colony and buy upgrades. Tomorrow the tree is new.

## Jam constraints

| | Epic AI MegaJam | Game Off 2026 |
|---|---|---|
| Deadline (CET) | **Mon 2026-11-16 02:59** | Tue 2026-12-01 22:37 |
| Our internal deadline | **Sun 2026-11-15 23:00** at the latest (was 22:00) | to be set after the MegaJam |
| Theme | Director's Cut | SHIFT |
| Deliverable | Packaged Windows development build, zip `inventivetalent_Leafline.zip` under 1 GB; 30–60 s gameplay video at 1920x1080 or higher; English text; link to dev log (optional) | itch page plus **public GitHub repo** with the source |
| Judging | Panel, 1–5 each: ambition and innovation, theme, fun, gameplay, visuals, level design and progression, audio | Participant votes: overall, gameplay, graphics, audio, innovation, theme interpretation |

Rules that shape the work (MegaJam Official Rules, read in full on 2026-11-12):

- **§7(h), clean room.** AI tools, agent and MCP configuration, prompt libraries and tooling scripts may predate the jam. "All other game content, including gameplay, levels, and art assets (other than noted sourced assets), must be created during the Contest Period", which began 2026-11-12 06:00 CET. Epic may ask for version-control history. An earlier prototype exists at `D:\UnrealProjects\TreeAntGame` (1–2 Nov). **Nothing is copied from it:** no code, materials, meshes, Blender files or levels. Do not open its source to "check how it was done". It is disclosed in the dev log and the submission. For the record: the planning session read that project's `CLAUDE.md`, `TODO.md`, commit list and file listing (not its source), and some workflow and engine lessons in `tech.md` and `CLAUDE.md` come from those notes.
- **§7(g).** The entry must demonstrate the theme. A nod in the title is not enough.
- **§7(e).** Gameplay must go beyond Epic's starter templates. The Third Person template content is deleted.
- **§7(f).** No extra software needed to run.
- **§8.** Teen-appropriate, and no depiction of animal cruelty at Epic's sole discretion. This is why the rocket launchers from the concept notes are dropped.
- **§15(c), sourced assets** (anything not made by us, such as CC0 sounds) are allowed with the right licence and must be listed in the submission. Content we generate with AI tools does not count as sourced. Nothing Fab-licensed may enter the repo, because the repo goes public.
- **§15(d).** No trademarks or logos in the game, the video or the page art, other than the Unreal Engine logo used per Epic's branding guidelines.
- **§17.** The entry must be submitted by the entrant in person, "not by an AI agent or other automated means". Agents prepare everything; the owner presses submit.
- **§7(c), §7(i).** The video is 1920x1080 or higher, and all game text, audio and the video are in English.
- No exclusivity or "not previously published" clause anywhere in the rules: the same game may go to both jams. Ownership stays with the author; Epic gets a non-exclusive licence for publicising the contest.
- Winners are contacted around 29 Jan 2027 at the submission email and have 72 hours to reply.

The MegaJam theme as announced by Unreal Engine on X (full post, supplied by the owner):

> This year's MegaJam Theme is... DIRECTOR'S CUT!
>
> Your vision, your specific edits and ultimately: your final cut - your Director's Cut.
>
> Every developer has that one feature that didn't quite make it, the mechanic scoped out while sleep deprived at 2 AM or that idea you've had shelved and never quite found a place for it yet. Well, now's your chance to finally build it all in this year's Epic MegaJam.
>
> If you're utilizing AI agents in your workflows, you're directing those too! Set the vision, delegate your tasks and decide what goes in and what doesn't. Aim for ambition and direct your path accordingly!

## Strategy

- One game, two milestones. The MegaJam build is a vertical slice. The same build goes to Game Off on Sunday, and the Game Off page keeps receiving updates until 1 Dec.
- **Two itch pages**: one frozen for the MegaJam, one for Game Off.
- The repo is **private until the owner says otherwise**. Game Off needs it public, so it is switched once the owner confirms.
- The owner is not chasing specific prizes. A short dev log is kept anyway.

## Pillar

**A tiny ant on an enormous tree that feels great to move on.** Surface-walking in any orientation, a floaty camera that never spins the view, and a real sense of scale. Any other feature is cut before this is compromised.

## How the themes are used

**Director's Cut (MegaJam)**
- *The cut*: you choose how much to cut from a leaf. Size is the central trade-off of every trip.
- *The director*: your scent trail is the direction you give the colony. Recruits follow it without you and strip the leaves around your cut. Where you lay it (sheltered side or exposed side) and how big you cut decide how many of them get home with their piece.
- The end-of-day recap is titled as the day's cut.
- This maps onto the theme post's last paragraph almost word for word: "set the vision" is the route you lay, "delegate your tasks" is sending workers along it unattended, "decide what goes in and what doesn't" is the size of each cut, and "direct your path" is the trail itself. Use that framing on the itch page and in the video.
- The post's "shelved idea" reading also applies honestly: this game is an idea from the owner's notes that had not found a home. Say so on the page, without overstating it.

**SHIFT (Game Off)**
- You shift between ants: sending one home hands you the next.
- Each worker trip is a shift, and a day is a set of shifts.
- The ant shifts between surfaces and orientations constantly.

## Core loop

1. **Leave the nest** at the base of the trunk. The ant lays a scent trail wherever it walks.
2. **Climb** to a leafy branch. Sprint is available while stamina lasts.
3. **Cut**: at a leaf, hold the cut button. A growing arc on the leaf shows the fragment size. Release to finish. The fragment is carried physically in the mandibles, held overhead.
4. **Send home**: the loaded ant retraces its own recorded path in reverse, unattended. Control shifts to a fresh worker at the nest. Walking home yourself is also allowed.
5. **The line**: a piece on its way home turns the scout's route into a *line*. Recruits from the colony's pool walk it out, cut the same size of piece from the leaves around the scout's cut (the *patch*) and carry them home, while you scout on. Patches run dry, so idle recruits wait for you to lay a new line.
6. **Next shift**: the new worker can hold a key to ride an existing trail at boosted speed, then branch off toward other leaves.
7. **Harvest**: when the last scout is home or on its way, the recruits finish their trips at five times speed (skippable). The camera watches the file of ants come down the trunk to the nest.
8. **Day ends** when every ant is home. A recap counts up line by line: pieces home, pieces lost to the wind, what is left on each patch, and whether it was the best day yet.
9. **Nest**: spend leaf mass on upgrades.
10. **New day**: the colony stays on its tree. The leaves it cut stay cut and never grow back, so the easy low leaves run out and the colony has to climb into more wind. Yesterday's trails stay for one more day, faint; the lines are gone. At the second and third milestones the colony moves to a new tree.

A day should take 4–6 minutes: three trips of roughly 90 seconds.

## Rules in detail

**Workers and days**
- Three scouts a day at every level, each making one trip. Recruits (the Brood upgrade) are the automation: one shared pool, a fixed number of trips each per day, never controlled.
- There is no real-time clock. The sun steps from morning to evening with each shift, as a visual cue only.
- Within a day the tree and all trails persist, so later workers benefit from earlier routes.

**Stamina**
- Stamina only gates sprinting. It drains while sprinting, drains faster under a large fragment, and refills while walking or standing.
- Eating to recharge is parked.

**Cutting and carrying**
- One fragment at a time, attached to the ant, never an inventory.
- Fragment size grows with hold time, up to a cap set by the leaf's remaining area. Leaves show bite marks and run out after about three cuts.
- A bigger fragment is worth more leaf mass, slows the ant, drains sprint faster and presents more area to the wind.
- While cutting, the arc's colour shows the predicted wind risk for that size (safe, risky, will be blown off in a full gust on an exposed route).
- For Sunday the ant cuts from the twig at the leaf's base. Walking on leaf blades is parked.

**Scent trails**
- Every worker records its path from the nest. The visible trail is the union of all paths walked today.
- Riding: hold the follow key near a trail to be carried along it at boosted speed toward its far end. Any movement input leaves the trail.
- A returning ant replays its own path backwards. No pathfinding is involved.

**Wind (the Sunday hazard)**
- Wind has one prevailing direction per day, so "the sheltered side" is consistent and can be planned around.
- Gusts cycle: calm, warning (sound rises, streaks appear, leaves sway), gust, fade.
- Exposure at a point depends on which way the surface faces relative to the wind and on whether the tree blocks the wind there.
- An ant is blown off when gust force times exposure times its sail area exceeds its grip. This is deterministic, so the player can learn it.
- The controlled ant can stop and **grip** to ride a gust out. Unattended ants never grip; their safety depends on the cut size and the route chosen for them.

**Lines, patches and recruits** (spec: `docs/briefs/the-line.md`)
- A line starts when a scout's piece is on its way home (sent home with it, or carried into the nest). A scout blown off or returning empty leaves a trail but no line.
- The patch is the cuttable leaves nearest the scout's leaf along the bark: the same twig first, then neighbouring twigs of the same bough, up to four leaves and ten pieces. The scout's own piece counts as the first.
- Recruits leave the nest a second and a half apart, so several on one line read as a file. Each picks the live line with the most remaining pieces per recruit already on it. A trip is out, one piece of the line's size, home.
- Recruits face the same wind test as scouts and never grip. A blown-off recruit loses its piece and that trip; one that finds its patch stripped turns round without using a trip.
- The trail's brightness shows its patch: bright is full, faint is nearly stripped or no line at all.
- Before committing, the patch around the leaf in reach is outlined, with what it is worth ("About 8 pieces on these leaves, each worth more").

**Richer leaves**
- Each leaf has a value multiplier from 1× at the lowest leafy branches to about 3× at high outer tips, from the tree's shape alone. The height part builds up over the first 40 m above the lowest leaves (`RichnessHeightScale` 40, the owner's choice of 15 Nov), so climbing a layer or two pays; before that it rose evenly over the whole crown. Later trees multiply it (×1.3 on tree 2, ×1.6 on tree 3). Rich leaves are paler, younger green. A piece's value is its size-based mass times the multiplier; the carried weight that slows the ant is the size-based mass only.

**Losing an ant**
- A blown-off ant falls, loses its fragment and walks home empty-handed. Nobody dies. The trip is wasted, which matters when there are only a few per day.

**Upgrades** (five, six levels each with steeply rising costs, bought with leaf mass on a list screen at the nest between days, saved between sessions)

| Upgrade | Effect |
|---|---|
| Mandibles | Cut faster (scouts and recruits) |
| Strength | Carry bigger fragments without slowing |
| Grip | Withstand stronger gusts (scouts and recruits) |
| Stamina | Sprint longer |
| Brood | Recruit pool: 2 / 3 / 4 / 6 / 8 / 10 / 12 (a new colony has two recruits from day 1, so the first trail already brings them out; owner's decision, 15 Nov) |

**The off switch.** One flag (`FDayTuning::bRecruits`, `Leaf.Recruits 0|1`) turns the line's automation off: no recruits, no harvest, no Brood row, and a smaller "by hand" milestone set. Richer leaves, the patch read-out, the longer ladders and the number-go-up interface stay either way.

**One tree per colony stage.** Agreed with the owner on 13 Nov (`docs/briefs/one-tree.md`); it replaced "new day, new tree".
- The colony lives on one tree, grown once and kept from day to day. Days still have three scouts, a recap and the nest screen.
- Cut leaves stay cut, across days and across quitting and relaunching. Leaves never grow back; the only reset is a move. A leaf with little area left loses its blade and keeps its stalk, so a stripped bough looks bare; one that hit the three-bite limit with area to spare keeps its bitten blade and turns brown.
- The tree is sparse low down and dense high up: the lowest boughs hold only a few patches, enough for two or three days.
- The day's bites are saved with the day's haul, at the end of the day. Quitting in the middle of a day restarts it from dawn: no bites, no haul.
- Yesterday's trails stay for one more day: faint, ridable, with no recruits on them. They are gone after the second night and on a move.
- The wind changes direction every day, so the sheltered side moves. Within a stage only the wind gets harder, a little each day; the big steps are the moves.
- The colony moves at the second and third milestones: the next day starts on a new tree, a different shape, taller, with richer leaves and stronger gusts, and a card says so ("The colony moves to a tall spire"). The first milestone does not move it (with the end card on about day 6, a move at each milestone would leave too few days per tree). At most one move a day. After the third milestone the end card shows and play continues on the third tree. The shapes come in a fixed order: the old giant, a spreading giant, a tall spire, a leaning elder.
- A save from before this change keeps its day count and upgrades and starts on a fresh tree for the stage its milestones put it in.
- The off switch: `FDayTuning::bOneTree`, `Leaf.OneTree 0|1`. Off, a new tree is grown every day with a rising leafy height, and the old milestone sets apply. It is independent of the recruits switch.

**Goal and progression**
- A colony-growth meter fills with total leaf mass hauled and has three milestones.
- Reaching the third milestone (target: 15 to 20 minutes into a fresh save, with the line on or off) shows an end card, then play continues. With the line on, day hauls should grow about tenfold from day 1 to the end card.
- With one tree per stage, the climb comes from the stripped low boughs and the moves; the gusts strengthen a little each day on a tree and more with each move. With it off, each new day raises the lowest leafy branch and strengthens the gusts.

Starting numbers, to be tuned by play:

| Item | Value |
|---|---|
| Fragment mass | 1 to 5 |
| Upgrade cost, levels 1 to 6 | see `Game/Upgrades.cpp` and `docs/economy.md` |
| Milestones (total hauled) | with the line and by hand: see `docs/economy.md` |
| Recruit trips per day | 3 each |

## Scale and pacing

- The tree is enormous relative to the ant, and that must be felt. Circling the trunk takes about 20–25 seconds at a walk and about half that at a sprint.
- The first cut of day one must be reachable within about 90 seconds, so the lowest leafy branch is low on the first tree. The tree is far taller than early days need, which leaves room for the climb as the low leaves run out.
- The owner's photos in `Art/Reference/Photos` are scale reference: the view up a trunk from close to the bark. An ant's view is tighter still.

## Camera and controls

- Movement is relative to the camera, projected onto the surface the ant is on.
- The mouse (or right stick) orbits freely.
- The camera has its own "up" that drifts slowly toward the ant's, so a sharp turn onto a branch does not swing the view. The ant turns; the camera barely moves.
- This is a first version. The owner tunes it by feel.

| Action | Keyboard and mouse | Gamepad |
|---|---|---|
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Sprint | Left Shift | Left trigger |
| Cut (hold) | Left mouse or E | X |
| Grip (hold) | Space | A |
| Send home | Q | Y |
| Follow trail (hold) | F | Right bumper |
| Drop the piece (bird bait, while the bird is out) | G | B |
| Cycle ants (tap) | Tab | Left bumper |
| Input view (debug; stays in the submission build, owner, 15 Nov) | F3 | |
| Wind map (hold) | Tab | Left bumper |
| Pause | Esc or P | Start |

The stick is a direction on screen everywhere (13 Nov): up/away and right, read from the camera when you start pushing and kept while you hold it, so the camera is free meanwhile. A snap to along or around on thin twigs exists but is off by default (`bSnapOnBranches`; the owner found that diagonals did not work with it). See `tech.md`, Movement.

## Look and sound

- **Target**: `Art/Concept/Codex/2026-11-12/initial/01-tree-ascent.png`. Summer canopy, warm plated bark with deep fissures and moss, dappled sunlight and light shafts, backlit green leaves, a restrained translucent cyan scent trail lying on the bark. Images 02–04 show carrying, the nest and the wind.
- **Math only**: tree, bark, leaves, ground, sky, trail and wind effects are procedural geometry and shaders. No textures and no hand-made tree models.
- **Exceptions**: the ant is a mesh generated by a Blender Python script, with leg motion driven by the shader. The nest mound is one too (`Tools/blender/make_nest.py`, the owner's OK on 13 Nov): it follows the walking field's mound exactly, with dug-soil pellets, an entrance shaded to black and a few leaf scraps.
- **Exception (HUD only)**: button prompt icons from Xelu's CC0 prompt pack (light keyboard keys, Xbox-style buttons for every pad), the owner's choice on 13 Nov; listed in `docs/sourced-assets.md`.
- **Audio** is synthesised by script: wind, footsteps, leaf-cut crunch, deposit chime, a soft ambient pad, the bird. No recorded samples are used; any CC0 sample added later must be listed as a sourced asset. Background music exists behind a switch (`FWindTuning::bMusic`, off by default; three options from `Tools/audio/make_music.py`).
- **HUD**: stamina, carried load, workers left, the colony meter, wind and exposure, the bird, and contextual key prompts in place of a tutorial. All text in English.
- **Nest**: an entrance among the roots plus the upgrade screen. No interior.

## Out of the MegaJam build

| Item | Status |
|---|---|
| Bird predator, distracted by a dropped piece | **In the MegaJam build**: the bird and its bait ship, on by default (owner, 15 Nov); with recruits on it first comes on day 2, with one gentle swoop and a hint |
| Taking over an ant that is already returning | Stretch |
| Walking on leaf blades | Stretch |
| Skill tree and a visible, growing fungus garden | Game Off |
| Several trees in the world at once | Game Off |
| Choosing which tree to move to, or going back to an old one | Not planned |
| Leaves growing back, seasons, weather other than wind | Not planned |
| Controlling several ants at once, defenders | Game Off, undecided whether it fits (automation arrived with the line) |
| Orders to recruits ("hold here in gusts") | Dropped: too fiddly |
| Trails that slow ants down | Not planned |
| Drop cutting, relay caches, castes | Game Off ideas |
| Other predators (for example the parasitic flies that real leafcutters guard against) | Game Off |
| Food as a second resource | Game Off idea (the bird's bait is the carried leaf piece, so the bird did not need one) |
| Eating to restore stamina | Parked |
| Rocket launchers | Dropped |

## Open items for the owner

- Join both jams on itch: <https://itch.io/jam/2026-epic-ai-megajam> and <https://itch.io/jam/game-off-2026>. The MegaJam entry needs your full name and the email on your Epic account.
- Upload the 30–60 s gameplay video. It was filmed and cut with the video tools (`docs/ship/video.md`; the final file is `Build/Video/leafline_megajam.mp4` in the main checkout).
- Choose a licence for the public repo before Sunday.
- Confirm the final title (alternatives considered: "Cutting Shift", "Canopy Crew").
