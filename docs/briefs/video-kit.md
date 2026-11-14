# Brief: video kit (Saturday)

For a parallel agent (Codex). Written Sat 14 Nov by the main session. The jam judges mostly see a 30 to 60 second video, recorded by the owner on Sunday (screen capture of the packaged game at 1080p or higher). This kit makes that video look good. Freeze 23:00 Saturday.

## What to build

1. **Cinematic mode** `Leaf.Cinematic 0|1`: hides the whole HUD (panels, prompts, banners, hints, the faint build line in the corner, on-screen debug messages) and the mouse cursor; the camera follows more smoothly (heavier damping of look input and of the follow spring), so a recording has no jerks. Game play continues normally; the player can still steer the ant.
2. **Scripted shots** `Leaf.Shot <name> [seconds]`: the camera leaves the player's control and runs a move, then hands back exactly where the player's camera was. Moves relative to the controlled ant and the tree:
   - `orbit`: one slow turn round the ant at about the normal boom, slightly from above;
   - `crane`: from behind the ant on the trunk, rising and tilting up until the crown fills the frame;
   - `dolly`: gliding along the branch the ant is on, toward the ant, low over the bark;
   - `reveal`: from the nest entrance pulling back and up until the whole tree stands in frame;
   - `follow`: a slow chase shot behind and slightly above the ant while it walks or rides a trail.
   Paths must stay out of wood (the camera already has field tests: `AAntCamera`, the tree's `FTreeField`); smooth start and stop (ease in and out); a field of view setting per shot.
3. `Leaf.ShotList <name> <name> ...` plays several shots back to back; `Leaf.Slomo <factor>` sets the time dilation (1 = normal) for dramatic moments.
4. Everything works in the packaged Development build from the console (the owner types it there) and must not touch saves or the economy. With none of it used, the game is exactly today's game.

## Where

- Ant/AntCamera.* (a mode where something else drives the camera), UI/LeaflineHUD.* (a hide-all flag), Game/LeaflinePlayerController.* if input needs it, and new files (for example Game/LeaflineShots.*). Console commands follow the `Leaf.` pattern and go in the table in `docs/tech.md`.
- Other agents work at the same time on the bird (World/, Colony/LineSystem.*, the HUD's wind group) and on wind cues (World/WindSystem.*, Ant/AntPawn.*). Keep your HUD change to the hide-all flag.

## Acceptance

- Each shot from the default seed (517192) on day 1 and on a later day: no frame inside wood, smooth start and end, the camera returns to the player's view.
- Cinematic mode: a screenshot with nothing but the game world.
- `Tools/perf_packaged.sh` unchanged.
