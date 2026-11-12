# The line: work split and interfaces

Companion to `docs/briefs/the-line.md` (read that first: it is the specification). The main session owns the line
core (`Colony/LineSystem.*`, `World/DayDirector.*`, `Ant/*`, `World/WindSystem.*`, `Colony/TrailNetwork.*`, the
controller, camera and debug commands). Three parallel agents each own a disjoint set of files.

Rules for every agent:
- Edit only the files listed for you. Read anything else.
- Do **not** build, launch Unreal, or run `Scripts/*.bat`: one Unreal lock, one builder (the main session).
  Write code that compiles against the declarations below; the main session builds and fixes integration.
- Do **not** commit. Leave changes in the working tree and report what you changed.
- Keep to the project's hard rules in `CLAUDE.md` (clean room, math only, English teen-safe text, no trademarks).
- One log category, `LogLeafline`. Log what can be verified from the log.

## A. Tree agent: richness and patches (`Tree/LeafSet.*`, `Tree/TreeActor.*`)

1. **Richness.** Add `float Richness = 1.f;` to `FTreeLeaf`, set in `FLeafSet::Build` from the tree's shape only:
   1× at the lowest leafy branches, about 3× at high outer tips (height in the crown and how far out the leaf is
   from the trunk axis both count). Log the distribution once per build (min, median, max).
   Write it to the leaf mesh as vertex colour alpha `(Richness - 1) / 2` in `BuildChunkMesh` (alpha is 0 today and
   unused; the main session will make `M_Leaf` turn pale young green with it). Decor leaves (grove crowns,
   `AddDecorLeaf`) get alpha 0.
2. **Pieces left.** `int32 FLeafSet::PiecesLeft(int32 Leaf, float Size01, float UsePerCut) const`: how many more
   pieces of that size the leaf gives under today's limits (`CanCut`: fewer than 3 bites and `Remaining > 0.16`;
   each piece needs `SizeCap >= Size01 - 0.001`; read `ApplyBite` for how a bite reduces `Remaining`).
3. **Patch membership.** On `ATreeActor`:
   ```cpp
   struct FPatchRules { int32 MaxLeaves = 4; int32 MaxPieces = 10; float MaxWalk = 700.f; float Template = 0.5f; float UsePerCut = 0.38f; };
   /** The patch around a leaf: CutLeaf first, then walkable leaves nearest first along the bark. */
   void BuildPatch(int32 CutLeaf, const FPatchRules& Rules, TArray<int32>& OutLeaves) const;
   ```
   Membership follows the brief: leaves on the same branch as the cut leaf, then on neighbouring twigs of the same
   bough (and the bough itself), nearest first by distance along the bark (not a bare radius). Leaves grow on
   boughs (level 2) as well as twigs (level 3), so "same twig" can be a bough. Stop at `MaxLeaves`, at `MaxPieces`
   (sum of `PiecesLeft` at the template; `CutLeaf` counts with the pieces it has left plus the scout's own piece,
   i.e. count CutLeaf's `PiecesLeft` + 1 when called right after the scout's cut — the caller passes that in; keep it
   simple: count `PiecesLeft` for every leaf and let the caller add 1), and at `MaxWalk`. Leave out leaves with no
   pieces left and leaves whose approach (below) cannot be built.
4. **Approach route.** On `ATreeActor`:
   ```cpp
   /** A walkable route over the bark from From (on branch FromBranch, or the nearest branch if INDEX_NONE) to the
    *  base of a leaf, following the branch graph (along FromBranch to the junction, along the parent, up the leaf's
    *  branch). Points about 10 cm apart, each projected onto the walking field, normals from the field. False if no
    *  valid route (a step leaves the bark, the route is longer than MaxWalk, or the leaf is invalid). */
   bool BuildLeafApproach(const FVector& From, int32 FromBranch, int32 Leaf, float MaxWalk, TArray<FTrailPoint>& OutRoute) const;
   /** The straight line A-B stays within 10 cm of bark (move the lambda from AAntPawn::StartReturn here). */
   bool StaysOnBark(const FVector& A, const FVector& B) const;
   ```
   `FTrailPoint` is in `Colony/ColonyTypes.h`. The skeleton: `FTreeBranch::Parent`, `ParentT` (0..1 along the
   parent's samples), `Level`, `Samples`, `SampleRadius`, `SampleDist`; `TreeSkeleton::SampleAt`. The walking field
   is `FTreeField` (`Distance`, look for a gradient/normal and a projection helper; the movement component projects
   with it, see `UAntMovementComponent::SnapToSurface` and friends). Keep the ant on the upper side of branches
   where it can, and turn toward the leaf's `Base` near the end. Validate each consecutive pair with `StaysOnBark`.
   The recruit follows the points with `UAntMovementComponent::StartFollow`, which projects onto the field as it goes.
5. Report: richness distribution on seed 517192 (or any), typical patch sizes in leaves and pieces at templates
   0.3 / 0.6 / 1.0, and how many candidate leaves failed the approach.

## B. Codex: ladders, milestones, save v2, economy (`Game/Upgrades.*`, `Game/LeaflineGameInstance.*`, `Game/LeaflineSaveGame.*`, `Game/LeaflineEconomyCommands.cpp`, `Tools/econ_sim.py`, `docs/economy.md`)

1. **Ladders.** `LeaflineUpgrades::MaxLevel = 6`, `Effects[7]` (levels 0..6), six steeply rising costs.
   Brood's meaning changes: its effect is the **recruit pool size** (start from 0 / 2 / 3 / 4 / 6 / 8 / 10),
   description along the lines of "More recruits to work your lines." Scouts stay at three a day at every level:
   `GetWorkersPerDay(Base)` returns `Base`.
2. **Milestones.** Two sets of three: with the line on, and a smaller "by hand" set for when recruits are off.
   `float GetMilestoneThreshold(int32 Index, bool bRecruits)`.
3. **Game instance API** (the HUD and the main session code against exactly these):
   ```cpp
   bool AreRecruitsEnabled() const;          // runtime only, default true; not saved
   void SetRecruitsEnabled(bool bOn);        // broadcasts OnChanged
   int32 GetRecruitPool() const;             // Brood effect, 0 while recruits are disabled
   float GetBestDayHaul() const;             // saved
   bool RecordDayHaul(float DayHaul);        // updates the best if higher; true if it is a new best; saved on SaveNow
   ```
   Milestone functions (`HasReachedMilestone`, `GetReachedMilestoneCount`, `IsEndCardDue`) use the current flag.
4. **Save version 2.** Add `BestDayHaul`. A version-1 save must load and be migrated (levels clamped to the new
   range, nothing else lost); today `Init()` throws away any save whose version differs, which would wipe the
   owner's colony. Unknown newer versions may still start fresh.
5. **Test slot.** A command-line option `-LeafSlot=<name>` selects the save slot (default `Leafline`), so automated
   runs never touch the owner's save.
6. **Economy commands** (`LeaflineEconomyCommands.cpp`): keep them working with the new ranges.
7. **Economy simulation** `Tools/econ_sim.py` (stdlib only), documented in `docs/economy.md`: a fresh save played
   to the end card, with the line on and with it off. Model per day: three scouts (trip time grows as the lowest
   leafy branch rises 7 m a day from 20 m; climbing about 2 m/s, with trail riding faster on later trips), piece
   mass `lerp(1, 5, size) × richness` (richness 1..3, higher and further out), wind losses by size and Grip,
   recruits from the pool with 3 trips each (trip = line out at 3.4 m/s + cut + home at about 2 m/s laden; patch
   6 to 10 pieces at day 1; patches run dry), harvest played at 5× (it counts toward wall-clock time at a fifth),
   and a player who buys upgrades between days (pick a sensible greedy policy). Put every assumption as a named
   constant at the top so the main session can replace guesses with measured numbers later.
   Targets from the brief: end card at 15 to 20 minutes of play with the line on **and** off; day hauls grow
   roughly tenfold from day 1 to the end card with the line on; from Brood level 2 up, three lines clearly beat one.
   Output: per-day table (time, haul, purchases), end-card time, and the recommended `Upgrades.cpp` numbers. Apply
   those numbers in `Upgrades.cpp`.

## C. HUD agent (`UI/LeaflineHUD.*` only)

Code against `Colony/LineSystem.h` (as declared; bodies are being written), `World/DayDirector.h` (new: `IsHarvest`,
`IsSkippingHarvest`, `GetHarvestView`, `GetLineSystem`, `IsNestRowVisible`, `AreRecruitsOn`; phase
`EDayPhase::Harvest`), `Colony/ColonyTypes.h` (`FDayStats`: `RecruitPieces`, `RecruitLost`, `ScoutMass`,
`PreviousBest`, `bNewBest`) and the game-instance API in section B.3 (being written by Codex; `MaxLevel` becomes 6).
`AAntPawn` gains `float GetCarriedValue() const` (mass × leaf richness: what the colony is credited).

1. **"+N" at the nest** for every entry in `ALineSystem::GetRecentDeliveries()`: projected from `WorldPos`, rising
   and fading over about 1.5 s, slightly larger as `Chain` grows. Skip ones behind the camera.
2. **Colony counter** at the top of the screen: total hauled (`GetTotalHauled()`), ticking up smoothly to the real
   value; today's haul beside it.
3. **Line rows**: one compact row per line (`GetLines()`): number, pieces home, pieces lost, patch remaining (a small
   bar from `Fullness()` plus the count), and richness. Dim lines that are no longer live. Under them, when
   `NumIdle() > 0` during a shift: "4 workers idle: lay a new line". Only while recruits are on and the pool is not
   empty.
4. **Patch read-out**: when the controlled ant has a cuttable leaf in reach (`GetLeafInReach()`, not carrying), call
   `GetPatchPreview(Leaf, Template)` with the current cut size while cutting, else `Tuning.PreviewTemplate`, outline
   the patch's other leaves faintly (reuse how `DrawLeafTarget` outlines the target leaf) and show
   "This patch: about 8 pieces, rich ×2.1". Only while recruits are on.
5. **Harvest overlay** during `IsHarvest()`: a short title ("Harvest: the colony brings in the rest"), the speed
   (`GetSpeedFactor()`, "×5" or "skipping…"), recruits out / trips left, and the controls: "[Enter] skip   [Tab] next
   line" (match how other prompts name keys/buttons).
6. **Nest screen**: ladders up to level 6 (pips or "3/6"), and skip the Brood row when
   `IsNestRowVisible(Brood)` is false. Row indices and hit-box ids stay the upgrade index (the director handles
   navigation over hidden rows); keep "next day" as id `LeaflineUpgrades::Count`.
7. **Recap**: count up line by line (each line's row appears in turn with its numbers counting up, then the day
   total), and mark a best day ("Best day yet" when `bNewBest`, else show the best so far). Keep it readable at
   1080p and phone-sized UI scale as the current recap does.
