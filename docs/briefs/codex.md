# Briefs for Codex

Three kinds of job. Each is self-contained: paste the section, plus the files it names, into a Codex task. Codex never edits `.uasset` or `.umap` files.

Shared rules for every job:
- This is a jam entry under a clean-room rule: all content is created on or after 2026-11-12. Do not read or reuse anything from other projects on this machine, including `D:\UnrealProjects\TreeAntGame`.
- Read `docs/design.md` first. Read `docs/tech.md` for code jobs.

## 1. Art

Existing work: `Art/Concept/Codex/2026-11-12/initial/` holds five concept images and `generation-prompts.json`, which records the shared art direction. Reuse that art-direction paragraph so new images match. Save new work under `Art/Concept/Codex/<date>/<set-name>/` with a `generation-prompts.json` beside it.

Corrections to the earlier brief:
- The ant is a **leafcutter**. It carries its fragment upright above its head like a sail, not out in front.
- Drop the rocket launcher entirely.
- The scent trail stays a restrained translucent cyan lying on the bark.

Wanted, in priority order:

| Set | Images | Purpose |
|---|---|---|
| `key-art` | A cover at 630x500 and a wide banner at about 1920x620: a tiny leafcutter with an oversized leaf fragment on vast bark, a cyan trail curving away behind it, more workers small in the distance on the same trail. Leave calm space for a title; do not draw any text. | itch pages for both jams |
| `cut-and-wind` | The cut itself: an ant at a leaf's edge, a half-finished curved cut. Then the same ant, loaded, leaning into a gust on an exposed branch with streaks of wind and bending leaves, a sheltered route visible on the lee side. | Reference for the leaf shader, the wind effects, and the HUD's risk colours |
| `bark-and-leaf-study` | Close studies at ant scale: bark plates and fissures with moss; a backlit leaf with veins and a bitten edge. Flat, evenly lit, no characters. | Reference for the procedural materials |
| `screens` | Mood for the title screen, the end-of-day recap ("today's cut") and the upgrade list at the nest entrance. Layout and colour only. | Reference for UI built in code |
| `bird` | Only if the stretch goal is started: an ordinary songbird from an ant's viewpoint, perched and alert. | Reference for a scripted model |

These images are reference and store-page art. They are never used as in-game textures. No logos, trademarks or text in any image.

## 2. Code review

Run after each milestone in `docs/plan.md`. Review the diff since the previous milestone tag.

Look for, in this order:
1. Correctness: crashes, null or stale object pointers, use of actors after destruction, out-of-range indexing, divide by zero or normalising a zero vector in the field and movement maths.
2. Places where the surface walker can stall, oscillate or leave the surface: junctions, twig tips, the ground boundary, very small radii, large frame times.
3. State machines (`DayDirector`, ant states, wind phases): unreachable states, missing transitions, anything that can leave the day unable to end.
4. Anything that works in the editor but not in a packaged build: editor-only modules or headers, asset references by path that will not be cooked, objects created without an owner.
5. Frame-time traps: per-frame allocation, rebuilding mesh sections every tick, field queries inside loops over all leaves.

Report findings as a list with file, line, what goes wrong and a concrete trigger. Do not report style. Do not fix anything unless asked.

## 3. Isolated C++ on a branch

Candidate once milestone M2 has landed and the interfaces exist: **the save game and upgrade system** (`Source/AntTreeGame/Game/`).

- `ULeaflineSaveGame`: leaf mass, total hauled, five upgrade levels, day number, base seed, a version field. One slot.
- `ULeaflineGameInstance`: loads on init, saves after each day and each purchase, exposes `GetUpgradeLevel`, `CanAfford`, `Buy`, `AddHaul`, and a multicast event when anything changes.
- An upgrade table in one place: name, description, cost per level (6 / 14 / 24), effect per level.
- Console commands `Leaf.Give <mass>`, `Leaf.Buy <index>`, `Leaf.ResetSave`.

Work on a branch named `codex/save-upgrades` in a **separate git worktree or clone**, never by switching branches in the main working tree. Touch only files under `Source/AntTreeGame/Game/` unless the task says otherwise. The main session builds, tests and merges.
