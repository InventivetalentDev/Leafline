# Leafline economy

Run the deterministic, standard-library-only model from the project root:

```sh
python3 Tools/econ_sim.py --check-cpp
```

It prints each day's wall-clock time, cumulative time, expected haul, lifetime haul, recruit pool, recruit trips, expected pieces lost, unreserved patch pieces left, harvest simulation seconds, and purchases. It then prints the one-tree campaigns, including tree/day, cut height, and remaining low-band supply. The optional `--check-cpp` reads `Upgrades.cpp` and `ColonyTypes.h` to check costs, all four milestone sets, effect ladders, stage defaults, and the within-tree gust rise. The script never starts Unreal or accesses a save.

With one tree **off**, the unchanged recommendation meets the pacing and haul-growth targets in this model:

| Mode | End-card day | Played time | Day-one haul | Final-day haul | Haul growth |
|---|---:|---:|---:|---:|---:|
| Recruits, watched harvest at 5× | 5 | 16.80 min | 9.4 | 94.4 | 10.08× |
| Recruits, skipped harvest at 400× | 5 | 15.66 min | 9.4 | 94.4 | 10.08× |
| By hand, recruits disabled | 5 | 18.52 min | 9.4 | 22.6 | 2.41× |

These are estimates of play, not measured Unreal results. Route lengths, leaf choices, player delays, and exposure distributions still need playtesting. Assertions enforce the 15–20-minute watched/by-hand windows, 7–14× recruit haul growth, finite patch/trip budgets, a meaningful advantage for three lines, and equal modeled outcomes when harvest is skipped.

## Applied tuning

All five upgrades have six purchasable levels. Costs for levels 1–6 are **14 / 32 / 68 / 144 / 304 / 640**. Effects are absolute at each level, rather than compounded.

| Upgrade | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Mandibles, cut-rate multiplier | 1 | 1.25 | 1.5 | 1.8 | 2.15 | 2.55 | 3 |
| Strength, carry-penalty multiplier | 1 | 0.75 | 0.55 | 0.4 | 0.3 | 0.22 | 0.16 |
| Grip, hold multiplier | 1 | 1.25 | 1.5 | 1.8 | 2.15 | 2.55 | 3 |
| Stamina, sprint-drain multiplier | 1 | 0.8 | 0.65 | 0.5 | 0.4 | 0.32 | 0.25 |
| Brood, shared recruit pool | 2 | 3 | 4 | 6 | 8 | 10 | 12 |

With one tree off, lifetime-haul milestones are **20 / 95 / 240** with recruits and **5 / 30 / 80** by hand. The separate one-tree sets are described below. Spending never reduces lifetime haul. Scouts remain three per day at every Brood level. High upgrade levels remain available after the end card; a fresh campaign does not need to max them.

The script evaluates the six steep cost ladders in `COST_CANDIDATES`. For each, it runs both modes from an empty colony. It chooses the day end nearest 17.5 minutes, rejects candidates outside the pacing/growth windows, then scores the survivors by time error plus logarithmic error from 10× haul growth. Milestones are 10%, 40%, and 100% of that final cumulative haul, rounded down to multiples of five. The final threshold must fall above the previous day's haul and within the selected day's haul.

## Inputs from the game and implemented line core

Every tuning assumption is a named constant near the top of `Tools/econ_sim.py`. The source-default inputs come from `DayDirector.cpp`, `ColonyTypes.h`, `AntPawn.cpp`, `AntMovementComponent.h/.cpp`, `WindSystem.h/.cpp`, and `LineSystem.h/.cpp`. The line-core values were also confirmed by the main session.

| Input | Model value |
|---|---|
| Scouts | Three; one piece each; the next scout starts after send-home |
| Leafy height | 20 m on day one, then +7 m/day |
| Walk / sprint / trail riding | 2 m/s / 2× walk / 6 m/s |
| Recruit outbound | 3.4 m/s |
| Recruit home | 2 m/s × carry factor × 1.5 |
| Carry factor | `max(0.45, 1 - 0.08 * StrengthEffect * sizeMass)` |
| Fragment size mass | `1 + 4 * size`, with size in 0–1 |
| Delivered value | Size mass × source-leaf richness, richness in 1–3 |
| Fragment diameter | `30 + 70 * size` cm |
| Cut time | `size / (0.28 * MandiblesEffect)` seconds, scouts and recruits |
| Recruit budget | Three trips each, shared pool from Brood; departures globally spaced 1.5 seconds |
| Patch limits | Up to four leaves and ten pieces, including the scout's own piece |
| Leaf capacity approximation | Three small pieces, two when size exceeds 0.6 |
| Harvest playback | 5×; skipping uses 400× with the same simulated outcomes |

Richness changes credited value; size mass controls carry slowdown and sail size. Strength applies to recruits as well as scouts through the existing ant-upgrade application.

## Estimates to replace with measurements

The model uses the same patch kinds for every cost candidate and both modes. All three scouts cut successfully and establish distinct, non-overlapping patches. Each patch's pieces have the same expected richness as its scout's piece.

| Estimate | Default and interpretation |
|---|---|
| Route length | `(leafyHeight + 25 m lateral travel) * 1.2 detour + 10 m ground travel` |
| Later scouts | Ride 55% of the route at 6 m/s; walk/sprint the remaining 45% to a new patch |
| Player delays | 20 s searching/steering per scout, 1.4 s per handoff, 22 s/day reading recap and shopping |
| Sprint use | Use 35% of sustainable sprint duty; duty is `0.22 / (0.22 + 0.17 * StaminaEffect)` |
| Chosen size | 0.40 on day one, +0.09/day, capped at 0.90 as the player learns |
| Chosen richness | 1.20 on day one, +0.25/day, capped at 3; represents choosing higher/outer leaves |
| Patch leaves | Two, three, four for the three scout patches |
| Patch capacities | Day one: 6 / 9 / 10 total pieces; at size >0.6: 4 / 6 / 8 |
| Route shelter | 60% of chosen routes sheltered; other routes have maximum exposure uniformly distributed over 0.35–1 |
| Controlled-scout wind avoidance | Scout loss rate is 75% of the unattended loaded-return rate |
| Loss recovery | Add 12 s × loss probability to the recruit's trip time |

The leaf-capacity approximation does not evaluate exact bite geometry, initial remaining area, leaf approach failures, overlapping patches, or the player's choice to return empty. It uses the main session's three-small/two-large rule and the implemented patch caps. Actual `Leaf.Patch` measurements should replace `PATCH_LEAVES` and the approximate size boundary first. A richer-leaf distribution from the tree should replace the expected choice progression separately.

Wind follows the game's full-gust force test: peak force is `0.22 * (1 + 0.15 * (day - 1))`, sail area is `(diameter / 40)^2`, body area is 1, and hold is `GripEffect`. Sheltered routes multiply exposure by 0.25. The model integrates the fraction of route classes where force exceeds hold, weighted by encountering a gust. The mean calm/hold lengths and their daily changes use the source defaults. Losses are expected values, so the table can show fractional lost pieces. A lost recruit trip still consumes one of its three trips. Consumed pieces include lost loaded returns; the model does not replay actual gust phases or ant positions. At these campaign days an empty ant remains below the full-exposure blow-off threshold.

## Shared pool, finite patches, and purchases

Scouts travel sequentially under player control; their returns overlap later scouting. A patch opens when its scout sends a piece home. Idle recruits choose the live patch with most unreserved pieces per assigned ant, reserve a piece, then leave through a global 1.5 s departure queue. Equal scores use patch order; all patches have equal expected richness that day. A recruit returns before spending its next trip. With no live patch it waits for the next scout. No patch can be overdrawn and no worker can exceed three trips.

Harvest begins after the last scout's handoff only if recruit work remains. Remaining simulation time counts at a fifth toward wall-clock time; without harvest, the final scout's return counts in full. Skip playback changes only that conversion to wall time. The equality check validates the model's time accounting; it does not verify Unreal's fixed-step implementation.

Between days the simulated player repeatedly buys the affordable level with greatest positive gain in next-day expected haul per second, divided by cost. It reevaluates after every purchase, breaking ties by upgrade order. Brood is unavailable with recruits off. If nothing affordable improves the estimate, balance carries forward. This policy has one-day foresight; it approximates a player who understands the upgrades.

The recommended recruit campaign buys nothing on day one, Brood 1 after day two, Brood 2 and Grip 1 after day three, then Grip 2 and level 1 Strength, Mandibles, and Stamina after day four. The end card appears before day-five purchases.

| Day | Recruits: cumulative minutes | Recruit haul | By hand: cumulative minutes | By-hand haul |
|---|---:|---:|---:|---:|
| 1 | 3.20 | 9.4 | 3.20 | 9.4 |
| 2 | 6.67 | 12.9 | 6.67 | 12.9 |
| 3 | 9.85 | 46.5 | 10.42 | 16.9 |
| 4 | 13.26 | 77.9 | 14.36 | 19.9 |
| 5 | 16.80 | 94.4 | 18.52 | 22.6 |

In the separate day-four comparison, the other four upgrades are level 1 and the scout budget stays three. The one-line case lets only the first scout establish a recruit line; the other scouts still bring their own pieces. Brood levels 2–6 produce **1.99× / 2.48× / 2.97× / 2.97× / 2.97×** as much haul with three lines. At Brood 4 and above all three patches run dry; more pool cannot create more leaf pieces. On day four, a size-0.9 fragment at Grip 0 has modeled exposed-route loss of 78.6% versus 0% in shelter; size 0.3 has 0% in both classes.

## Save and integration notes

The line economy used save version 2. In that migration, a version-1 colony kept its balance, lifetime haul, day, seed, end-card flag, and existing upgrade levels (clamped to 0–6); the new best-day record started at zero because the old file had no day-haul history. Brood levels kept their numeric level and began representing recruits. The one-tree work raises the save version again and adds bites and stage state; its save implementation and migration are owned by the main session. This simulator neither reads saves nor verifies migration.

`BestDayHaul` is saved. `RecordDayHaul` accepts a higher finite haul, broadcasts the change, and leaves persistence to `SaveNow`. The recruit flag defaults to true and remains runtime-only; `SetRecruitsEnabled` broadcasts changes, and the pool and milestone queries use the current flag. `GetWorkersPerDay(Base)` always returns `Base`.

For integration tests, launch with a unique option such as `-LeafSlot=LineEconomyTest`. Loading, purchases, reset, migration, and explicit saves all use that selected slot; the default remains `Leafline`, user index zero. `Leaf.Give <mass>`, `Leaf.Buy <0..4>`, `Leaf.ResetSave`, and `Leaf.Upgrades` keep their existing syntax. `Leaf.Upgrades` reports levels through 6, pool, best day, and the current milestone set. `Leaf.ResetSave` resets the selected slot.

C++ compilation, actual save migration, state-machine termination, and runtime performance remain for the main session's Unreal verification. No engine build or launch was used to produce these recommendations.

## One tree per stage

With `FDayTuning::bOneTree` on, measured supply gives these thresholds. Costs, upgrade effects, and one-tree-off tuning stay unchanged.

| Mode | Lifetime-haul thresholds | Days on tree 1 / tree 2 | End-card day | Watched harvest | Skipped harvest |
|---|---|---|---:|---:|---:|
| Recruits enabled | **25 / 155 / 270** | 4 / 1 | 5 | **17.13 min** | 15.83 min |
| By hand | **5 / 60 / 95** | 4 / 1 | 5 | **18.57 min** | 18.57 min |

Both modes move to tree 2 on day 5 and start tree 3 on day 6 when continuing after the end card. Only the second and third milestones trigger moves, at most one per dawn. Tuning targets day four for the second milestone and an end day nearest 17.5 minutes within 15–20 minutes, rounding lifetime-haul thresholds down to five.

### Recruits from day 1 (15 Nov)

The owner's decision (`docs/briefs/recruits-day-1.md`): Brood level 0 has a pool of two, the ladder is **2 / 3 / 4 / 6 / 8 / 10 / 12**. Every level adds recruits, and the model's greedy player buys Brood 1 after day 1 (the first purchase is worth its price). With recruits off nothing changes. The one-tree-off cost search of `Tools/econ_sim.py` finds no candidate with two recruits on day 1 and is skipped with a note; the default game is one tree.

`python3 Tools/econ_sim.py --one-tree` runs the default game with today's costs; `--milestones a,b,c` evaluates a set; `--richness height` takes richness from the cut height like `FLeafSet::Build` (mean outward share), linear over the crown or, with `--height-scale m`, the climb option's `1 - exp(-dz / m)`. The default `--richness day` keeps the older per-day guess. Height richness runs about a day slower than the per-day guess, so compare variants within one mode.

| Variant | Milestones (one tree, recruits) | Model: end day, days on tree 1 / 2, minutes | AutoPlay twenty days: milestone days, end card, haul |
|---|---|---|---|
| 14 Nov (no recruits until Brood 1) | 25 / 155 / 270 | day 5, 4 / 1, 17.13 min | 3 / 12 / 16, after day 16, 487.78 |
| **A**: today's defaults, recruits from day 1 | **25 / 155 / 270** (unchanged) | per day: day 5, 3 / 2, 16.04 min; height (linear): day 6, 4 / 2, 19.71 min | 1 / 6 / 8, after day 8, 989.71 |
| **B**: `RichnessHeightScale` 40, recruits from day 1 | **25 / 190 / 335** | height (40 m): day 6, 4 / 2, 19.38 min | 1 / 6 / 8, after day 8, 1497.22 |

A stays in the 15–20 minute window, so its thresholds stay. For B, any second milestone from 150 to 190 and third from 255 to 335 gives the same model day; 190 / 335 is the top of that range, which in AutoPlay gives the same milestone days as A. The 14 Nov climb set (25 / 240 / 390) now ends on model day 7 at 22.7 min, outside the window. AutoPlay is a slow scripted player (one small line per scout): two recruits from day 1 double its twenty-day haul and halve its days to the end card, while the model's player ends on day 5 or 6 either way.

Day 1's first patch: the lowest leaf is a one-leaf patch with two pieces left after the scout's cut (template 0.6), so the two recruits take both on their first trip ("patch 0 of 3 left (2 reserved)"); their second and third trips wait for the next trail.

Retain the updated stage defaults. Configured leafy height is a generation parameter; actual first leaves appear higher.

| Tree | Trunk multiplier | Configured leafy height | Observed first leaves | Richness multiplier | Gust multiplier | Sparse spacing / blend |
|---|---:|---:|---:|---:|---:|---|
| Old giant | 1.0 | 20 m | 32.4 m | 1.0 | 1.0 | 12 / 55 m |
| Spreading giant | 1.4 | 26 m | about 36 m | 1.3 | 1.2 | 8 / 45 m |
| Tall spire | 1.15 | 32 m | about 33 m | 1.6 | 1.4 | 8 / 45 m |

Assumptions and limits:

- `TREE_SUPPLY` loads [the measured reports](../Tools/tree_bands_measured.txt): **22,626 / 19,392 / 29,820** template-0.5 pieces. Absolute 10 m bands use the observed first-leaf height initially and lower edges thereafter. Results represent these three seeds.
- Low patches use 1/2/3 leaves; higher patches retain the old estimate. Bands pool supply and reserve distinct scout patches. Cuts consume one unit, or 1.5 above size 0.6, including lost pieces. Uncut supply survives overnight; bite geometry and patch reach remain approximate.
- Learning, purchases, routes, and shelter remain player estimates. Yesterday's trails provide 55% shared-route riding; move cards cost four seconds. Gust peaks rise 6% per day on a tree with stage multipliers. Skipping harvest preserves cuts, losses, supply, and haul.
- Scripted lowest cuts plateau before reaching 57.9 m on day 13. Its random 0.3–0.95 sizes and days losing every recruit trip are not the modeled player's choices or expected losses. Strictly rising daily height and exact bough depletion remain unverified.

`python3 Tools/econ_sim.py --check-cpp` validates all four milestone sets, stage defaults, finite supply/trips, deferred moves, continuation, and harvest equivalence. One-tree-off simulation results remain unchanged.
