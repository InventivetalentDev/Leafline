# Codex review: M2 and M3 code (2026-11-12)

Codex reviewed `m1..19de8c2` (M2, the merged save game and upgrades, M3) read-only, with static inspection and numerical or state-machine reproductions. Its sandbox could not write files, so the findings were copied here by the main session, condensed, with what was done.

| # | Severity | Finding | Resolution |
|---|---|---|---|
| 1 | Medium | `DayDirector::PossessNext` only considered waiting and returning workers, so a worker left standing after Tab was never picked again; the day could stall with nobody possessed. | `PossessNext` now returns to an idle worker that is still out before following returning ones. |
| 2 | Medium | An expired `PendingPossess` (after a blow-off) could fire after control had already moved on, consuming another worker. | The delay remembers the blown-off ant and only hands control on if the player is still watching it (or nothing). |
| 3 | Medium | Trail following committed failed projections. | A failed projection keeps the last good position and normal, as walking does. |
| 4 | Medium | The camera's obstruction-thickness walk exited at once because `March` reports contact within 0.5 cm of the probe, so thick occluders never pulled the boom in. | The walk continues while touching or inside (gap under 1 cm) and steps at least 2 cm. |
| 5 | Medium | A bite reaching past the midrib could keep edge points inside the removed circle. | If the bite's inner boundary crosses the midrib, the edge collapses to the midrib. |
| 6 | Medium | The leaf size cap ignored the fixed part of a cut's cost, allowing oversized last cuts. | The cap solves `UsePerCut * (0.35 + 0.65 s) <= Remaining`; leaves stop being cuttable below 0.16 remaining. |
| 7 | Medium | Trail ribbons rebuilt the whole growing path five times a second. | Paths are drawn in chunks of 200 points, each its own mesh section; only the tail chunk is rebuilt. |
| 8 | Low | Holding F while moving restarted riding every frame (full path copy and a log line each time). | Riding only starts without movement input. |
| 9 | Low | The recap's trail total was only refreshed on nest arrivals. | It is refreshed when the day ends. |
