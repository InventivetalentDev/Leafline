# Night code review, 14 Nov 2026

Codex (read-only) reviewed `git diff 763b3db..999e49c -- Source/` (54 files, about 6,800 lines: the bird, in-world wind cues and the trail fade, walking on fillets, the floor's far look and relief, HUD icons, the UI text rewrite, the Controls and Credits pages). A static review; nothing was built or run by the reviewer.

**Serious findings: none.** Bird targets and callbacks traced across day changes, tree moves and destroyed ants; array access, geometry maths, shipped defaults and save loading checked. The save format is unchanged and leaf generation still uses the field without fillets, so earlier saves keep their leaf indices.

## Medium and low findings

| # | Finding | Status |
|---|---|---|
| 1 | Medium. Bait dropped mid-dive: target and approach change but the bird's visual curve kept its start, controls and elapsed time, so the bird could jump (`World/BirdSystem.cpp` ~722). | Fixed (f10a481, cfca2f0, 189ba70): the turn starts from the curve's own position and velocity; drops in the last `CommitSeconds` (1.0 s) leave the bird committed to its pass, and the piece stays as bait. Checked frame by frame by bd-core; deterministic. |
| 2 | Medium. Sending the targeted scout home during a dive left the camera nudge aiming the next scout at the old attacker (`BirdSystem.cpp` ~934). | Fixed (f10a481). |
| 3 | Low. With `Leaf.FilletWalk 0`, dropped bait sits at the simulation position, under the lifted skirt (`BirdSystem.cpp` ~708). | Open (non-default path). |
| 4 | Low. Toggling `Leaf.Relief` with a ground fillet on rebuilds the ground without its skirt (`Tree/TreeActor.cpp` ~552). | Open (both off by default). |
| 5 | Low. Loading yesterday's trails built every chunk twice (`Colony/TrailNetwork.cpp` ~170). | Fixed (f10a481). |
| 6 | Low. Litter clusters can overshoot their sector's count by up to 25 leaves (`World/ForestFloor.cpp` ~433). | Open (harmless; fixing it would change the litter layout). |
| 7 | Low. The cut warning said a gust "will blow it away" although the check assumes full exposure without gripping (`UI/LeaflineHUD.cpp` ~867). | Fixed (f10a481): "Too big: gusts blow it off exposed bark". |

All fixes merged in 179dcdc; auto20 unchanged (0 rescues, 487.78 hauled, end card after day 16).
