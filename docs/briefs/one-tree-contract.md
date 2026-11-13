# One tree: work split and interfaces

Companion to `docs/briefs/one-tree.md`. Read that first: it is the specification. The main session owns the day flow
(`World/DayDirector.*`, `World/WindSystem.*`), the save and colony (`Game/LeaflineSaveGame.*`,
`Game/LeaflineGameInstance.*`), `Colony/ColonyTypes.h`, the debug commands (`Debug/*`), materials and every build.
The shared declarations below are already in the tree and compile (stub bodies where an agent fills in the code).

Rules for every agent:
- Edit only the files listed for you. Read anything else.
- Do **not** build, launch Unreal, or run `Scripts/*.bat`: one Unreal lock, one builder (the main session).
  Write code that compiles against the declarations below; the main session builds and fixes integration.
- Do **not** commit. Leave changes in the working tree and report what you changed, with anything you were unsure of.
- Keep to the hard rules in `CLAUDE.md` (clean room, math only, English teen-safe text, no trademarks).
- One log category, `LogLeafline`. Log what can be verified from the log.
- With the flag off (`FDayTuning::bOneTree = false`) the game must play exactly as at tag `pre-one-tree`.

## A. Tree agent (Opus): `Tree/LeafSet.*`, `Tree/TreeActor.*`, `Tree/TreeTypes.h` (do not change `FSavedLeaf`)

1. **Leaves that can no longer be cut look different from a distance** (`FLeafSet::BuildChunkMesh`).
   - Little area left (`Remaining <= 0.16`, the threshold in `FTreeLeaf::CanCut`; `StripBelow` also lands here): no blade.
     Draw only the stalk, from `Base` to a short stub past `Origin` (about 12% of `Length` along `Dir`), a thin strip about
     3 cm wide facing `Normal`. The stalk is not drawn today at all, so this is new geometry. A stripped bough must read
     as bare.
   - Bite limit reached with area to spare (`Bites >= 3 && Remaining > 0.16`): keep the bitten blade.
   - Vertex colour R marks the state: 1 for a leaf that can still be cut (as today), 0 for both spent kinds (blade and
     stalk). The main session makes `M_Leaf` turn R = 0 brown. Keep G, B and A as they are.
   - Time `ATreeActor::BuildLeafChunk` and log a warning when one rebuild takes over 4 ms (a hitch check).
2. **Density by height** (`FLeafSet::Build`). New params (already in `FTreeParams`): `SparseSpacingMul` (default 1) and
   `SparseBlendHeight` (cm). The spacing multiplier is `SparseSpacingMul` at `LowestLeafyHeight` and eases (smoothstep) to 1
   at `LowestLeafyHeight + SparseBlendHeight`, by the height of each leaf position. A spacing tweak alone is probably not
   sparse enough: the target is that the two lowest leafy limbs together hold only a few days of patches (a day takes up to
   three patches of up to four leaves), i.e. a few tens of cuttable leaves, so also drop whole twigs low down with
   probability `1 - 1 / multiplier`, from a separate random stream keyed by seed and branch index. Make both easy to tune;
   the main session tunes with `Leaf.TreeReport`.
   **With `SparseSpacingMul == 1` the leaf set must be bit-identical to today** (same random draws, same order). Baseline
   from the current code: seed 517192 with the old giant's params gives `8140 leaves in 21 chunks`, richness median 1.74,
   mean 1.73; seed 103900 (spreading giant) gives 6731 leaves in 17 chunks.
3. **Richness multiplier**: `FTreeParams::RichnessMul` (default 1) multiplies every leaf's `Richness` after it is computed.
   Vertex alpha stays the shape-based value `(Richness / RichnessMul - 1) / 2`, so the look does not saturate. Log it in
   the richness line.
4. **Saved bites** (declared in `TreeActor.h`, stubs in `TreeActor.cpp` to replace):
   - `ExportBites(Out)`: every leaf with `Bites > 0` or `Remaining < 1`, in leaf order: index, bite count, remaining share,
     and per bite the centre (along, across) and radius in `BiteData` (3 floats each).
   - `GrowWithBites(Seed, Bites, LeafCount)`: grow like `Grow`, apply the bites before the leaf meshes are built (one
     build in all; split `Grow` into parts as needed, keep `Grow(int32)` working as before). If `LeafCount` differs from the
     grown leaf count, or any entry is out of range or malformed, apply none of them and log a warning. Log
     `Tree: applied N saved bites (M leaves)` on success.
   - `StripBelow(Z)`: every leaf whose `Base.Z < Z`: `Remaining = 0`. Rebuild each changed chunk once. Return the count.
   - `FindSpentLeafNear(P, Reach)`: nearest leaf with `!CanCut()` whose blade start is within `Reach`, like
     `FLeafSet::FindNearest` (use the grid).
   - `CountPiecesLeft(Template, UsePerCut)`: sum of `PiecesLeft` over all leaves.
   - `LogTreeReport(Label)`: lines starting `TreeReport <Label>:` with, per 10 m height band from the lowest leaf up,
     live leaves (`CanCut`) / all leaves and pieces left at template 0.5; then the four lowest leafy limbs (level-1
     branches that carry leaves, by the height where they leave the trunk) with their height, live leaves / all leaves and
     pieces; then the whole tree's pieces left and the mean height of live leaves. Keep it to about 30 lines.
5. Keep `BuildPatch`, the patch reach cache and the approach code working on a kept tree (they are per tree, not per day).

## B. Trails agent (Sonnet): `Colony/TrailNetwork.*`

Yesterday's trails. Declared in `TrailNetwork.h`, stubs in `TrailNetwork.cpp` to replace.
- `FPath` gains a yesterday flag. `LoadYesterday(Routes)`: `ClearAll()`, then add each route (2+ points) as a yesterday
  path: points, distances, merge-drawn among yesterday paths as today, ribbon brightness a new `UPROPERTY float
  YesterdayGlow = 0.3f`, chunks built. The director calls it at day start before any worker starts a path.
- Today's paths ignore yesterday's when deciding where ribbon is already drawn, so a fresh trail over a faint one draws at
  full strength. Avoid z-fighting: draw yesterday's ribbon slightly lower (for example `Lift - 0.4`, at least 0.5 cm).
- Riding (`FindRideStart`) and the way home (`BuildReturnRoute`) treat yesterday's paths like any other (they start at
  the nest like every worker path). `SetLineGlow` never brightens a yesterday path: it stays at `YesterdayGlow`.
- `DrawnLength` counts today's paths only (the recap's "trail laid").
- `ExportToday(Out)`: today's paths with 2+ points (positions and normals). `IsYesterdayPath`, `NumYesterdayPaths`.
- `ClearAll` keeps clearing everything (used with one tree off and on a move).

## C. Economy agent (Codex): `Game/Upgrades.cpp` (the two one-tree milestone sets only), `Tools/econ_sim.py`, `docs/economy.md`

Model one tree per stage in the simulation and retune `MilestonesOneTreeWithRecruits` / `MilestonesOneTreeByHand`
(today placeholders equal to the old sets). Facts:
- Moves at the second and third milestones only (the next day starts on the new tree); one move per day at most (if both
  land on one day, the second move follows a day later). After the third milestone the end card shows and play continues
  on the third tree. Leaves never grow back on a tree; a move gives a fresh one.
- Per stage (`FDayTuning::Stages` in `Colony/ColonyTypes.h`): trunk height multiplier, lowest leafy height, richness
  multiplier, gust multiplier, sparse spacing. Within a stage the wind's peak force rises by `GustPerDayOnTree` (6%) per day
  on that tree; with one tree off it rises 15% per absolute day as before.
- The supply per tree comes from `Leaf.TreeReport` (pieces left per 10 m band). Until the main session sends measured
  tables, use an estimate and keep the table in one place in the sim (input file or constant) so it can be swapped.
- Higher leaves cost more: a longer climb and more exposure. Model trip time from the height of the lowest live leaves.
- Targets: with one tree on, recruits on and off: tree 1 lasts at least three days (aim for about four), the end card
  arrives in 15 to 20 minutes. Report days per tree and minutes to the end card for both. With one tree off the old
  numbers must stay as they are.
- Recommend stage values (leafy height, richness and gust multipliers) if the model wants others; the main session sets
  them in `ColonyTypes.h`.
- Write the result into `docs/economy.md` (a "One tree per stage" section).

## D. HUD agent (Sonnet): `UI/LeaflineHUD.*`

- `EDayPhase::Move` (new; the director shows it at the start of a day on a new tree, confirm starts play): a card with
  `D->GetMoveCardText()` large, a line such as "A new tree: taller, with richer leaves and stronger gusts.", and the
  confirm prompt in the style of the other cards. The world behind it shows the new tree.
- The goal "Climb the trunk: the leaves start about N m up" shows on the first day of each tree: with one tree on when
  `D->GetDayOnTree() == 1`, with it off on day 1 only (as now). Use `D->GetLowestLeafyHeight()`. The second first-day goal
  (riding) stays day 1 only.
- New hint "Stripped. Fresh leaves are higher up." where the patch read-out goes, when the controlled ant has no cuttable
  leaf in reach but `Tree->FindSpentLeafNear(<the point the read-out uses>, Reach)` finds one, or when the read-out finds
  no pieces.
- The colony meter: use `GI->GetMilestoneThreshold(m)` (it knows both flags) instead of
  `LeaflineUpgrades::GetMilestoneThreshold(m, bRecruits)`. With one tree on, mark the second and third milestones as
  moves (a small tree mark or "new tree" label).
- Recap: with one tree on, a line such as "Tree 1, the old giant: day 3 on this tree". On the recap and the nest screen,
  when `D->IsMoveDue()` (declared by the main session: the next day starts on a new tree), a line "Tomorrow the colony
  moves to a new tree".
