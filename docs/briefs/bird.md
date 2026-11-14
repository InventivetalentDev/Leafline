# Brief: the bird (Saturday's one new feature)

For the main session. Written Sat 14 Nov, 09:30. The owner wants one new thing before the 23:00 freeze; this is M6 item 1 in a small form. The owner's verdict on the line is still formally open; they have kept playing and extending it, and this brief builds on it.

**It ships only on the owner's go.** They play it in the afternoon and decide at about 20:00. Everything is behind one switch, and with the switch off the game must be exactly today's game.

This brief is the specification. When the work lands, update `design.md` (the cut list says "stretch"), `tech.md`, `plan.md` (M6) and `devlog.md` yourself.

## The idea in three rules

A bird hunts on the wind. The player learns three things, all of which reuse what they already know:

1. **It only sees ants on exposed bark.** The wind map (hold Tab) already shows where that is: red is in the bird's view, green is hidden.
2. **Gripping holds.** An ant that grips when the bird dives keeps its piece.
3. **It takes the easiest piece** (tier 2). A piece dropped in the open draws the bird away from the line for a while.

The bird never touches an ant. Its wing gust knocks a laden ant off the bark; the piece flutters away and the ant falls and walks home, exactly as in a gust. Nothing is caught, eaten or hurt, and nothing happens to the bird (MegaJam rules §8).

## Before you start

- **Baseline first.** Fillets changed the walking field at 22:24 last night and only a two-day smoke test has run since. Run `Leaf.AutoPlay` for twenty days with recruits and fillets on and the bird not yet built. If recruits stick at junctions, fix that or ship `Leaf.Fillets 0`; do not start the bird on a broken baseline. Keep the loss figures from this run: they are the "bird off" comparison below.
- Tag the current commit `pre-bird`. Commits start `Bird:`.
- The sim does not wait for the model or the sounds. Start with a placeholder shape (a dark stretched sphere) and existing sounds.

## Tier 1: the swoop

**The clock.** The bird's schedule, its choice of target and the hit all run in `ALineSystem`'s fixed steps, seeded from the day, like the wind clock. The visible bird only follows that schedule. Codex found real-time clocks in this code twice; do not add a third.

**The clock has its own random stream.** The bird must not draw from the wind's, so the gusts of a day are the same with the bird on and off. Check that the fixed steps run with `Leaf.Recruits 0` too; if they do not, the bird needs them to.

**When it is active.** Only in `Shift`. It is not active in the harvest, the walk-home fast-forward, the recap or the nest. When the shift ends, a dive in progress is cancelled and the bird flies off.

**Early days.** A day without recruits has only the three scouts' pieces, and that haul pays for the first Brood level. So the bird first appears on the first day the recruit pool is above zero (with `Leaf.Recruits 0`: on day `FirstBirdDay`, start with 3), and that first day has one swoop. Whether a new colony starts with recruits is an open question with the owner; this rule works either way.

**Cycle.** `Circling` → `Diving` → `Away` → `Circling`.

- **Circling:** the bird glides in a wide circle on the upwind side of the tree, at the height of the ant the camera follows, far enough out to be seen against the sky. After `SwoopInterval` seconds (seeded jitter) it looks for a target.
- **Target:** among ants that carry a piece and are in a state that a gust can blow off (see `AAntPawn`'s wind check), take the one with the highest exposure. Use `AWindSystem::GetExposure` itself, and the HUD's "exposed" band as the threshold (move the band values somewhere both can read them; do not copy the numbers). Scouts and recruits count alike. No target: keep circling and look again a few seconds later. Ants without a piece are never targets.
- **Diving:** `WarnSeconds` (start with 2.5) from the call to the pass. The bird calls, turns and comes in from upwind, following the target as it moves. Exposure already means "open toward the wind", so that approach is clear of wood.
- **The pass:** the most exposed bark faces the wind, so the bird flies straight at the surface its target is on. It pulls up `PassHeight` (60 to 100 cm) short of the bark and leaves upward or along the surface, whichever the surface normal leaves open. It never continues downwind through the branch. At the sim step of the pass, test the target again where it now stands:
  - exposure below the threshold: a miss; the bird breaks off;
  - gripping: holds;
  - otherwise: `BlowOff`. The bird arrives along the wind, so the existing fall and the fluttering piece already point the right way; pass only a cause, for the log and the stat.
  The same test applies to other laden ants within `HitRadius` (start with 120 cm), up to `MaxHitsPerSwoop` (2).
- **Away:** out of sight for `AwaySeconds`, then circling again. At most `MaxSwoopsPerDay` (start with 3; one on the first bird day).

**No new ant state.** The hit goes through the existing `AAntPawn::BlowOff`, so `OnRecruitBlownOff`, `OnBlownOff`, the roster, the rescue and `UpdateDayFlow` / `HasWork` stay as they are. Give the log line a cause ("knocked off by the bird"), and count bird losses separately in the day's stats so the recap can say "Pieces lost to the bird: N".

**Telling the player.** In the world first (the owner's open playtest note):

- the bird itself, large enough to read at circling distance (`BirdScale` is a tuning value) and casting a shadow;
- a call at the start of the dive, placed in the world at the bird; a wing rush at the pass;
- on the HUD, one line in the wind group: "Bird circling" and "Bird diving!", in the plain text colour (green, amber and red there mean exposure), nothing when it is away. No new panel;
- on the first bird day, a hint in the existing hint style: "A bird hunts on the wind. Stay off the red, or grip when it dives." The grip prompt shows during a dive at the player's ant.

**Nothing is saved.** No save field and no version change. The bird's day is seeded from the tree seed and the day.

## Tier 2: bait (its own switch, first to cut)

- **Drop.** A new action drops the carried piece where the ant stands; it lies on the bark. Pad: B (`Pad_B` is already in `UIRaw`). Keyboard: a key that has no icon yet; take it from the same Xelu CC0 pack and add it to `docs/sourced-assets.md`. Show the prompt only while a bird is circling or diving and the ant carries a piece.
- **After dropping,** the scout is as it was before its cut: it can cut again and send that piece home. Dropping makes no line.
- **The bird prefers bait.** A lying piece on exposed bark beats every ant as a target. The bird dives at it, takes it, and stays away for `BaitAwaySeconds` (start with 45). A piece dropped on sheltered bark is not seen and lies there until the day ends.
- **The hint gains a clause:** "…or drop your piece in the open to draw it away."
- Bait swoops count toward `MaxSwoopsPerDay`.

## The model and the sounds (parallel agents)

**Bird mesh** (a Claude agent with blender-mcp, on the pattern of `docs/briefs/ant-mesh.md`):

- One static mesh from a Python script, `Art/Blender/bird.py`. No rig, no textures, under 3,000 triangles.
- A plain small songbird, not a recognisable species: body, head, short beak, fanned tail, two wings modelled spread flat in a glide.
- Data travels in UV channels, not vertex colours (the nest import lost its colours):
  - UV0: U = 0 body, 0.25 left wing, 0.75 right wing; V = distance from shoulder (0) to wing tip (1), 0 on the body.
  - UV1: U = part id for colour (body, head, beak, wing, tail, eye).
- The main session imports it under new names (`SM_Bird`, `M_Bird`). `M_Bird` bends each wing about its shoulder by a flap phase and a flap amount from C++ (0 gliding, 1 flapping).

**Sounds** (a Claude agent, on the pattern of `docs/briefs/audio.md`, checked with earshot):

- `bird_call`: two or three short falling notes, under a second; it must cut through the wind loop.
- `bird_pass`: a wing rush of about half a second.

## Switches and debug commands

Register all of these before a full build, and add them to the table in `tech.md`.

| | |
|---|---|
| `FBirdTuning` | Every number above, in one struct, with `Leaf.Tune bird` |
| `Leaf.Bird 0\|1` | The whole feature. Off: no bird, no schedule, no HUD line, no hint, no drop action |
| `Leaf.Bait 0\|1` | Tier 2 only |
| `Leaf.BirdNow` | Start a dive now (tests, and the owner's video) |

Default on in today's test packages. The default in the submission build is the owner's decision at about 20:00.

## The economy

No milestone retune today. The bird's cost is bounded by `MaxSwoopsPerDay` × `MaxHitsPerSwoop`. Compare twenty `Leaf.AutoPlay` days with the bird on against the baseline: if the end-card day moves, lower the swoop count until it does not.

## Time boxes

| When | What |
|---|---|
| By 14:00 | Tier 1 with the placeholder, tests 1 to 5, a package for the owner |
| By 17:00 | Model and sounds in; tier 2 if tier 1 is clean; second package |
| 20:00 | Owner's go or no-go, tier by tier. Last candidate package. |
| 23:00 | Freeze |

Cut order, first to go first: bait, the recap line, the model's wing flap (a gliding bird is enough).

## Acceptance tests

1. Baseline: twenty unattended days with fillets on, before any bird code, run twice. Every day ends. If the two runs differ (the scripted scouts cut random sizes), that difference is the spread for tests 2 and 3 and for the economy comparison.
2. Bird off: the same run after the bird code is in gives the baseline's day summaries, within that spread.
3. Bird on: twenty unattended days, twice. Every day ends; no recruit needs the rescue because of a bird hit. Once more with `Leaf.Recruits 0`: the bird still appears and every day ends.
4. From the log, with `Leaf.BirdNow`: a laden ant on exposed bark is knocked off; the same ant gripping holds; the same ant on sheltered bark is never chosen; an ant that reaches shelter during the dive is missed; an ant without a piece is never chosen.
5. A dive in progress when the shift ends is cancelled, and nothing is hit in the harvest or the fast-forward. Watched and skipped harvests give the same result.
6. Bait: a piece dropped on exposed bark is taken on the next dive and the bird stays away; a piece dropped in shelter is ignored; the scout cuts again and its second piece makes a line.
7. Stills of the dive from the player's camera, one frame before the pass and one after: the bird does not touch the ant and does not pass through wood.
8. Prompts and the HUD line at 1280×720 and 3840×2160, with keyboard and with pad icons.
9. Frame times unchanged within noise. Packaged smoke test with and without ray tracing, with the bird on and off.

## Not in this work

- Perching, a nest for the bird, more than one bird, other predators.
- Food as a second resource, or anything to pick up.
- Any contact between the bird and an ant, in any frame.
- Changes to milestones, upgrade costs or the save.
