# Brief: recruits from day 1

For the main session. Written Sun 15 Nov, 10:15. The owner's decision this morning. It goes into today's final package.

## Why

The line is the game's Director's Cut: you lay a trail and the colony follows it. Today a new colony has no recruits until the first Brood level, which the economy model buys after day two. A judge who plays ten minutes may never see it.

## The change

**A new colony has two recruits.** The first piece the player sends home on day 1 makes a trail, and two recruits walk out along it.

- The pool by Brood level is now `0 / 2 / 3 / 4 / 6 / 8 / 10` (`Game/Upgrades.cpp`). Level 0 becomes 2. Choose the rest of the ladder with `Tools/econ_sim.py` so that every Brood level still adds recruits and the first purchase is worth its price; for example `2 / 3 / 4 / 6 / 8 / 10 / 12`.
- With `Leaf.Recruits 0` nothing changes: no recruits at any level.
- No save change. The pool follows the Brood level, so an existing save simply has more recruits from its next day.

## What else it touches

1. **Milestones and the end card.** Day 1 hauls more, so everything arrives sooner. Re-run the economy model for one tree and adjust the thresholds only if the end card leaves the 15 to 20 minute window. Then twenty unattended days, twice: every day ends, no rescues.
2. **The bird.** Its first day is "the first day the pool is above zero" (`BirdSystem.h`), which would now be day 1. Day 1 stays calm: with recruits on, the bird first comes on day 2, with `FirstDaySwoops` and the hint as now.
3. **Day 1's flow.** Day 1 has never had recruits. Check on a fresh save: the first trail brings recruits out, the "recruits waiting for a new trail" row and the trail rows appear, the harvest runs when the last scout heads home, and the recap lists the recruits' pieces.
4. **Day-1 patches.** Low patches hold one to three leaves. Two recruits with three trips each should not strip the first patch before the player has seen them work; if they do, say so with the numbers before changing anything.
5. **Text.** The Brood line says "More recruits…", which stays true. `docs/ui-text.md` notes that on a fresh save Brood "is where recruits are first met": that is no longer so. Check that some line on day 1 tells the player what the recruits are doing when they first walk out; add one in the existing hint style if none does.
6. **Docs and pages.** `design.md` (the Brood row and the upgrade table), `economy.md`, `tech.md`, and any line in `docs/ship/` that says recruits come from the first Brood level. The video's caption "Buy Brood: more recruits." stays true.

## Acceptance tests

1. Fresh save, day 1, from the log and one screenshot: the first send-home makes a trail, two recruits go out, bring pieces home and the day ends.
2. Day 1 has no bird; day 2 has one swoop and the hint.
3. Twenty unattended days, twice: every day ends, no rescues. The end-card day and minute beside yesterday's figures.
4. `Leaf.Recruits 0`: day summaries as before.
5. A save from last night's package loads and plays its next day.
6. Packaged smoke test, with and without ray tracing.
