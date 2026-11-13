# Brief: one tree (a persistent tree per colony stage)

For the main session. Agreed with the owner on Fri 13 Nov, morning. It replaces "new day, new tree": the colony lives on one tree, the leaves it cuts stay cut, and the colony moves to a new, bigger tree at colony milestones.

This brief is the specification. When the work lands, update `design.md` (core loop, day structure, day scaling, the out-of-scope table), `tech.md`, `docs/economy.md`, `plan.md` (a milestone "M5c: One tree") and `devlog.md` yourself.

## Before you start

- Tag the current commit `pre-one-tree`.
- Keep this work in its own commits, with messages starting `Tree:`.
- **Order in the plan:** this comes before the bird. The bird moves to Game Off unless everything here is kept and finished early.
- **Owner's verdict: tonight (Fri).** The owner judges this together with the line.
- Build the parts in the order of "Build order" below, so that a late stop still leaves a working game.

## Rules agreed with the owner

1. **One tree per colony stage.** The tree is grown once and kept from day to day. Days still have three scouts, a recap and the nest screen.
2. **Cut leaves stay cut.** Bites and used-up leaves carry over to the next day and survive quitting and relaunching.
3. **Leaves never grow back.** The only reset is moving to a new tree.
4. **The colony moves at colony milestones.** When the second or the third milestone is reached, the next day starts on a new tree: a different shape, taller, with richer leaves and stronger gusts. A short card says so, for example "The colony moves to a tall spire". The first milestone does not move the colony; see "Where the agreed wording changed". After the third milestone the end card shows, as now, and play continues on the third tree.
5. **The tree is sparse low down and dense high up.** The lowest boughs hold only a few patches, enough for two or three days. After that the easy leaves are gone and the player has to climb into more wind.
6. **Yesterday's trails stay for one more day.** They are faint, can be ridden, and carry no recruits. They vanish after the second night, and on a move.
7. **The wind still changes every day**: a new direction daily, so the sheltered side moves.

## Decided without asking the owner

The owner can veto any of these; they are in the hand-over message too.

- A leaf that can no longer be cut must look different from one that can, from a distance. With little area left (the existing threshold), it loses its blade and keeps only its stalk, so a stripped bough looks bare. If it only hit the three-bite limit with area to spare, it keeps its bitten blade but turns brown.
- Leaf bites are saved only together with the haul they produced, at the end of the day. Quitting in the middle of a day restarts that day from dawn: no bites, no haul.
- The nest stays at the same spot on a tree for the whole stage.
- Within a stage, only the wind gets harder day by day (a modest rise in gust strength). The big steps in difficulty are the moves. The rule "leaves start N metres higher each day" is removed.
- The four existing shapes are used in a fixed order: the old giant, a spreading giant, a tall spire, a leaning elder. Later trees repeat the order with new seeds.
- A save from before this change keeps its day count and upgrades and starts on a fresh tree for the stage its milestones put it in.

## Where the agreed wording changed

The owner agreed to a move at **each** of the three milestones. `docs/economy.md` puts the end card on day 5, with the milestones reached on about days 2, 4 and 5. A move at each would give tree 1 two days, tree 2 two days and tree 3 one, which is too short for rule 5 to be felt.

So the rule is: **moves at the second and third milestones only.** Tree 1 lasts about four days, tree 2 runs from there to the end card, and tree 3 is what the player continues on. The first milestone stays a mark on the colony meter. The same rule applies with recruits on and off.

## The off switch

One tuning flag, for example `FDayTuning::bOneTree`, plus `Leaf.OneTree 0|1`. If the owner rejects the persistent tree tonight, the flag ships off.

Gated by the flag, so that with it off the game plays as it does at `pre-one-tree`:
- keeping the tree and its bites from day to day, and saving them;
- moves and the move card;
- leaf density by height and the per-stage scaling;
- yesterday's trails;
- the milestone thresholds: keep today's two sets (with recruits, by hand) for flag off, and add two retuned sets for flag on.

With the flag off, the tree is grown new every day with the rising leafy height, as now. Keep that code path working.

Not gated, because they are harmless either way: the look of leaves that can no longer be cut, and the new debug commands.

This flag and `bRecruits` are independent. All four combinations must play to the end card.

## Technical notes

**Day start.** With the flag on, `StartDay` no longer calls `Tree->Grow` unless the stage changed or nothing is grown yet. It resets the roster, the lines and the day's statistics, keeps the leaves, and starts the wind for the new day. Seed the wind from the tree's seed and the day number.

**Tree identity.** Stage number and tree seed are saved. Shape and leafy height come from the stage, not the day.

**Save timing.** The day's bites and the day's haul go into the save in one write, at the end of the day. Check that nothing writes the haul earlier (deposits, purchases) without the bites, or the bites without the haul. A purchase on the nest screen happens after that write, so it is safe.

**Saving leaves.** Store only the leaves that have been bitten: leaf index, bite count, remaining share, and whatever `PlanBite` needs to reproduce the holes. Leaf indices are only stable for the same seed, parameters and code, so also store the leaf count; if it does not match after loading, drop the saved bites and log a warning. Raise the save version.

**Loading.** Grow the tree from its seed, apply the saved bites, then build the leaf meshes once. Do not rebuild chunks bite by bite.

**Leaves that can no longer be cut.** Removing blades changes chunk geometry; check that a day's worth of stripping does not cause visible hitches.

**Density by height.** Vary leaf spacing with height above the lowest leafy branch: wide at the bottom, the current spacing from part-way up. Tune it with the measurement in the tests below, not by eye.

**Stage scaling.** Per stage: tree height and lowest leafy height, a multiplier on leaf richness, and gust strength. Re-run the economy simulation with these and record the result in `docs/economy.md`.

**Yesterday's trails.** Save the points of each of the day's routes at the end of the day. On the next day they load as one faint, ridable set with no lines attached. Drop them when the following day ends, or on a move. `BuildReturnRoute` and riding must treat them like any other trail.

**Hints.** "Climb the trunk: the leaves start about N m up" shows on the first day of each tree. Add one for a patch read-out that finds nothing: "Stripped. Fresh leaves are higher up."

**Debug commands** (add to the table in `tech.md`): `Leaf.OneTree 0|1`, `Leaf.Move` (move to the next tree now), `Leaf.Strip <metres>` (use up every leaf below a height, for testing the climb), `Leaf.TreeReport` (pieces left per 10 m height band).

**Review.** Have Codex review the save and load paths and the new day start before the owner plays.

## Build order

1. Persistent tree across days in one session, the flag, the look of leaves that can no longer be cut.
2. Saving and loading leaves.
3. Density by height, stage scaling, moves at milestones, the move card, economy numbers.
4. Yesterday's trails. This is the first part to cut if time runs short; without it, trails are cleared every night as now.

## Acceptance tests

Each shown in the log or in a screenshot.

1. Play two days, quit, relaunch: same tree, same bites, same day number.
2. `Leaf.TreeReport` at the start of each day of a scripted colony shows the lowest bands emptying. By the fourth day on tree 1 the two lowest leafy boughs have no live patch.
3. The mean height of the day's cuts rises from day to day within a stage in a scripted colony.
4. Reaching the second or third milestone moves the colony, and reaching the first does not: the next day starts on a new shape with fresh leaves and no trails, the card shows, and the save holds the new stage.
5. The economy simulation reports days per tree and minutes to the end card for one tree on, with recruits on and off: tree 1 lasts at least three days and the end card arrives in 15 to 20 minutes.
6. Day start with a kept tree is faster than a day start that grows one.
7. Twenty unattended days across at least two moves all end, with a save and reload between each.
8. A version-2 save loads and plays.
9. With the flag off, a day plays as at `pre-one-tree`: a new tree each day, the old leafy heights, the old thresholds.
10. If part 4 is built: yesterday's routes appear faint, can be ridden, attract no recruits, and are gone a day later.
11. On one tree, the wind direction in the log differs on consecutive days.
12. The line's tests are re-run on the sparse tree, low down and part-way up: patch sizes from `Leaf.Patch`, and "three lines haul clearly more than one" from Brood level 2 upward.
13. Recruits off with one tree on plays to the end card, with moves at the second and third milestones.
14. Quit in the middle of a day and relaunch: the day restarts from dawn with the bites and haul of the day before, and nothing from the abandoned day.

## Not in this work

- Leaves growing back, seasons, weather other than wind.
- Choosing which tree to move to, or going back to an old one.
- Several trees in the world at once (Game Off).
- The bird.
