# Open questions

Facts I could not confirm from the named sources, and choices that belong to the owner.

## Owner-only facts and decisions

1. **Full name and Epic account email** for the submission form. Not in any file.
2. **Video link** and **itch page links** (MegaJam page, Game Off page). None exist yet.
3. **Licence** for the repo. `design.md` and `plan.md` list it as an owner decision. No `LICENSE` or `README.md` exists in the repo yet. The README draft says "TBD by the owner".
4. **Final title.** `design.md` calls "Leafline" a working title and lists "Cutting Shift" and "Canopy Crew" as alternatives. I used "Leafline" everywhere.
5. **Repo visibility.** The repo (https://github.com/InventivetalentDev/Leafline) is private. It may be made public only after a fresh yes from the owner. The Game Off page and the dev-log link depend on this: if it stays private, post the dev log on the itch page and drop the source link (`plan.md` M7).
6. **Dev log link.** The dev log (`docs/devlog.md`) is marked "drafted by Claude in the owner's voice; owner: check every statement". The prototype note in `submission-form.md` is word for word from it and is not yet checked by the owner. The dev log's last entry is 13 Nov (one tree); the plan's later M7 items (Saturday playtest fixes, final package) are not in it.
7. **The "idea that had not found a home" line** on the MegaJam page: `design.md` says to say so "without overstating it". Owner: check the wording.
8. **Game Off "Updates" section.** I listed the "after the MegaJam" candidates from `design.md` as ideas under consideration. Only the owner knows which, if any, will be built. Keep, trim or delete.
9. **Tags and genre** on the itch pages are my suggestion, not from the docs.

## Facts I could not confirm

10. **Sourced assets.** `docs/sourced-assets.md` does not exist (the task and `CLAUDE.md` name it). `design.md` and the audio brief say audio is synthesised and CC0 samples "may be added later". I found no record of any sourced asset, so every page says "none". Please confirm no CC0 sample, font or other file was added. The engine's default font is used (`tech.md`).
11. **Final zip size and name of the executable folder.** The latest recorded zip is 463 MB (`plan.md`, 13 Nov 11:28); an earlier note says 0.43 GB. The zip has everything under a `Leafline/` folder with `AntTreeGame.exe` (`tools.md`, `Build/Windows`). The itch pages say "about 0.46 GB"; update after the final package.
12. **System requirements.** Not documented. `tools.md` says the build was not tested on another PC, players may need the Visual C++ Redistributable 2015-2022 x64, and Windows SmartScreen may warn about the unsigned exe. (`package_win64.bat` passes `-prereqs`, which adds Unreal's prerequisites installer.) Performance was measured only on the owner's NVIDIA GPU at 60 fps and 1920x1080; mid-range GPUs are unchecked. I put no requirements on the pages.
13. **Play length.** The end card is *targeted* at about 15-20 minutes into a fresh save; the model gives 17.1 minutes with recruits and 18.6 by hand (`economy.md`). That is a model, and the owner's play has not confirmed it. The pages do not state a play time.
14. **Difficulty.** `plan.md` (M5b) says that from about day 6 the gusts blow off nearly every laden recruit unless Grip is high, and the owner should decide whether to cap it. I did not mention it on the pages.
15. **Gamepad labels.** `design.md` calls the cut button "X" on the pad, and the code binds the left face button (X on an Xbox pad, Square on a PlayStation pad). Same for A (bottom), Y (top). The pages use Xbox names. PS5/PS4 pads are reported as working (`playtest-notes.md`); the PS5 audio-routing fix is unchecked.
16. **Open playtest items** that a reader might notice (all in `playtest-notes.md`): picking leaves is a bit hard, walking round branches sometimes sticks at the tangent point, the walking animation is hard to see from behind, no key or controller icons, the wind-risk overlay on Tab does not exist yet (`docs/briefs/exposure-overlay.md` exists; I did not check its state). The pages do not claim any of these are fixed.
17. **Features I left out because I could not confirm they ship:** the bird, taking over a returning ant, walking on leaf blades (all marked stretch or Game Off in `design.md` and `plan.md` M6), and any wind-risk overlay.
18. **The Game Off rules** beyond what `design.md` says (public repo, itch page, theme SHIFT, deadline Tue 1 Dec 22:37 CET). I did not check the jam page for required tags or a required licence.
19. **Project plugins.** `AntTreeGame.uproject` enables `ModelContextProtocol`, an experimental engine plugin, and `ProceduralMeshComponent`. I did not check whether `ModelContextProtocol` is needed to build from source or ships with a stock 5.8 install; the README does not mention it. Also: a first build on another machine has not been tested, and the batch scripts contain the author's `C:` and `D:` paths (the README says so).
20. **Credits.** I named Claude and Codex without model versions. `devlog.md` says planning was done with "Fable 5.2" and `plan.md` names Opus for building; the exact models for the final credits are the owner's call. The README line about Epic's branding is my addition: keep or delete.
21. **Cover image and screenshots.** `plan.md` says Codex produces the cover and screenshots are captured for the itch pages. Files exist in `Art/Concept/Codex/2026-11-12/key-art` (covers and banners); I did not look at them. Check each for trademarks or logos (rule §15(d)) and add them to the pages.
22. **Concept art in the repo** (`Art/Concept/Codex`) is AI-made with Codex. The README credits Codex for it; whether the owner wants that art in the public repo is not stated.
23. **Save location and reset.** Saves use one slot named `Leafline` (`tech.md`). I did not confirm the file path in the packaged build, so the pages say only that progress is saved between sessions.
