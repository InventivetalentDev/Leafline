# Open questions

## Docs pass — 15 Nov 2026, evening

The ship docs now describe the game as it ships from `main` on 15 Nov: recruits from day 1 (Brood ladder 2/3/4/6/8/10/12), the bird from day 2 with the bait, the climb curve (leaves higher up are worth more; one-tree milestones 25 / 190 / 335), relief and the seam-blended bark on by default, the quieter wind mix, the UI polish, the build line off and the F3 input view kept.

- `itch-megajam.md` / `.html` and `itch-gameoff.md` / `.html`: the bird "swoops" from day 2 instead of "hunts" (the in-game hint changed the same way for the teen rating, `docs/ui-text.md`); two recruits from day 1, Brood adds more; leaves higher up are worth more; "Ride a trail" as in the game; a Drop row (G / pad B) in the controls table; one `[IF MUSIC SHIPS]` line each. Game Off only: "Three workers" (was "scouts"), the colony moves at its second and third milestones (the first doesn't move it), "sounds and music" cut to "sounds". The `.html` files are regenerated with `Tools/md_to_itch_html.py`.
- `submission-form.md`: our deadline 23:00 (was 22:00); the gameplay instructions gain riding, the climb curve, recruits from day 1 and the bird from day 2; the theme answer quotes the recap's real title, "today's cut", and says "trails" (was "lines"); one `[IF MUSIC SHIPS]` line in the tools answer; the checklist gains `Tools/release_build.sh`, the JPEG screenshots from `Tools/itch_shots.sh` and the 23:00 deadline.
- `README-draft.md`: the "20:00 bird decision" note is gone; the same gameplay changes as the pages, plus the bait; `Tools/release_build.sh` named in the build steps; one `[IF MUSIC SHIPS]` line.
- This file: settled questions are marked **Settled** with the answer; open ones stay open.

## Refresh changelog — 14 Nov 2026

- `README-draft.md`: replaced the old thin-branch controls; added the harvest and walk-home fast-forward, day-end save behaviour, look passes, nest and neck, fuller AI/prototype credits, build prerequisites and one-line Saturday options. Kept title, licence and page/video links open.
- `itch-megajam.md`: refreshed the line and Director's Cut framing, one-tree progression, controls, presentation and fast-forward; expanded the prototype and AI disclosures, credited Xelu, removed the stale download size and marked Saturday options.
- `itch-gameoff.md`: matched current gameplay, controls and credits; kept SHIFT and the public-repo requirement, replaced the stale bird roadmap with conditional copy, and left later updates to the owner.
- `submission-form.md`: refreshed fields and current-build facts; preserved the dev log's prototype note verbatim with its review status, clarified pre-jam tooling, sourced assets and AI use, and added conditional features plus the M7 checklist.
- `open-questions.md`: replaced the old inventory with the decisions below, newest first; separated Saturday feature gates from Sunday owner items and older checks still unresolved.

## Saturday feature gates — 14 Nov, newest first (all settled)

All six gates are settled: every feature below ships, and no `[IF … SHIPS]` marker for them is left in the pages, the form or the README. The only conditional marker left is `[IF MUSIC SHIPS]` (item 10 below).

| Marker | Status on 15 Nov | Action left |
|---|---|---|
| `[IF ANT LOOK SHIPS]` | **Settled: ships.** `M_Ant2` with `Leaf.AntLook` (variant A, copper-rust head and thorax, in the packages since 14 Nov 13:30). | None. |
| `[IF FOREST FLOOR SHIPS]` | **Settled: ships.** `AForestFloor` (`Leaf.Floor`): leaf litter, grass tufts and twigs round a clearing. The pages name the forest floor. | None. |
| `[IF BIRD SHIPS]` | **Settled: ships, on by default (owner, 15 Nov).** With recruits on, the bird first comes on day 2, with one gentle swoop and the hint. | None. The copy keeps "wing gust", "never touches them", and ants that are only knocked off. |
| `[IF BAIT SHIPS]` | **Settled: ships, on by default (owner, 15 Nov).** Drop is G / pad B (`IA_Drop`); the G key icon is in `docs/sourced-assets.md` (one row, 27 icons). | None. |
| `[IF WIND CUES SHIP]` | **Settled: ships.** `AWindCues` (`Leaf.Cues`, on by default): the gust front with its dust wall, the ant's air, grip dust and pose. | None. |
| `[IF VIDEO KIT SHIPS]` | **Settled: ships.** `Leaf.Cinematic` and the shot tools; the final video was filmed with them (`docs/ship/video.md`). | None. |

**Fillets — settled.** Fillets are on by default again (`FTreeParams::bFillets = true`, `bFilletWalk = true`). The laden recruits that stuck at junctions were fixed in `UAntMovementComponent::TickFollow` (488 hauled in twenty AutoPlay days, no rescues; comment in `Tree/TreeTypes.h`). No page mentions fillets.

## Sunday M7 — owner items

Our submission deadline is Sun 15 Nov 23:00 CET at the latest (the MegaJam's own deadline is Mon 16 Nov 02:59; Game Off's is Tue 1 Dec 22:37). Agents prepare; the owner submits personally.

1. **Final title.** Open: no doc records the owner's final decision. `Leafline` is in use everywhere: the key art banner, the video's title and end cards, the in-game title and credits, and both pages. `design.md` still calls it the working title. **Owner:** confirm the title; if renamed, update titles, archive/folder naming and links together. The Unreal project/executable remains `AntTreeGame`.
2. **Licence.** Still open: there is no `LICENSE` file yet. **Owner:** choose the repository licence and have `LICENSE` added before publication; resolve the `[LICENCE]` placeholders in the README and the Game Off page. Xelu's CC0 icons keep their own licence.
3. **Public repo.** Still open. The plan records `https://github.com/InventivetalentDev/Leafline` as private until owner approval. No visibility change was made here. **Owner:** explicitly authorise making it public, then check the source and dev log links while signed out before the Game Off submission. Keeping it private means the Game Off public-repo requirement is unmet; a dev log copied to itch can serve the MegaJam only.
4. **Full name and Epic email.** **Settled:** filled in the form's private team-member field.
5. **Video and page links.** Still open. The video is rendered (`Build/Video/leafline_megajam.mp4`, 46.2 s, 1920x1080, 60 fps; `docs/ship/video.md`). **Owner:** approve it, upload it unlisted, check it plays signed out, and fill `[VIDEO URL]` in the form and both pages. Create the two itch pages and fill the page links. Freeze the MegaJam page and build at submission; date later Game Off updates separately. Pick the cover and screenshots, and check the game, the video and the page art for logos (only the Unreal Engine logo is allowed, per Epic's guidelines).
6. **Final build and download.** Partly settled: every package of 15 Nov zips to 0.44 GB (the pages say so), `bShowBuildLine=False` is set, and the F3 input view stays (`bInputVizKey=True`, owner's choice; the pages' control tables leave it out because it is a testing aid). **Owner:** package the submission build from the final commit with `Tools/release_build.sh` (zip, frame rates, twenty days from a fresh folder with spaces), and after uploading, check the downloaded zip with `Tools/release_build.sh --check <zip>`. If the zip size changes, update it on both pages.
7. **Dev log and disclosure review.** Still open. The dev log asks the owner to check every statement, especially the earlier prototype, and its last entry is still Friday's look pass (13 Nov); Saturday and Sunday are not recorded. **Owner:** confirm the prototype note and the AI and tool credits, then have the main session finish the log before linking it. Keep the facts that the planning agent read the prototype's notes, commit list and file listing, not its source, and that no game files were reused.
8. **Final sourced-assets review.** **Settled:** `docs/sourced-assets.md` has one row, Xelu's CC0 prompts (27 icons: 16 keyboard and mouse keys, 11 Xbox buttons, the G key included). The pages credit the pack without a count.
9. **Page choices and later updates.** Partly settled: the theme colours are chosen ("Evening bark"), and the screenshots come from `Tools/itch_shots.sh` (upload the JPEG copies in its `jpg/` folder: itch takes at most 3 MB per screenshot). **Owner:** select tags, decide which later work to announce, and resolve or delete all placeholders and editorial comments before pasting. Tag the submitted commit `megajam-submission` after submitting.
10. **Music (new, 15 Nov).** Open: the switch exists (`FWindTuning::bMusic`, off by default) and the owner is choosing one of three options rendered by `Tools/audio/make_music.py`. The pages, the form and the README each carry one `[IF MUSIC SHIPS: …]` line where the music text goes. **Owner:** if music ships, write that one line in each place (both `.md` pages, then regenerate the `.html` with `Tools/md_to_itch_html.py`, or edit both); if not, delete the lines. The video has no music either way.
11. **Friend A (new, 15 Nov).** Open: the in-game credits comment out "Playtesting and design feedback" until Friend A's feedback arrives. **Owner:** when it arrives, fill `[FRIEND A]` in both pages and the README, and uncomment the two lines in `Content/Leafline/UIRaw/credits.txt` (main session) before the build; if it doesn't, delete the line from the pages and the README so they match the game.

## Older checks — 13 Nov, then 12 Nov

12. **13 Nov: final feel and visual verdict.** Open. Context: the owner settled many single features on 15 Nov (the bird and bait, relief, the seam-blended bark, recruits from day 1, the climb curve, the wind mix, the UI polish), but no verdict on the whole package is recorded. **Owner:** accept the final package or give a short veto list.
13. **13 Nov: current pacing and difficulty.** Still modelled, not confirmed by a full run of the owner's. With the climb curve and 25 / 190 / 335, the model reaches the end card on day 6 at 19.4 min (inside the 15–20 minute goal); a twenty-day AutoPlay run reaches it after day 8 (`docs/economy.md`, variant B). **Owner:** judge the pacing and the bird's cost. The pages promise no play length or difficulty.
14. **12 Nov: PS5 audio routing.** Still open. PS4/PS5 input was checked with the owner's DualSense, but `au.DisableDeviceSwap=1` still needs the plug-in audio test. Xbox glyphs on every pad are intentional. **Owner:** test connecting the pad during play; if audio still disappears, choose a fix or a documented workaround.
15. **12 Nov: supported hardware and source build.** Still open. Performance checks cover the owner's GPU, including packaged runs with and without ray tracing and DX11; another PC, minimum requirements and a clean clone build are unverified (the README says so). **Owner:** decide whether a second-PC or clean-clone check fits before submission, and publish only requirements that were actually verified. The drafts list the development setup, not a tested minimum spec.

Sources checked for the 15 Nov pass: `CLAUDE.md`; `docs/design.md`, `docs/ui-text.md`, `docs/economy.md`, `docs/plan.md`, `docs/tech.md`, `docs/tools.md`, `docs/sourced-assets.md`, `docs/devlog.md`; `Content/Leafline/UIRaw/credits.txt`; `Config/DefaultGame.ini`; the input bindings (`LeaflinePlayerController.cpp`), the HUD strings (`LeaflineHUD.cpp`), the bird schedule (`BirdSystem.h`), the fillet defaults (`TreeTypes.h`), the relief default (`GroundRelief.cpp`) and the music switch (`WindSystem.h`); `git log` to `1d8d468`.
