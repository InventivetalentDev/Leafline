# Brief: the line (recruits, patches, richer leaves, number-go-up)

For the main session. Agreed with the owner on Thu 12 Nov, evening. This adds the game's automation arc: you scout and cut, the colony follows your trail and strips the leaves around your cut, and then needs you for a new path.

This brief is the specification. When the work lands, update `design.md` (core loop, theme mapping, upgrades, the out-of-scope table), `tech.md`, `plan.md` (a milestone "M5b: The line", before M6) and `devlog.md` yourself.

## Before you start

- Tag the current commit `pre-line`.
- Keep this work in its own commits, with messages starting `Line:`.
- **Order in the plan:** this comes before the bird. The bird (M6) only starts once the owner has kept the line.
- **Owner's verdict: Friday evening.** The owner plays it and either keeps it or has it switched off (see "The off switch").

## Terms

| Term | Meaning |
|---|---|
| Scout | A worker the player controls. Three a day, as now. |
| Recruit | A worker the colony sends by itself. Never controlled. |
| Line | A scout's loop-free route from the nest to the leaf it cut, plus the patch at its end. |
| Patch | The cuttable leaves around the scout's cut. A line is *live* while its patch has pieces left. |
| Template | The fragment size the scout cut on that line. Recruits on the line cut that size. |

## Rules agreed with the owner

1. **A line starts when a scout's fragment is on its way home.** That is when the scout is sent home with a fragment, or carries it to the nest itself. A scout that is blown off or returns empty leaves a trail that can be ridden, but no line.
2. **Recruits are one shared pool.** Pool size comes from the Brood upgrade; at Brood level 0 there are none. Recruits spread over the live lines. When a patch is stripped, its recruits move to another live line. If there is none, they wait at the nest.
3. **Recruits work while the player scouts.** They leave the nest spaced a second or two apart, so several on one line read as a file of ants.
4. **Each recruit has a fixed number of trips per day.** A trip is: walk the line out, cut one piece of the template size from a patch leaf, walk home, deposit.
5. **Recruits take the same wind risk as scouts.** The same force-against-hold test, with the Grip upgrade, at wherever they are in a gust. The route and the template size decide the losses. The bird, later, will threaten them too.
6. **Leaves run out.** A patch is finite. The existing bite limits per leaf stay.
7. **Idle recruits are shown.** The HUD says, for example, "4 workers idle: lay a new line".
8. **The patch is shown before the player commits.** With a cuttable leaf in reach, the patch around it is outlined and the HUD says what it is worth, for example "This patch: about 8 pieces, rich ×2.1".
9. **A trail's brightness shows its patch.** Bright is a full patch, faint is nearly stripped. See "Trail fading" for the details.
10. **Richer leaves are higher up and further out.** Each leaf has a value multiplier from 1× at the lowest leafy branches to about 3× at high outer tips. It depends only on the tree's shape, not on the day's wind. It is visible from a distance as colour: pale young green is rich. A piece's mass is its size-based mass times its leaf's multiplier.
11. **The day ends when every ant is home.** No clock.
12. **Harvest phase.** When the last scout is home, or sent home, and recruits still have work, the rest of the day plays fast. Gusts still apply. The player can skip it.
13. **Brood is now the recruit pool.** Scouts stay at three a day at every Brood level.
14. **Longer upgrade ladders.** About six levels per upgrade with steeply rising costs, and colony milestones rescaled so the end card arrives 15 to 20 minutes into a fresh save.
15. **Number-go-up interface**, four parts:
    - A floating "+N" at the nest for every delivery. The deposit chime rises in pitch as deliveries chain close together and falls back after a pause.
    - A colony counter at the top of the screen that ticks up smoothly.
    - One HUD row per line: pieces home, pieces lost, patch remaining.
    - A recap that counts up line by line and marks a best day.

## Decided without asking the owner

The owner can veto any of these; they are listed in the hand-over message too.

- Recruits never use the grip action. Only their route and load protect them.
- A blown-off recruit loses that trip, walks home empty, and goes again if it has trips left.
- A recruit that arrives at a stripped patch turns round empty; that does not use up a trip.
- A recruit chooses the live line with the most remaining pieces per ant already assigned to it, with richer patches preferred on a tie.
- The scout's own cut counts as the patch's first piece.
- The harvest view is at the nest, not pulled back to the whole tree. See below.

## Two places where the agreed wording had to change

**The harvest view.** The owner agreed to "the camera pulls back to show the tree". From that distance a 20 cm ant and a 7 cm ribbon are smaller than a pixel. Build this instead: the camera sits near the nest and looks up the trunk, where every line converges, so the file of ants with leaf pieces walks toward the camera and the "+N" pops land in view. Tab hops to the patch end of each live line and back. If time allows, give carried fragments a small minimum size on screen so a line reads from one branch away during normal play too.

**"Brood level 0 plays exactly as today."** That stops being exact once ladders are longer, milestones are rescaled and leaves vary in value. The agreed meaning is now the off switch below.

## The off switch

One tuning flag, for example `FDayTuning::bRecruits`, plus a console command `Leaf.Recruits 0|1`.

With it off:
- the pool is zero and nothing recruits;
- the Brood row is hidden on the nest screen;
- there is no harvest phase;
- colony milestones use a smaller "by hand" set that three scouts can reach in 15 to 20 minutes.

These stay on either way, because they are good without recruits: richer leaves, the patch read-out, longer ladders for the other four upgrades, and the number-go-up interface.

If the owner cuts the line on Friday evening, the flag ships off. Nothing is reverted.

## Technical notes

**Reachability is the main risk.** Recruits can replay a recorded route, but the patch's other leaves are not on it.
- Anchor the patch to the leaf the scout cut, not to wherever the path stops.
- Define patch membership so its leaves can be walked to: the leaves on the same twig as the cut, then on neighbouring twigs of the same bough, nearest first, up to a tuned limit. Do not use a bare radius; it picks up leaves on branches that are close in the air and far along the bark.
- From the end of the line, a recruit steers along the surface toward the base of its target leaf.
- Allowed fallback: if a recruit has not reached its leaf within a few seconds, it cuts from where it stands and the bite still goes on the target leaf. Log every fallback, and report how often it happens.

**The line's route.** `BuildReturnRoute` already produces the scout's loop-free way home. The line is that route reversed, ending at the cut.

**Trail fading.** The patch-based brightness replaces the age-based fade and thinning from `b7d75a5` and `63375eb`. Paths merge, so a stretch of ribbon can belong to several lines: draw it at the brightness of the liveliest line that uses it. A trail with no line stays faint and can still be ridden.

**Harvest speed.** Use a director-owned speed factor on ant movement and the wind clock, with substeps so no ant moves further per step than at normal speed. Do not use global time dilation: the camera, interface and audio stay in real time. Skipping the harvest is the same simulation at a much higher factor, so the result does not depend on whether the player watched.

**Day end.** The new invariant: the day ends when no scout is waiting or out, and every recruit is home and either has no trips left or has no live line to go to. Every recruit state must lead to "home" without player input, including: its line dies while it is walking out, it is blown off, the pool is zero, the harvest starts with nothing to do, the player skips. Have Codex review this state machine before the owner plays; its last review found two ways a day could fail to end.

**Save and upgrade table.** `LeaflineUpgrades::MaxLevel` and `FLeaflineUpgradeInfo::Effects` grow, and Brood's description and effect change meaning. Raise the save version and clamp old saves.

**Recap data.** Per line: pieces home, pieces lost to wind, mass home, patch remaining. Per day: best day so far, kept in the save.

**Debug commands** (add to the table in `tech.md`): `Leaf.Recruits 0|1`, `Leaf.Patch` (logs the patch around the leaf in reach: leaves, pieces, richness), `Leaf.Pool <n>` (force the pool size), `Leaf.Harvest` (jump to the harvest phase).

## Tuning: targets, not numbers

Leaf density changed in the look pass, so measure with `Leaf.Patch` before choosing numbers. Starting points only: pool by Brood level 0 / 2 / 3 / 4 / 6 / 8 / 10, three trips per recruit, a day-1 patch of 6 to 10 pieces.

The tension to tune around: with a fixed trip budget, extra lines are worthless if one patch can absorb the whole pool. Patches have to be small enough that they run dry.

Acceptance tests, each shown in the log:

1. From Brood level 2 upward, a day with three lines hauls clearly more than a day with one line to the same kind of patch.
2. At a mid Brood level, the first patch runs dry while at least one scout is still waiting, and its recruits are seen moving to the next line or idling.
3. The same line laid on the exposed side loses clearly more pieces to gusts than on the sheltered side, and a large template loses more than a small one.
4. A fresh save reaches the end card in 15 to 20 minutes of normal play, with the line on and with it off. Check the arc with a small economy simulation before asking the owner to play it.
5. Day hauls grow roughly tenfold from day 1 to the end card with the line on.
6. A skipped harvest and a watched harvest of the same day give the same haul.
7. Twenty unattended days (`Leaf.AutoMove`-style scripted scouts, random patches, forced gusts) all end.
8. The frame rate with a full pool out matches the figures in `tech.md` to within a few frames.

## Not in this work

- A "hold here in gusts" marker or any other order given to recruits. The owner found it too fiddly.
- Trails that fade with time, or that slow ants down.
- Anything that persists on the tree across days.
- A visibly growing nest or fungus garden (Game Off).
- Drop cutting, relay caches, castes (Game Off ideas).
