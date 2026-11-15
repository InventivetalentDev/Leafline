# tools

## strip_image_metadata.py

`Tools/strip_image_metadata.py` removes metadata (GPS location, camera model, timestamps, XMP, C2PA credentials and so on) from reference images. Run it on new photos before you commit or share them.

```
python3 Tools/strip_image_metadata.py            # everything under Art/Reference (subfolders too)
python3 Tools/strip_image_metadata.py PATH ...   # specific files or folders
python3 Tools/strip_image_metadata.py --check    # list what would be removed, change nothing
py Tools\strip_image_metadata.py                 # from Windows
```

- It needs only Python 3, with no extra libraries.
- It handles JPEG, PNG and WebP.
- It reports HEIC, TIFF, AVIF and GIF files as "not stripped". Convert those first, or turn off HEIC in the phone's camera settings.
- It doesn't re-compress anything, so the pixels stay identical. It keeps the colour profile, plus the rotation setting if a photo has one, so the image still displays correctly.
- On Pixel photos it also removes the Ultra HDR gain map, so those photos show as normal SDR images afterwards.
- It doesn't touch files that are already clean, so it's safe to re-run.
- It doesn't rename files. Pixel filenames like `PXL_20261112_081150350.jpg` still show the date and time each photo was taken.
- Exit codes: 0 = OK, 1 = `--check` found metadata, 2 = error or unsupported file.

## line_test.sh

`Tools/line_test.sh <scenario>` runs one unattended day (or twenty) of the line from the editor binaries, in its own save slot (`-LeafSlot=LineTest`), and prints the day summaries from the log. The editor must be closed (runs go through the Unreal lock). Scenarios: `smoke`, `watch` / `skip` and `windwatch` / `windskip` (a watched versus a skipped harvest of the same day; use `DAYN=2` for losses), `lines1` / `lines3`, `exposed` / `sheltered` / `small`, `dry` (a patch runs dry while scouts wait), `shots` (HUD screenshots into `Saved/RefShots/line-shots-*.png`), `perfbase` / `perfpool` / `perfharvest` (frame times with the CSV profiler), `migrate` (loads `Saved/SaveGames/LineTestV1.sav`), `auto` (twenty days with `Leaf.AutoPlay`). Environment: `SEED`, `POOL`, `DAYN`. Logs: `Saved/Logs/LineTest-<scenario>.log`.

Since one tree per stage (13 Nov) is on by default, the recorded line numbers (seed 517192, the old tree) need `Leaf.OneTree 0` before `Leaf.StartDay`; without it the same commands run on that seed's sparse one-tree version.

## tree_test.sh

`Tools/tree_test.sh <scenario>` runs the one-tree acceptance tests (`docs/briefs/one-tree.md`) from the editor binaries in the save slot `TreeTest` (the editor must be closed). Scenarios: `flagoff` (one tree off, days 1 and 2, compare leaf counts with `pre-one-tree`), `fresh` (tree report and patch survey of a fresh first tree), `play2` then `relaunch` (two days, then load the slot), `midquit` then `relaunch` (cut mid-day, quit, reload: the day restarts with nothing from it), `strip` (`Leaf.Strip`, `Leaf.Move`), `moves` (seven days with 100 haul given each morning: both moves and the end card), `auto20` (twenty days, save reloaded and tree regrown every morning, tree report each morning), `recoff`, `offauto`, `offrecoff`, `combo` (with `OT=0|1 RC=0|1`: each flag combination to the end card), `v2` (a copy of a real version-2 save, `Saved/SaveGames/TreeTestV2.sav.orig`), `shots`, `custom` (the commands in `$TCMDS`). Logs: `Saved/Logs/TreeTest-<scenario>.log`. `Tools/tree_bands_measured.txt` holds the measured supply per stage that `Tools/econ_sim.py` reads.

## cues_shots.sh

`Tools/cues_shots.sh <exposed|sheltered|both|grip> [label]` takes screenshots of the in-world wind cues (`docs/briefs/in-world-cues.md`) from this tree's editor binaries in the save slot `CuesTest` (the editor must be closed; runs go through the Unreal lock). One forced gust per pass, every 14 s; each pass is a spot and `Leaf.Cues` 0 or 1, shot at fixed times after the gust starts (warning 3 s, rising 1.2 s). Spots: `exposed` / `sheltered` (`Leaf.FaceWind`), `grip` (windward, `Leaf.Hold grip` as the gust rises), `limb` (`LIMB`, default `Leaf.Teleport 21 0.4`), `leaf` / `leafgrip` (`Leaf.GoLeaf`, the usual play view). The spot is placed and aimed only when it changes, so the off and on passes of one spot see the same bark. Environment: `PASSES` (`spot:cues[:Field=Value/...]`, the fields set with `Leaf.Tune wind` before that pass, `AIM=<deg>` aims that pass), `AT` (seconds after the gust; negative values shoot the calm before it; keep shots at least 0.25 s apart or the engine drops one), `AIM` / `PITCH` (camera relative to the wind, default 90 / -12), `TUNE` (commands at the start), `OFFSET` (start every pass later, e.g. to reach shift 3 first), `SEED`, `ROOT`. Output: `Saved/RefShots/cues-<label>-<spot>-<off|on>[-p<pass>]-<seconds>.png` and the cue lines of the log (`Saved/Logs/CuesTest-<mode>.log`, the lock's own output in `.lock.log`).

## md_to_itch_html.py

`python3 Tools/md_to_itch_html.py docs/ship/itch-megajam.md > docs/ship/itch-megajam.html` turns an itch page draft into basic HTML for the itch.io description editor, which takes simple HTML but no Markdown: headings, paragraphs, bold, links, lists and inline code. The header lines before the first `---` (title, tags, download, video) and HTML comments are left out, since they go into itch's own fields; tables become bullet lists. Regenerate the `.html` after editing the `.md`.

## econ_sim.py

`python3 Tools/econ_sim.py [--check-cpp]` (Codex) models a fresh save to the end card with the line on and off, and prints a per-day table and the recommended upgrade costs and milestones; `--check-cpp` checks `Game/Upgrades.cpp` against them. Assumptions are named constants at the top; details in `docs/economy.md`.

## zip_build.py

`Tools/zip_build.py` zips the packaged build (`Build/Windows`) for sharing, into `Build/inventivetalent_Leafline.zip` with everything under one `Leafline/` folder. Run it after `Scripts/package_win64.bat`.

```
python3 Tools/zip_build.py                     # default paths
python3 Tools/zip_build.py --out PATH.zip      # another name or place
python3 Tools/zip_build.py --keep-pdb          # include debug symbols (about 360 MB more)
py Tools\zip_build.py                          # from Windows
```

- It leaves out `AntTreeGame/Saved/` and `Engine/Saved/` (logs with local paths, and the save game and config from test runs), `*.pdb` debug symbols and the packager's `Manifest_*.txt` files.
- It needs only Python 3 and takes about 40 s. The zip is about 0.43 GB (2026-11-12).
- It writes to a `.part` file first, so an interrupted run never leaves a broken zip under the real name.
- Exit codes: 0 = OK, 1 = the zip is 1 GB or larger (over the MegaJam limit), 2 = no packaged build found.
- The build has not been tested on another PC yet. Players may need the Visual C++ Redistributable 2015–2022 x64, and Windows SmartScreen may warn about the unsigned exe.

## perf_packaged.sh and csv_fps.py

`Tools/perf_packaged.sh` measures the packaged build at 1920x1080 in five runs:

- on the trunk with hardware ray tracing;
- in the canopy with hardware ray tracing;
- both again with `-noraytracing`, which makes the D3D12 adapter report no ray tracing, like a GPU without it;
- a short DX11 run that checks the game starts at all on DX11.

Each run walks the ant with `Leaf.AutoMove` and a wandering camera, captures frames with the CSV profiler (`CsvProfile FRAMES=N` and `EXITONCOMPLETION`), then quits by itself. `Tools/csv_fps.py` turns each capture into average, median and 1%-low fps, skipping the first 8 s.

```
Tools/perf_packaged.sh                 # from WSL, editor closed, nobody playing the packaged build
FRAMES=3600 Tools/perf_packaged.sh     # longer captures
python3 Tools/csv_fps.py FILE.csv      # summarise one capture
```

- Runs go through `Scripts/unreal-lock.sh`, so the editor must be closed.
- The packaged game's `Saved/SaveGames` and `Saved/Config` are backed up to `Build/perf-backup-*` before the runs and restored after, so a playthrough in that build is not disturbed.
- Captures land in `Build/Windows/AntTreeGame/Saved/Profiling/CSV/`; `Tools/zip_build.py` leaves them out with the rest of `Saved`.

## unreal-lock.sh

`Scripts/unreal-lock.sh` runs one command while holding one of this PC's Unreal lock slots, and always releases it, also when the command fails or is stopped. At most two Unreal processes (build, cook, standalone game, editor) run on this machine at a time (`UNREAL_LOCK_SLOTS`, default 2 since 14 Nov at the owner's say; it was 1); an editor takes about 12 GB idle and 20 GB in Play, and three at once crashed with "The paging file is too small". Slot 1 is shared with other Unreal projects on this machine, whose copies of the script take only slot 1.

```
Scripts/unreal-lock.sh <holder> -- <command...>   # wait for the lock, run, release
Scripts/unreal-lock.sh --status                   # free, held (by whom, how long) or stale
Scripts/unreal-lock.sh --no-wait|--timeout <min>|--break-stale ...
```

- Slot 1 is the directory `/mnt/d/UnrealProjects/wt/locks/unreal`, slot 2 `.../unreal-2`; each one's `owner` file says who holds it. `--status` lists every slot (exit 0 when one is free). `FOREST_LOCK_DIR` overrides the location (the variable names come from the project the script was written for).
- Exit code: the command's own; 75 when held and not waiting; 76 when stale; `--status` gives 0 free, 1 held, 2 stale.
- Never take or release the lock by hand. A lock taken by hand is waited on, never broken.
- For the editor, the wrapped command must keep running until the editor exits, or the lock is released while it still runs. `cmd.exe /c start ""` should block that long, but that is untested with the editor.
- `Scripts/unreal-lock-test.sh` tests the script.

## RunGame.bat

`Scripts/RunGame.bat <logname> "<execcmds>"` starts the game standalone from the editor binaries, without packaging. Run it through `unreal-lock.sh`.

## Reference screenshots (`Tools/ref_shots.sh`)

`Tools/ref_shots.sh <label>` takes five fixed views on seed 517192 (day 1, morning shift) from the editor binaries, without packaging. Each view runs with hardware ray tracing and with `-noraytracing`. Files land in `Saved/RefShots/<label>-<view>-<rt|nort>.png`. The views:

- `trunk`: up the trunk into the crown, framed like concept image 01.
- `canopy`: on a twig in the canopy.
- `base`: from the nest at the trunk base.
- `junction`: limb 21 where it leaves the trunk, looking down at −80°. This was the black wedge of the look pass.
- `sunward`: the trunk's lee side, looking along the bark toward the low sun. This was the amber wash of the owner's shot 1.
- `bough` (sweeps only, not in the default set): bough 55, 30 cm radius.

Every run uses the save slot `RefShot` (`-LeafSlot=RefShot`), never the owner's colony. The script stops with exit code 3 if `Saved/SaveGames/Leafline.sav` changes while it runs. The editor must be closed: runs go through the Unreal lock. About 30 s per shot.

Options (environment variables):

- `ONLY="junction sunward"` takes those views only. `NO_NORT=1` skips the runs without ray tracing.
- `LOOK="Leaf.Bark 0,Leaf.Tune look SkyLightIntensity 3"` runs any console commands before the view is placed. Write colours with semicolons, e.g. `(R=0.75;G=0.85;B=1.15;A=1)`.
- `PITCHES="-80 -45 -10 25 60"` sweeps the camera pitch at each view. Files are named `<label>-<view>-p<pitch>-<mode>.png`.
- `SHIFT=2` takes the views at noon by sending the first worker home with a fragment first. `SHIFT=3` (evening) is in the script but unverified.
- `TRUNK_T`, `TRUNK_PITCH`, `CANOPY_BRANCH`, `CANOPY_HEADING`, `BASE_HEADING`, `BASE_PITCH`, `JUNCTION_BRANCH`, `JUNCTION_T`, `JUNCTION_HEADING`, `JUNCTION_PITCH`, `SUN_HEADING`, `SUN_PITCH`, `BOUGH_BRANCH`, `BOUGH_T`, `BOUGH_HEADING` and `BOUGH_PITCH` move the views.

`Tools/ref_shots.sh --probe "<commands>" [seconds]` runs the game once with those commands and no screenshot, then prints the `LogLeafline` lines. Examples: a teleport's branch radius, or `Leaf.LightInfo`. After a teleport, `Leaf.LightInfo` logs the camera position and yaw, which gives the branch heading.

Parallel use:

- All runs share `Saved/Logs/RefShot.log`.
- All runs share `Saved/RefShots/.marker`, which a run uses to pick out its own screenshot. A concurrent run can therefore lose its screenshot ("no screenshot").
- Bash reads the script while it runs, so don't edit it in place while a run may be executing. Write a new file and move it into place.

Findings and values from the look pass: `docs/reviews/look-shots.md`.

## Material code (`Tools/materials`)

The look-pass bark `M_TreeBark2` is three Custom HLSL nodes; their code is kept here, not only in the asset:
`bark2_bark.hlsl` (colour, roughness, world-space normal, AO), `bark2_cut.hlsl` (camera cutaway and buried bark,
the opacity mask outside the shadow pass) and `bark2_keep.hlsl` (the shadow pass: buried bark only). Edit the files,
then have the main session push them into the material over unreal-mcp (`ProgrammaticToolset.execute_tool_script`;
the script environment can read files but has no `exec`, so the script body is pasted). `build_bark2_mcp.py` builds
the material from scratch (`MODE='build'`, only under a new name) or replaces the three nodes' code (`MODE='code'`);
`mpc_view_params_mcp.py` adds the cutaway scalars to `MPC_View`. Lessons from 13 Nov: a new Custom node already has
one unnamed input, and its `Inputs` and `AdditionalOutputs` arrays only take when the existing elements are sent back
exactly as read and one element is added per call; the vertex colour node's main output is RGB only (alpha is the `A`
output); and setting only a node's `Code` did not always mark the package dirty, so nudge a material property before
`save_assets`.

## Build info (`Tools/write_buildinfo.sh`)

Writes `Content/Leafline/UIRaw/buildinfo.txt` (short commit, `+dirty` if tracked files have uncommitted changes, and the time). The HUD's faint bottom-left line starts with it; without the file it says "dev build". The file is staged with the UIRaw folder and ignored by git. Run it right before `Scripts/package_win64.bat`.

## Floor shots (`Tools/floorfar_shots.sh`)

`ROOT=<tree> Tools/floorfar_shots.sh <label> <plan> [extra game args]` runs one standalone game from that tree's editor binaries (slot `RefShot`, through the Unreal lock) and saves each `SHOT` of the plan as `Saved/RefShots/<label>-<name>.png`. A plan is a text file with one step per line, `<seconds>|<console command>` or `<seconds>|SHOT <name>`; several `0|` lines run in order before the first delay. Plans from the floor pass (`docs/briefs/floor-from-above.md`) are in `Tools/floorfar_plans/`: `final.txt` (the owner's pose, 22 and 120 m up, the nest, fog at five times the density), `accept.txt` (M_Ground2 against M_Ground), `perf.txt` (CSV frame-time captures; pass `-noraytracing` as the extra argument for the second set; summarise with `python3 Tools/csv_fps.py --skip 1`), `ring.txt` (the near-to-far blend straight down), `shafts.txt` (fog start regression on the light-shaft views). For look comparisons hold the wind calm first (`0|Leaf.Tune wind CalmMin 400`, `CalmMax 400`, `StreakCount 0`, `0|Leaf.Cues 0`): the gust's dust front otherwise drifts through every 15 s or so.

## Video shots (`Tools/video_capture.sh`)

`bash Tools/video_capture.sh <shot> [label]` films one shot of the MegaJam video (`docs/ship/video.md`). Each shot goes to its own clip, so takes can be judged one by one. Output goes to `Saved/VideoCap/shots/`:
- `<shot>-take<N>.mp4`: 1920x1080, 60 fps, H.264, no sound;
- a contact sheet (`.jpg`);
- the game's events with their game time (`.log`).

How it runs:
- Through the Unreal lock (the editor must be closed).
- From the editor binaries, offscreen. It never takes the screen.
- In the save slot `Video`, and it checks that `Leafline.sav` is unchanged.

The frame dump: Unreal's own (`-dumpmovie`, with a fixed 1/60 s step and `-ForceRes`). Every frame is exactly 1/60 s of game time however long it takes to render, so camera shots, `Leaf.Delay` and `Leaf.Slomo` keep their timing. It renders about 4.5 frames per real second, so an 8 s shot takes about 2 minutes plus startup. The PNGs, about 3 MB each, land in `Saved/Screenshots/WindowsEditor`; the engine ignores a configured folder. They are encoded and deleted straight after each run.

Commands:
- `bash Tools/video_capture.sh list` lists the shots: `ant tree climb cut wind ride harvest recap bird move spire end`.
- `all` films every shot once; the bird only with `BIRD=1`.
- `reel` joins every take into `Saved/VideoCap/reel.mp4`, each labelled with its name, for choosing.
- `--probe <shot>` runs the steps without filming and prints the events with their game time, to place the window of a shot that waits on an event (the recap).
- `--dry-run <shot>` prints the console commands.

Environment:
- `DUMP_AT` / `DUMP_LEN` override a shot's window (game seconds).
- `SEED` and `SPIRE_SEED` choose the trees.
- `EXTRA` adds console commands at the start.

The shots are defined in `shot_def` at the top of the script.

Later additions (14 Nov evening):
- Calm shots set long calm spells (`Leaf.Tune wind CalmMin/CalmMax 90` before the day starts), so no gust streaks cross them.
- Filming windows sit inside the camera moves, because the camera's return to the player's view at the end of a move jumps in a clip.
- `all` films `SHOTS_ALL` (default: spire ant tree climb cut wind recap end).
- **Sound (`AUDIO=1`, or `--sound <shot> <take>` for an existing take).** A second, real-time run plays the shot's sound while ffmpeg records the VB-Audio cable. Three `Leaf.Sound ui_confirm` clicks before the window line the recording up with the frames.
  - A `-game` run cannot be told its output device: the editor's audio-device setting is not loaded in `-game`, and there is no console variable.
  - So Windows must send `UnrealEditor.exe` to "CABLE In 16ch (VB-Audio Virtual Cable)": Settings > System > Sound > Volume mixer. That also silences PIE in the editor until it is reset there.
  - Without that routing, the sound plays on the default device and no clicks are found.

## Video edit (`Tools/video_edit.py`)

`python3 Tools/video_edit.py` cuts the video together from the takes. It reads the cut list `docs/ship/video_cut.txt`, one line per clip: `<take> <in> <out> | <caption>`, or `@title TEXT`, or `@end A / B / C` in place of the caption.

Output and speed:
- By default it writes a 720p, 30 fps draft to `Saved/VideoCap/draft_720p.mp4`, in about 15 s.
- `--final` writes 1920x1080 at 60 fps to `Build/Video/leafline_megajam.mp4`.

What it does:
- hard cuts, a fade from black at the start and to black at the end;
- captions in Roboto Medium (the HUD's font), lower left;
- the takes' own sound where present, silence otherwise, normalised to −14 LUFS;
- prints the duration and whether it is inside the jam's 30–60 s.

Cut-list options (14 Nov, late): after `<out>`, a clip may carry `zoom=1.45` (enlarge), `zoomto=1.6` (push in from `zoom` to that value over the clip), `cx=` / `cy=` (the zoom's centre as fractions of the frame) and `box=1` (a dark box behind the caption, for clips where it lies over HUD text).

More in `video_capture.sh` (14 Nov, late):
- `--stills <shot> "<t1 t2 ...>" [label]` takes one frame at each game time and tiles them into `Saved/VideoCap/stills/<shot>-<label>.jpg` with the events log beside it. It takes about a minute. Use it to find a shot's timing and framing before filming. `RUN_FOR=<seconds>` makes a probe or stills run last that long.
- Shot `colony`: recruits coming down a trail with their pieces. It lays two lines with `Leaf.TestLines 2 0.6 climb`; the scout rides 3 s up one and stops. Vary it with `LINES`, `COLONY_AT`, `COLONY_AIM`, `COLONY_BOOM`, `COLONY_RIDE` and `COLONY_STRAFE` (steps the scout off the trail so the recruits are seen apart; the take in the cut used 0.5 with aim `up 20`).
- Shot `recap2`: the recap earned from three real lines, then the nest and a Brood purchase from that haul. `RECAP_AT` is the recap's game time (10.5 s, from `--probe recap2`).
- `Leaf.TestLines` lays only one line with `any` or `sheltered` on day 1's sparse low boughs (a single candidate leaf); `climb` picks the lowest leaves on different boughs.
