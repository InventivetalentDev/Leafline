# Player-facing text pass (14 Nov)

The owner asked on 14 Nov at 17:10 for a pass over every string a player can see, so that none of it reads like dev text. The trigger was the patch read-out "Line 2 here: about 9 pieces, rich ×1.2" and the cut label "This piece: …".

This file lists every player-facing string, grouped by screen, with the old text, the new text and the reason. "Keep" means the string was checked and already reads well. Most strings are in `Source/AntTreeGame/UI/LeaflineHUD.cpp`; other files are named in their sections.

## One word per idea

| Idea | Word on screen | Replaces | Notes |
|---|---|---|---|
| What you cut and carry | **piece** | "fragment" (falling prompt, recap) | "Piece" was already used in most places. |
| The amount hauled or spent | **leaf** ("leaf hauled", "leaf to spend") | | Unchanged. |
| The ant you play (three a day) | **worker** | | Unchanged ("Worker 2 of 3 sets out"). |
| The automatic helpers (Brood) | **recruits** | "workers" in the idle row | The idle row counted recruits but called them workers. |
| Your scent trail | **trail** | | Unchanged. |
| A trail that recruits work (internal: *line*) | **trail**, numbered in its colour: "Trail 2" | "Line 2" | The player sees a coloured trail on the bark. The game never explains "line". The number and colour still tie a row to its trail. |
| The leaves around a cut (internal: *patch*) | **"these leaves"** (they are outlined on screen) | "patch", "Line 2 here" | The game never teaches "patch". |
| A leaf's value multiplier (internal: *richness*) | **"each worth more"**, **"each worth much more"**, or nothing | "rich ×1.2" | See [Leaf value](#leaf-value). |
| Riding a trail (F / RB) | **ride** | "follow" in two places | The goal text and the riding prompt already said "ride". |
| Everything hauled since the start | **hauled in all** | "Colony" | The ui agent's word. The owner read "Colony 1,234" as leaf in storage. |
| How windy your spot is | **sheltered**, **partly exposed**, **exposed** (green, amber, red) | | Unchanged words and colours. |

## Leaf value

Each leaf is worth 1× to 3× from its height and how far out it sits, times the tree's multiplier (×1.3 on tree 2, ×1.6 on tree 3). The old read-out printed the patch's average multiplier ("rich ×1.2"). The new one gives a word in three steps:

| Patch average, divided by the tree's multiplier | Text |
|---|---|
| below 1.25 | nothing |
| 1.25 to 1.6 | ", each worth more" |
| 1.6 and up | ", each worth much more" |

- **Why divide by the tree's multiplier.** The leaf colour does the same (paler is richer, `LeafSet.h`), so the word matches what the player sees. Every leaf on a later tree would otherwise read "worth more". The move card already says the new tree's leaves are worth more.
- **Why these steps.** From the formula in `LeafSet.cpp`, a leaf at 70 m on tree 1 is worth about 1.1–1.3×, at 120 m 1.3–1.7× and at 200 m 1.5–2.3×. The test runs logged all three steps on tree 1: the lowest-band patch at ×1.00 (no word), patches at random leaves (`Leaf.GoLeaf`) at ×1.30 and ×1.36 ("each worth more"), and a cut leaf at ×1.67 ("each worth much more").
- The trail rows no longer show value. By then the trail is laid, and the value doesn't help any decision. The rows say how many pieces are left.

## HUD during a shift

### Colony counter, top centre

| Where | Old | New | Why |
|---|---|---|---|
| `DrawColonyIcons` (icons on, default) | "125 hauled in all" | keep | The ui agent's wording; it answers the owner's "leaves in storage" confusion. |
| `DrawColonyIcons` | "+3.0 today" | keep | |
| `DrawColonyIcons` | "next milestone at 200" | keep | |
| `DrawColonyIcons` | "new tree at 400" | keep | |
| `DrawColonyCounter` (`Leaf.HudIcons 0`) | "Colony  1,234" | "1,234 hauled in all" | Same words as the icon version. |
| `DrawColonyCounter` | "Today  +3.4" | "+3.4 today" | Same words as the icon version. |

### Day panel, top left

| Where | Old | New | Why |
|---|---|---|---|
| `DrawDayInfo`, `DrawDayPanelIcons` | "Day 3: the old giant" | keep | |
| same | worker states "1 Ready", "Out", "Returning", "Home", "Blown off" | keep | |
| same | "Hauled 3.0" | **removed** | It is `Stats.MassHauled`: everything hauled today, by workers and recruits. That is the same number as "+3.0 today" in the colony counter beside it. Two labels for one number read as two different things. |
| `DrawCarry` (`Leaf.HudIcons 0`) | "Carrying 3.0" | "Carrying 3.0 ×1.6" (the multiplier smaller, pale yellow, only above ×1.0) | The owner's decision (14 Nov): weight as the large number, the leaf's value multiplier beside it, so "+4.8" on arrival is visibly 3.0 × 1.6. Beside rather than under the number, because the size bar sits right under it. |
| `DrawCarry` | "Carrying: empty" | "Carrying nothing" | Plain sentence instead of `label: value`. |
| `DrawCarryIcons` | "3.0" beside the leaf icon | "3.0 ×1.6" (as in the text version) | Same decision. |
| `DrawCarryIcons` | "empty", "gripping" | keep | Short words beside icons. |
| `DrawStamina` | "Stamina", "Stamina: recovering" | keep | |

### Wind panel, top right

The text version (`DrawDayInfo`, `Leaf.HudIcons 0`) and the icon version (`DrawWindIcons`) share the legend.

| Old | New | Why |
|---|---|---|
| "Wind: calm", "Wind: gust coming", "Wind: easing", "Wind: GUST" | keep | |
| icons: "calm", "gust coming", "easing", "GUST!" | keep | |
| "Here: sheltered" / "Here: partly exposed" / "Here: exposed" (text version) | "You're sheltered" / "You're partly exposed" / "You're exposed" | Names who is sheltered. The icon version keeps its single words. |
| "Wind map:" | "Wind map" | Heading, not a label. |
| "green sheltered" | "Green: sheltered" | The colon makes it read as a key. |
| "amber partly exposed" | "Amber: partly exposed" | |
| "red exposed" | "Red: exposed" | |
| "Hold {Cycle}: wind map" | keep | |
| "Bird diving!", "Bird circling"; icons: "diving!", "circling" | keep | |

### Cut arc, centre

| Old | New | Why |
|---|---|---|
| number in the arc, "3.0" | "3.0" with "×1.6" smaller underneath, in one panel (only above ×1.0) | The owner's decision (14 Nov): the large number stays the piece's weight, and the multiplier under it is the leaf's value, the one the haul uses (`AAntPawn::GetCarriedValue`). So "+4.8" on arrival is visibly 3.0 × 1.6. Both show one decimal, so the product can be 0.1 off the "+N". |
| "This piece: safe in a gust" | "Good size: safe in gusts" | The owner found the label switching from "safe" to a wind warning confusing (playtest 1), and still found "This piece: …" odd. As the arc grows the label now judges the **size**: good size, getting big, too big. The colours stay green, amber, red. |
| "This piece: risky in a gust" | "Getting big: risky in gusts" | |
| "This piece: too big for a gust" | "Too big: gusts blow it off exposed bark" | Says what happens, and where: the prediction assumes a full gust on exposed bark without gripping, so sheltered bark or gripping can keep the piece (night review, 14 Nov). |

### Leaf read-out, above the prompt (`DrawPatchPreview`)

| Old | New | Why |
|---|---|---|
| "Line 2 here: about 9 pieces, rich ×1.2" | "About 9 pieces on these leaves, each worth more" | No internal terms ("line", "here" for the patch, "rich ×"). "These leaves" points at the faint outlines drawn on the patch. The value word follows [Leaf value](#leaf-value). The text keeps the next trail's colour. It promises nothing about recruits, because they may be busy on other trails (and with recruits off there are none), and the read-out shows anyway. |
| (one leaf in the patch) | "About 3 pieces on this leaf" | Singular when the patch is a single leaf. |
| (one piece) | "1 piece left on these leaves" | No "about 1 pieces". |
| "Stripped. Fresh leaves are higher up." | "No leaf left here. Climb higher for fresh leaves." | "Stripped." alone read like a status code. Now it says what to do. |

### Trail rows, left (`DrawLineRows`)

| Old | New | Why |
|---|---|---|
| "Line 1" | "Trail 1" | See the glossary. |
| "Home 3   Lost 0" | keep | |
| "5 left   rich ×1.2" | "5 pieces left" ("1 piece left", "Used up") | Counted as the bar is: pieces still on the leaves, including those recruits are on their way to cut. The old number left those out, so a trail could read "0 left" beside a half-full bar. Value is dropped here (see [Leaf value](#leaf-value)). |
| "2 workers idle: lay a new line" | "2 recruits waiting for a new trail" ("1 recruit waiting…") | These are recruits, not workers. "Lay a new line" is jargon. |

### Prompts, bottom centre (`DrawPrompt`)

| Old | New | Why |
|---|---|---|
| "Blown off! The fragment is lost" | "Blown off! The piece is lost" | One word per idea. The next prompt says the worker walks home. |
| "Gripping. Hold on until the bird has passed" | keep | |
| "Bird diving! Hold {Grip} to grip      {Drop}: drop your piece" | keep | |
| "Walking home empty-handed.   {Cycle}: switch ant" | keep | |
| "Gripping. Release {Grip} when the gust passes" | keep | |
| "Gust! You are sheltered here" (also shown while the gust is still coming) | "Gust coming: you're sheltered here" before the gust, "Gust! You're sheltered here" during it | It said "Gust!" before the gust arrived (seen in the 720p shots). The contraction matches the wind panel. |
| "Gust coming: hold {Grip} to grip, or get to the side away from the wind arrow" | "Gust coming: hold {Grip} to grip, or move to the sheltered side" | Shorter. Uses the wind panel's word instead of describing the arrow. |
| "Gust! Hold {Grip} to grip" | keep | |
| "Heading home on the trail.   {Cycle}: switch ant" | keep | |
| "Release to finish the cut" | keep | |
| "{Send}: send home      {Drop}: drop your piece for the bird" | keep | |
| "{Send}: send home      or carry it to the nest yourself" | "{Send}: send it home, or carry it to the nest yourself" | The old gap made "or carry it…" look like a second, keyless prompt. |
| "Hold {Cut}: cut" | "Hold {Cut}: cut a piece" | Says what you get. |
| "{Send}: end this worker's trip" | keep | |
| "Riding the trail.   Move to step off" | keep | |
| "Hold {Follow}: follow the trail" | "Hold {Follow}: ride the trail" | One verb: the goal text and the riding prompt say "ride". |
| "Everyone is home" | keep | |

### Goals and hints, top centre

| Old | New | Why |
|---|---|---|
| "Climb the trunk: the leaves start about 26 m up" | keep | |
| "Your first worker walks home on its own. Hold {Follow} on the cyan trail to ride it up" | keep | Trail 1 is cyan, so this stays true. |
| (new, 15 Nov) day 1, second worker, while recruits are out on the first trail | "Your first worker walks home on its own, and recruits follow its trail to fetch pieces. Hold {Follow} on the cyan trail to ride it up" | A new colony has two recruits from day 1. This is where the player first sees them, so it says what they do; without recruits (Leaf.Recruits 0) the line above shows instead. |
| "A bird hunts on the wind. Stay off the red, grip when it dives, or drop your piece in the open to draw it away." | "A bird is out today. It swoops at ants carrying pieces on exposed bark: stay off the red, grip when it dives, or drop your piece in the open to draw it away." | "Hunts" suggests the bird eats ants (teen rating, nothing that reads as cruelty). Now says who it goes for and why red matters. |
| (bait off) "A bird hunts on the wind. Stay off the red, or grip when it dives." | "A bird is out today. It swoops at ants carrying pieces on exposed bark: stay off the red, or grip when it dives." | Same. |

### Shift banner, centre (`DrawShiftBanner`)

| Old | New | Why |
|---|---|---|
| "Day 3: the old giant" | keep | |
| "Worker 2 of 3 sets out" | keep | |
| "Worker 2 heads home with its piece" | keep | |

## Harvest and fast-forward (`DrawHarvest`)

| Old | New | Why |
|---|---|---|
| "Harvest: the colony brings in the rest" | "Harvest: your recruits bring in the rest" | Names who is doing it. |
| "Fast-forward: the last workers head home" | keep | |
| "Skipping...", "Speed ×5   Recruits out: 3   Trips left: 7" | keep | Plain enough, and the numbers show what is left. |
| "{Grip}: skip      {Cycle}: next line" | "{Grip}: skip      {Cycle}: next trail" | Glossary. |

## End-of-day recap (`DrawRecap`)

| Old | New | Why |
|---|---|---|
| "Day 3: today's cut from the old giant" | keep | The "Director's Cut" theme title. |
| "Tree 1: day 3 on this tree" | keep | |
| row: "Line 1" | "Trail 1" | Glossary. |
| row: "Home 3", "Lost 0", "Hauled 6.2" | keep | |
| row: "Patch left 5" | "Left 5" | "Patch" is never taught. It sits beside "Home" and "Lost". Counted as in the trail rows. |
| "and 2 more" | "and 2 more trails" | Says more of what. |
| "Leaf hauled: 12.3", "Best day yet!", "Best day so far: 15.0" | keep | |
| "Fragments brought home: 12" | "Pieces brought home: 12" | Glossary. This counts every piece, recruits' included. |
| "Recruits: 9 pieces home, 2 lost to the wind" | keep | |
| "Largest fragment: 3.1" | "Largest piece: 3.1" | Glossary. |
| "Workers blown off: 1", "Trail laid: 140 m", "Pieces lost to the bird: 2" | keep | |
| "Tomorrow the colony moves to a new tree" | keep | Also on the nest screen. |
| "Press E or click to go into the nest" | keep | |

### Colony meter, recap and nest (`DrawColonyMeter`)

| Old | New | Why |
|---|---|---|
| "Colony: 125.4 hauled in all" | "Colony growth: 125.4 hauled in all" | Says what the bar is (the growth meter with the milestones), using the HUD's "hauled in all". |
| milestone numbers, "new tree" | keep | |

## Title (`DrawTitle`)

| Old | New | Why |
|---|---|---|
| "Leafline", "You are a leafcutter ant on an enormous tree.", "Cut a piece of leaf, send the worker home along your scent trail, and take over the next one." | keep | |
| "{Cut} cut (hold)      {Send} send home      {Follow} follow a trail (hold)" | "{Cut} cut (hold)      {Send} send home      {Follow} ride a trail (hold)" | One verb. |
| other control lines | keep | |
| "Press E or click to continue: day 3" | "Press E or click to start day 3" | The colon read like a log line. A day quit midway restarts at dawn, so "start" is true. |
| "Press E or click to begin", "{Back}: credits" | keep | |

## Nest (`DrawNest`, `Game/Upgrades.cpp`)

| Old | New | Why |
|---|---|---|
| "The nest", "Leaf to spend: 40.0" | keep | |
| Mandibles: "Cut faster." | "Cut faster (recruits too)." | It affects recruits too (design.md). |
| Strength: "Reduce carry slowdown." | "Big pieces slow you down less." | Plain words instead of a stat name. |
| Grip: "Withstand stronger gusts." | "Hold on in stronger gusts (recruits too)." | Plainer. Affects recruits too. |
| Stamina: "Reduce sprint stamina drain." | "Sprint for longer." | |
| Brood: "More recruits to work your lines." | "More recruits to fetch pieces along your trails." | No "lines". A new colony already has two recruits (they are first met on day 1's first trail, see the day-1 goal); this line says what more of them do. |
| upgrade names, "Cost 40", "Max", "Start day 4", "Start day 4 on a new tree" | keep | |
| "{Choose}: choose      {Confirm}{orclick}: buy or start      {Back}: start the next day" | keep | |

## Move card (`DrawMoveCard`, `World/DayDirector.cpp`)

| Old | New | Why |
|---|---|---|
| "The colony moves to a tall spire" | keep | |
| tree names: "the old giant", "a spreading giant", "a tall spire", "a leaning elder" | keep | |
| "A new tree: taller, with richer leaves and stronger gusts." | "A new tree: taller and windier, and its leaves are worth more." | No "rich", the same idea as the leaf read-out. |
| "Press E or click to begin day 6" | "Press E or click to start day 6" | Same verb as the nest's "Start day 6". |

## End card (`DrawEndCard`)

All kept: "The colony thrives", "Enough leaf has come home to feed the fungus garden for a season.", "Thank you for playing Leafline. The trees keep growing, and so can the colony.", "Press E or click to keep playing".

## Pause, options, controls, credits

| Where | Old | New | Why |
|---|---|---|---|
| Pause | "Paused", "Resume", "Controls", "Options", "Credits", "Quit", the two prompt lines | keep | |
| Options | "Mouse sensitivity", "Invert look", "Volume", "Graphics", "Back", the prompt lines | keep | |
| Options values (`LeaflinePlayerController.cpp`) | "0.50", "On"/"Off", "80%", "Low"/"Medium"/"High"/"Epic" | keep | "Epic" is the engine's name for the top level. "Ultra" is an option if the owner prefers. |
| Controls | "follow a trail (hold)" | "ride a trail (hold)" | One verb (the title screen has the same list). |
| Controls | "grip in a gust or a dive (hold)" | "grip in a gust or a bird dive (hold)" | "A dive" alone is unclear. |
| Controls | "Showing the pad. A key or the mouse shows the keyboard." | "Showing the controller. Press a key to show the keyboard." | "Controller" is the usual word. Instruction first. |
| Controls | "Showing keyboard and mouse. A pad button shows the pad." | "Showing keyboard and mouse. Press a controller button to show the controller." | |
| Controls | other rows | keep | |
| Credits page | "Credits", "Back", "{Choose}: scroll      {Back}: back" | keep | |

## For the owner (not changed here)

- **Credits placeholders.** Done 15 Nov: the UI art credit is gone (no UI art in the MegaJam build), and the Friend A lines in `Content/Leafline/UIRaw/credits.txt` are commented out until their feedback comes in.
- **Build line.** The faint bottom-left line ("dev build | the old giant, seed …, day 3 | Leaf.Goto …") is debug text and is on by default. `docs/plan.md` already says to set `bShowBuildLine=False` in `DefaultGame.ini` before the submission build.
- **design.md** still quotes the old read-out ("about 8 pieces, rich ×2.1", line 121).
- **Prompt fallbacks.** `UI/PromptIcons.cpp` has text fallbacks ("E / LMB", "left stick") that show only if a button image is missing. Left as they are.

## Checked: nothing a player sees

These files have no player-facing text, only log lines, debug commands, asset paths and sound names: `World/BirdSystem.cpp`, `Game/LeaflinePlayerController.cpp` (apart from the option values above), `Colony/LineSystem.cpp`, `Colony/NestActor.cpp`, `Ant/AntPawn.cpp`, `Tree/TreeActor.cpp`, `World/WindSystem.cpp`, `UI/Skin.cpp`, `Game/LeaflineShots.cpp` and `Debug/*`.
