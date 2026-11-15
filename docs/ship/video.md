# The 60-second video: script, shot plan, capture

Written Sat 14 Nov (main session, for the owner). The video goes with the MegaJam submission. Most judges will see the game only through it.

## What the rules ask

- 30 to 60 seconds, "demonstrating recorded gameplay of the Submission" (Official Rules §1(e)). Resolution 1920x1080 or higher (§7(c)). English (§7(i)).
- No trademarks or logos, except the Unreal Engine logo used as Epic's branding guidelines describe (§15(d)). So the cards carry no Epic or jam logo. The UE logo is optional, and you would have to download it yourself.
- Teen audience, and nothing that could read as animal cruelty (§8). An ant blown off a branch walks home. Show the piece flying away, not a falling ant in close-up.
- Our targets: **57 s** total including cards, measured with `ffprobe`. 1920x1080 at 60 fps, H.264 MP4 with AAC audio. Only game sound and no added music, so there are no licensing questions.

## The idea

The judges score theme, fun, gameplay, visuals, progression and audio. The film has to show four things in a minute:

1. **Scale:** a tiny ant on an enormous tree.
2. **The loop:** climb, cut, send home, take the next ant.
3. **The theme:** you direct the colony with your trails and choose every cut. `design.md` maps this onto the theme post: "set the vision" is the route, "delegate" is sending workers along it, "decide what goes in" is the size of each cut.
4. **That it grows:** recruits, the day's cut, upgrades, the move to a taller tree.

There is no voice-over. Short captions carry the story, so the film also works with the sound off. The game's own sound carries the mood.

## Script

Times are positions in the finished film. **HUD** says whether the shot needs the game's interface. Cinematic mode hides the whole canvas, including the cut arc, the patch read-out and the recap, so those shots are filmed with the HUD on. **By** says whether a script can film the shot unattended or you play it by hand (see Capture).

| # | Time | Picture | Caption (lower left) | HUD | By |
|---|---|---|---|---|---|
| 1 | 0:00–0:04 | **The ant.** A slow orbit round the copper ant on a sunlit limb in morning light, the ground far below. | none | off | script |
| 2 | 0:04–0:10 | **The tree.** Looking up the trunk past the tiny ant, then a crane rising into the crown. | title **LEAFLINE** fades in at 0:07 | off | script |
| 3 | 0:10–0:15 | **The climb.** Chase shot behind the ant walking up the trunk, its scent trail on the bark behind it. | Lay the route. | off | script |
| 4 | 0:15–0:22 | **The cut.** At a leaf the patch outline and its read-out appear. Hold: the arc grows and shifts colour toward risky. Release: the piece goes up over the ant's head. | Choose every cut. | **on** | script, or you |
| 5 | 0:22–0:26 | **The wind.** On the exposed side a gust front sweeps in: wind lines, dust, leaves swaying. The ant grips with its big piece and holds. | Bigger cuts catch the wind. | off | script |
| 6 | 0:26–0:31 | **The shift.** Send home: "Worker 1 heads home with its piece", the camera flies down to a fresh worker at the nest, and the worker row changes. | Send it home. Take the next ant. | **on** | script |
| 7 | 0:31–0:34 | **The ride.** The new worker rides the trail at speed, chase camera low behind. | Ride the trails you laid. | off | script |
| 8 | 0:34–0:41 | **The colony.** Recruits climb coloured lines (cyan, magenta) in a file, filmed close beside a line on the trunk. Then the harvest: ants with leaf pieces coming down. | Your colony follows your line. | off, then on (harvest banner) | script (framing unproven), or you |
| 9 | 0:41–0:44 | **The bird** *(only if the bird ships tonight)*: it swoops past a laden ant on exposed bark. | Watch the sky. | off | script |
| 10 | 0:44–0:49 | **The day's cut.** The recap counts up line by line, then the nest screen where an upgrade is bought. | none (the screen says it) | **on** | script |
| 11 | 0:49–0:53 | **The move.** The card "The colony moves to a tall spire", then a crane up the spire into its crown. | Strip the low boughs. Climb higher. | on, then off | script |
| 12 | 0:53–0:57 | **End card.** Evening on the spire, a slow orbit at half speed. The picture dims behind the text. | **LEAFLINE**<br>Direct the colony. Choose every cut. | off | script + edit |

Without the bird, shot 8 runs 3 s longer, to 0:44, and the total stays 57 s.

**Sound.** Throughout: the wind bed and the ambient pad, footsteps in close shots. Spot sounds: the cut crunch (4), the gust rising and its peak (5), the deposit chime as pieces reach the nest (8), the bird's call and pass (9), the day-end and upgrade sounds (10). The gust in shot 5 is the loudest moment. Final mix: −14 LUFS integrated, true peak below −1 dBTP, measured with earshot.

**Captions.** Roboto Medium, as in the HUD (the engine's font, Apache 2.0). White with a soft shadow, about 4.5% of the frame height, 3 to 4 s each with 0.3 s fades. Burned in during the edit, not drawn in the game. Your call: drop the captions if you prefer the pictures alone. The end card stays either way.

## What the shots looked like on 14 Nov

`Tools/video_test.sh 1` (the main tree's editor binaries on Saturday afternoon, day 1, seed 517192) filmed one frame from each of the kit's moves:

- **Strong.** Looking up the trunk past the ant (the cinematic still). The chase up the trunk, with the trail behind the ant. The orbit on limb 21 with the ground far below. The crane into the crown.
- **Weak: the end of `reveal`.** It shows slim trees standing on an empty grey plain, so the tree looks small. The script uses the crane instead and keeps the reveal's first half in reserve. Look at it again on the final build: the far-floor relief and haze (`floor-far`) were merged after this test.
- **Weak: `dolly` on limb 21.** It runs along the shaded underside, so the limb is almost black against the sky. Use a sunlit bough, or leave the dolly out.
- **Gust streaks** read as rain in shots that should be calm. Each day's wind schedule is fixed by the seed (day 1 begins with 13 s of calm), so time the calm shots inside calm phases. Read the phases from the log of a trial run.
- That run's frames came out at **888x500**, although it asked for 1920x1080. The log shows the size dropping from 1280x720 to 1066x600 to 888x500 during startup. The primary display is 2560x1440, so the desktop-size clamp is not the explanation, and I have not found the cause. `-ForceRes` (which skips the engine's resolution clamp) gave 1920x1080 in both test runs below. The capture tool must still check the frame size on every run. Two side notes: `video_test.sh` does not have the executable bit set (run it with `bash`), and it does not pass `-ForceRes`.

## First takes (14 Nov, 19:00–20:00)

`video_capture.sh all` plus extra `cut` takes produced 17 takes, now in `Saved/VideoCap/shots/`; `Saved/VideoCap/reel.mp4` joins them, labelled. No take had camera frames inside wood.

- **Usable:**
  - `tree` (the opening up the trunk and the crown at the end; the middle passes close to bark);
  - `climb` (the first 5 s; the end blurs as the camera returns to the player's view);
  - `cut` take 4, and take 2 (the ant is clear on its twig, with the arc and read-out);
  - `ride` (along the cyan line from the ground and up the trunk);
  - `recap` (the day's cut counting up, then the nest screen);
  - `move` (the tall-spire card over the spire);
  - `end` (an evening limb, the ant lit warm).
- **Weak:**
  - `ant`: limb 21 is in shade.
  - `wind`: the camera sits low and close, and the carried piece fills the frame as a flat bright green shape.
  - `harvest`: no ants in view, a grass blade across the frame, and sun flare.
  - `spire`: a fast crane with motion blur, and gust streaks at the end.
  - `cut` takes 1, 3 and 5: the ant is hard to find.
- **Bird:** it dived and knocked the player's ant off (log at 5.5 s), but the camera never shows the bird; the frames show the piece close up, bark, then the nest.

## Draft 2 (14 Nov, 21:00)

The owner's notes on the reel:
- Leave out ride, harvest, move and probably the bird.
- Open on the spire, which feels more epic than the first tree.
- Some takes jump at the start or end.

The jumps were the camera easing into a move and returning to the player's view at its end, both inside the filming windows. The windows now sit inside the moves.

The reshoots:
- `spire` 2 and 3 (identical: that shot has no randomness). At 6.4–7.0 s the crane rises through foliage, and big leaves sweep across the lens. The cut skips that part.
- `ant` 3 (a sunlit limb).
- `tree` 2, `climb` 3, `end` 2.
- `wind` 2 and 3 (camera higher; still only fair).

The cut list is `docs/ship/video_cut.txt`. Draft 2 runs 48.7 s, silent, in `Saved/VideoCap/draft_720p.mp4`.

The recruits following a trail, the "delegate" half of the theme, has no shot since ride and harvest were dropped.

## Drafts 3 and 4 (14 Nov, 22:00–23:00)

Codex reviewed draft 2 from frames it extracted itself (`docs/reviews/video-draft2-codex.md`). It cannot play video, so it looks at frames 15 to 30 per second. Its main points:
- show delegation;
- put the wind before the send-home;
- drop the first tree clip (the crane lurches into the bark);
- replace the recap, which said "Trail laid: 0 m" and used `Leaf.Give 200`;
- reach the first cut sooner.

Draft 3 was its re-cut from existing takes. Draft 4 adds the two reshoots it asked for:

- **`colony`**: two real lines (`Leaf.TestLines 2 0.6 climb`) and six recruits. The third scout rides 3 s up a line and stops beside it, and the camera looks up the trunk. Leaf pieces come down the magenta trail toward the camera in pairs, at 24–28 s and 32–36 s of game time. The owner asked for "2–3 recruits walking back down the tree with leaves".
  - `Leaf.TestLines` with `any` or `sheltered` finds a single leaf on day 1's sparse low boughs, so only one line is laid. `climb` gives lines to different boughs.
  - Side-on views (`COLONY_AIM="0 -20"`) show the ant under its piece in profile, but each recruit crosses the frame in about a second, and the trunk reads as flat ground.
- **`recap2`**: three real lines, the harvest skipped. The recap shows three trail rows, "Leaf hauled: 47.1" from 12 pieces and "Trail laid: 76 m". On the nest screen, Brood is bought from that haul (47.1 → 33.1). No `Leaf.Give`.
- **Edit tool**: `video_edit.py` now takes `zoom=`, `zoomto=`, `cx=`, `cy=` and `box=1` per clip. The recap and the nest are pushed in from 1.2× to 1.5× so the numbers read (the owner's request), with a dark box behind their captions.
- **Stills mode**: `video_capture.sh --stills <shot> "<times>"` takes single frames at chosen game times in about a minute. It found the colony shot's timing and framing before any filming.

Draft 4 runs 49.2 s, in `Saved/VideoCap/draft4_720p.mp4`. Codex's review of it is in `docs/reviews/video-draft4-codex.md`.

## Draft 6: the picture-lock candidate (14 Nov, 23:10)

Codex round 2 called draft 4 a lock candidate with two must-fixes:
- end the colony clip before a leaf turns edge-on and vanishes;
- give the recap and the nest one steady zoom (`zoom=1.45 cy=0.25`) instead of two push-ins.

Draft 5 applied both and split the wind caption so the grip is named.

Draft 6 swaps in **`colony-take2`**: the scout steps off the trail first (`COLONY_STRAFE=0.5`, aim `up 20`), so the trail runs diagonally and two leaf pieces come down it visibly apart. Codex round 3 (`docs/reviews/video-draft6-codex.md`) chose it over the head-on take and found no must-fix.

The cut list is `docs/ship/video_cut.txt`. It runs 48.6 s:

| Film time | Clip | Caption |
|---|---|---|
| 0:00 | the ant on the spire's bark, then the spire's crown | title LEAFLINE |
| 0:06 | the ant on a sunlit limb | |
| 0:09 | the climb with the trail | Lay a scent trail. |
| 0:15 | the cut (HUD) | Choose your cut. |
| 0:20 | the gust and the grip | Bigger cuts catch the wind. / Grip through the gust. |
| 0:27 | send home, the next worker (HUD) | Send it home. / Next worker. |
| 0:30 | recruits coming down the trail with their pieces | Your colony follows your trail. |
| 0:36 | the recap, then the nest, enlarged (HUD) | A day's haul. / Buy Brood: more recruits. |
| 0:43 | evening limb, end card | LEAFLINE / Direct the colony. Choose every cut. |

Previews: `Saved/VideoCap/draft6_720p.mp4` and `draft6_1080p60.mp4` (1920x1080, 60 fps, 48.6 s, 50 MB; rendered in 44 s).

## Draft 7: sound (15 Nov, morning)

The owner routed `UnrealEditor.exe` to "CABLE In 16ch" in the Windows Volume mixer. The game's log confirms it ("Using Audio Hardware Device CABLE In 16ch"). `video_capture.sh --sound <shot> <take>` then recorded each of the eight takes in the cut. Three things had to change before it was right:

- **Markers after the window.** The three clicks that line the recording up with the frames were lost when they came before the window: the game plays no sound in its first two seconds.
- **A fixed frame rate of 60 for the sound run** (`bUseFixedFrameRate`, `FixedFrameRate=60` on the command line). Without it the slow first frame counted as several seconds: the early steps fired at once, and the sim ran seconds behind them. With it, the steps fire on the same frames as in the frame dump. A drift check on every run compares game time with the clock from the window to the markers: all eight takes are within 41 ms, most within a frame.
- **The purchase sound.** `Leaf.Buy` is silent; the game plays `upgrade` only for a menu purchase. The `recap2` shot now plays `Leaf.Sound upgrade 0.75` (the menu's volume) with the purchase.

`video_edit.py` now builds the sound itself:
- each clip keeps its take's sound;
- at each cut the two sounds cross-fade over 0.3 s (clips that follow one another in the same take are one stretch);
- one gain brings the film to −16 LUFS and a limiter holds the peaks at −1.5 dBFS.

So the quiet shots stay quiet and the gust stays the loudest moment.

earshot on draft 7 (`timeline`, `measure`):
- −16.9 LUFS integrated, true peak −1.2 dBTP, no clipped samples.
- The loudest stretch is the gust, 0:22.5–0:25.5, at −9.4 LUFS short-term.
- The cut's release lands at 0:18.72, the send-home sound at 0:26.90 (on its cut), and the purchase sound from 0:40.5.
- The limiter takes up to about 5 dB off the gust's peaks.

Not judged by ear yet (the owner's job):
- whether the pad's jumps at the cuts are covered by the cross-fades;
- two swells in the opening spire shot (0:00.6 and 0:01.8, about 20 dB over the bed);
- one swell on the nest screen (0:42.0).

The end card no longer carries the owner's name (owner, 15 Nov).

Previews: `Saved/VideoCap/draft7_720p.mp4` and `draft7_1080p60.mp4`.

## Draft 8 and the day-1 recruits (15 Nov, late morning)

- **Opening.** The owner found the first two seconds odd: the crane's start on the ant, cut against the crown, read as a pan one way whipped round into a move the other way. Draft 8 opens directly on the spire's crown with the title (`spire-take2 7.6–12.4`), then the sunlit ant. It runs 47.3 s: `Saved/VideoCap/draft8_720p.mp4`.
- **Draft 9** (owner, 15 Nov): the climb's caption is now "Climb anywhere."
  - The fresh trail is a thin pale ribbon until a piece goes home along it, and no camera made it read. The trail idea is carried by the colony shot.
  - The constant beds (wind, rustle, pad) were tiring in the film, where nobody sets the volume. `video_edit.py` now lowers them with a downward expander (`--bed`, default −9 dB) and sets the level from the loudest three seconds (`--loud`, default −10 LUFS).
  - earshot on draft 9: the calm opening went from −22 to −29 LUFS, the gust stays at −10, −17.0 LUFS integrated, true peak −1.4 dBTP, no clipped samples.
  - The owner's verdict on draft 9: the wind is still loud, and the expander makes the pad cut in and out. `--bed` is now off by default.
  - The game sets the loop volumes in code (`WindSystem.cpp`) with no tuning value. The owner has asked the main session to turn the wind down in the game; the video's sound waits for that build and is then re-recorded.
- **A new colony now has two recruits from day 1** (`6eec0b0`: Brood ladder 2/3/4/6/8/10/12, the bird from day 2). The takes in drafts 6 to 8 were filmed the evening before on a forced pool of six. For the final pass:
  - `colony` and `recap2` no longer force a pool (`VIDEO_POOL=<n>` still can), so both show a real day 1.
  - `recap2`, probed on the new build: 31.5 hauled from 8 pieces, one lost to the wind, 76 m of trail, the recap at 9.7 s. Brood (14) is still affordable.
  - `colony`, in stills on the new build: the two recruits come down as a pair between 25 and 29.5 s. The shared trunk stretch of the trail changes from magenta to cyan at about 24.5 s (the brighter line's colour), so the window now starts at 24.8 s.
  - `spire`, `ant` and `end` are on day 3, which is now a bird day; they switch the bird off (`Leaf.Bird 0`) so it cannot cross the frame.
  - `cut`: after the send-home, the goal line at the nest now names the recruits. Day 1 has no bird, so no bird hint covers the HUD.

## Draft 10: the final pass (15 Nov, 12:25–13:35)

Every take in the cut was filmed again, with its sound, from the build of 15 Nov 12:00. That build has recruits from day 1, richer leaves higher up with new milestones, the seam-blended bark, and the quieter calm wind (`ef61b1b`: the calm bed about 4.5 dB down, gusts as loud as before). Last night's takes are in `Saved/VideoCap/shots_1114`.

- **Takes used:** `spire-take1`, `ant-take3`, `climb-take1`, `cut-take2`, `wind-take1`, `colony-take1`, `recap2-take1`, `end-take1`. All sound runs were within 17 ms of drift.
- **Ant.** Two random limbs were shaded. The shot now goes to limb 20 of the spire, which is sunlit in the evening (`ANT_LIMB`).
- **Cut.** Four takes on random leaves; take 2 shows the ant clearly on its twig. Take 4 has no sound (one marker click was missed) and is unused.
- **Colony and recap** use the game's own two recruits: the recap shows 37.2 hauled from 8 pieces, one lost to the wind, 76 m of trail.
- **Sound, by earshot.**
  - −18.9 LUFS integrated, true peak −1.3 dBTP, no clipped samples.
  - The calm passages sit at −26 to −30 LUFS (−22 in draft 7) and the gust at −11, with no expander.
  - The recording as a whole came out about 4 dB hotter than last night's (−26.2 against −30.4 LUFS), so the film needs 4 dB less gain; that also lowers the beds.
- **Files.** Preview `Saved/VideoCap/draft10_720p.mp4`. The final render is `Build/Video/leafline_megajam.mp4`: 46.2 s, 1920x1080, 60 fps, H.264 High, AAC 48 kHz stereo, 51 MB.

**earshot's judgement of the final sound** (15 Nov, afternoon; `ab`, `match`, `measure`, `critique`):
- **Wind.** Against draft 7, the calm stretch (ant and climb) is 8.4 dB quieter (−30.5 against −22.1 LUFS). Within it, the noisy bands are about 3.3 dB lower against the pad's band (mid −15.9 against −12.6 dB, presence −11.7 against −8.4), and the harmonic ratio rose from 0.86 to 0.93. In a forced choice the stretch read as "white noise hiss" 58% in draft 7 and 21% now; it now reads mostly as leaves rustling and pad.
- **Pad at the cuts.** The pad is a fixed set of notes (about 64, 96–100, 132, 164, 196, 292 Hz) whose balance drifts over its 48 s loop. Across most cuts the low band steps by 2 dB or less (3.8 dB from climb to cut), and the same notes are present on both sides.
- **The one larger step** is the "Next worker" clip, 8 dB up in the low band. That is not the pad: a gust warning builds there (the HUD says "gust coming") and peaks 0.1 s before the cut to the colony shot. It works as a transition swell. `CALM=1` on the `cut` shot would remove it.
- **Against launch films** (`critique`): the verdict is "needs work", mostly for reasons of style (no music-led track, synthesised sound, few accents on cuts). Two points are relevant here:
  - the film is quiet overall (−18.9 LUFS; web films sit between −16 and −9);
  - the gust stands 8 LU above the rest, whose quietest stretch is at −31 LUFS.
- **Alternative mix.** `Saved/VideoCap/draft11_720p.mp4` (cut list `Saved/VideoCap/cut_draft11.txt`: `vol=-4` on the wind clips) lifts the quiet passages by 4 dB to about −27 LUFS and leaves the gust where it is. The owner compared the two and chose draft 10: draft 11 makes the wind louder again. The final file keeps draft 10's balance.

**Draft 12 (15 Nov, 14:40): the nest hand-over.** The owner saw about four ants piled on each other in the "Next worker" clip.
- Cause: the `cut` shot teleports the scout to its leaf, so the line it leaves is a four-point jump. The day's two recruits cannot follow it and stay in the nest entrance with the next worker. It comes from the staging; in play the route is walked.
- Fix: the hand-over clip now comes from `cut-nest`, filmed with `CUT_POOL=0` (no recruits for that take). The cut and send-home clips still come from `cut-take2`.
- Along the way: a rising gust masked the third marker click of a sound run twice, so the marker clicks are now played louder (volume 8).
- The final file was rendered again with this clip. Its sound is unchanged by earshot `measure`: −18.9 LUFS integrated, true peak −1.3 dBTP, no clipped samples.

Still to do (owner):
- Listen to and approve the final file.
- Upload it to YouTube as unlisted, check that it plays signed out, and put the link in the submission form and both itch pages.
- Set `UnrealEditor.exe` back to the default output device in the Windows Volume mixer.

## Draft 13: the final UI and the music (15 Nov, 21:00–21:50)

The cut is draft 12's. Every shot was filmed again from the build of 15 Nov 21:00 (main at `2a6a04c`): the artist's panel skin, Codex's HUD icons, the music and the quieter wind bed. The takes of drafts 10 to 12 are kept in `Saved/VideoCap/shots_1115a`, and the earlier final in `Saved/VideoCap/leafline_megajam_draft12.mp4`.

- **Picture.** Only the cut, the nest hand-over and the recap show the HUD, so only they look different. Colony, climb, spire and ant came out frame for frame like the earlier takes; the cut and the end card use a random leaf and limb, so they are new views. `affbf91` (exact screen-direction mapping on by default) landed during the pass; climb and colony, the two shots that walk the ant by input, match their earlier takes.
- **Music.** The game now plays one track per day. Each take would start its track anew, and the music would jump at every cut, so the takes are filmed with the music muted (`EXTRA="Leaf.Tune wind MusicVolume 0"`: the with-music wind levels, no pad), and day 1's track, `Art/Audio/music_a.wav`, runs as one piece under the whole film. Its level is measured, not chosen: a recording of the tree shot with the music on gives −15.1 dB on the file (`Tools/video_music_gain.py`, correlation 0.99), and the edit mixes it in before the loudness gain (`--music Art/Audio/music_a.wav --music-db -15.1`). The day-3 shots (spire, ant, end card) would play `music_c` in the game.
- **Sound passes.** Three things went wrong and are fixed or noted:
  - The Volume mixer routing of `UnrealEditor.exe` to the cable was gone (the game opened "Voicemeeter Input"); the owner set it again at 21:08.
  - The fixed-rate sound pass held only about 43 frames per second in the first three passes (cause not found; later the same evening it held 60 again), so the marker clicks came out 0.45 to 0.6 s apart. The pass can now follow the real clock (`docs/tools.md`, the default), and the check compares the scripted steps' clock times with their game times. Drift of the takes in the film: at most 9 ms on the real clock, +17 ms for the wind take and +39 ms for the recap (both at the fixed rate).
  - Another agent's game in the second lock slot shared the GPU during one spire pass (3 s lost); it was recorded again.
  - The recap needs the fixed rate (`SOUND_CLOCK=fixed`): its start is set by the simulation, not by a scripted step, and on the real clock it came 0.33 s early against the picture (3.00 s after the skipped harvest, 3.33 s in the frame dump). At the fixed rate it is 0.11 s early.
- **Readings** (earshot `measure`): −19.6 LUFS integrated, loudest three seconds −10.8 LUFS (the gust), true peak −1.3 dBTP, no clipped samples. The calm stretch (ant and climb, 4.8–13.8 s) reads −26.3 LUFS against −30.5 in draft 12: the music fills it, and the hiss bands (presence, air) are 9 dB lower against the rest. The purchase sound starts at 38.22 s; the purchase is on the picture at 38.23 s.
- **File.** `Build/Video/leafline_megajam.mp4`: 46.23 s, 1920x1080, 60 fps, H.264 High, AAC 48 kHz stereo, 50 MB. Preview: `Saved/VideoCap/draft13_720p.mp4`.

## Shot setups

Each run is one launch with its own save slot, `-LeafSlot=Video`, so your colony is never touched. The commands below are starting points. A trial run without frame dumping confirms the times first (see Capture). The tree is seed 517192 (it grows the old giant, seed 675856) unless noted. Each run starts with:

`r.DumpingMovie 0, Leaf.ResetSave, Leaf.OneTree 1, Leaf.StartDay <day> 517192, Leaf.BuildLine 0, Leaf.PadIcons 0`

Add `Leaf.Cinematic 1` in the runs without the HUD. Every shot is filmed with half a second spare at each end. The first frame of a dump can be blurred by the move starting, and the edit trims it.

**Seen** says how much of each run has actually been looked at as of Sat 14 Nov:
- *motion*: frame-dumped and encoded;
- *stills*: frames from a run with the same steps;
- *borrowed*: the steps come from another agent's test script, but no frames were taken for video;
- *unseen*: nothing yet.

| Run | Shots | Setup after the common start | Seen | Notes |
|---|---|---|---|---|
| A ant | 1 | `Leaf.Teleport 21 0.5`, then `Leaf.Shot orbit 10` | stills | use 4 s of the turn, the sunlit side |
| B tree | 2 | `Leaf.Teleport 0 0.2`, `Leaf.Aim up 25`, hold 2 s, then `Leaf.Shot crane 8` | stills | inside day 1's opening calm |
| C climb | 3 | `Leaf.Teleport 0 0.2`, `Leaf.AutoMove 0.6`, `Leaf.Shot follow 8` | motion | the trail shows behind the ant |
| D loop | 4, 6 | HUD on. `Leaf.GoLeaf`, then `Leaf.Hold cut 3`, and 4.5 s later `Leaf.SendHome` | stills | The cut reads well at 1080p: the arc counting 2.1, 3.2, 4.1, "This piece: risky in a gust", "Line 1 here: about 9 pieces, rich ×1.3", the cut leaf outlined. Leaves crowd that take (leaf 1974), so try more. `GoLeaf` picks at random; `-FixedSeed` should make the pick repeat (untested). The send-home view flies to the nest with motion blur, so trim to the banner and the arrival. |
| E wind | 5 | `Leaf.FaceWind exposed`, `Leaf.Carry 0.9`, `Leaf.WindAim 90 -12`, `Leaf.Gust 1`, `Leaf.Hold grip 6` from 2.5 s after the gust | borrowed | the same steps as `Tools/cues_shots.sh grip` |
| F colony | 7, 8, 10 | `Leaf.Pool 6` before the start. `Leaf.TestLines 2 0.6 any`, the ride (`Leaf.Hold follow 6` with `Leaf.Shot follow 6`), a close view beside a line, `Leaf.Harvest` with `Leaf.Slomo 0.5`, the recap, then `Leaf.Buy 4` on the nest screen | stills of the harvest only | See the note below the table. |
| G bird | 9 | `Leaf.Pool 2` before the start (a bird day needs recruits). `Leaf.FaceWind exposed`, `Leaf.Carry 0.8`, `Leaf.Tune cam SurfaceFollow 0`, `Leaf.BirdNow`, `Leaf.BirdInfo aim` | unseen | only if the bird ships |
| H spire | 11, 12 | `Leaf.Move` twice (the second shows the tall-spire card, HUD on). Send two workers home (`Leaf.Carry 0.5`, `Leaf.SendHome`) so the sun stands at evening, then `Leaf.Teleport 0 0.15`, `Leaf.Shot crane 8`, then `Leaf.Slomo 0.5` with `Leaf.Shot orbit 12` | unseen | `video_test.sh 3` shows how to reach tree 3 |

**Run F, as seen so far.**
- The harvest view (camera at the nest, looking up the trunk) shows the magenta line climbing the bark. The sampled frames show no ants on it.
- A blurred grass blade from the forest floor crosses the picture, and the trunk base is soft (depth of field).
- With `Leaf.SkipHarvest` the recap had still not come 20 s later (9 trips left), so the recap and the nest screen are unseen.
- Shot 8 needs a different camera. Put the controlled ant beside a line on the trunk and let recruits pass it. If the script cannot frame that well, it is the best shot for you to play by hand.

## Capture

**Short answer.**
- **Picture.** All of the picture can come from scripts, with no game code: Unreal's frame dump at a fixed 1/60 s step, offscreen, at 1920x1080. That is tested.
- **Shots seen working today:** 1 to 4 and part of 6.
- **Shots still to prove in a rehearsal:** the colony (8), the recap and nest (10), the move and spire (11, 12) and the bird (9). Shot 8 is the likeliest to need your hands.
- **Sound.** The dump has no sound. It comes either from the game's own sound files laid at the logged events (no setup, tested in a 4 s mix) or from a second real-time pass routed through the VB-Audio cable (one Windows setting from you, untested).

### What I tested today

Two runs of the main tree's editor binaries as built on Saturday afternoon, offscreen, through the Unreal lock, with this launch line:

`-game -windowed -ResX=1920 -ResY=1080 -ForceRes -RenderOffscreen -nosplash -unattended -LeafSlot=VideoCap -UseFixedTimeStep -FPS=60 -dumpmovie`

1. `Leaf.Delay 4 r.DumpingMovie 240` during a chase shot up the trunk, in cinematic mode.
2. With the HUD on: the cut, send-home, a test line, the harvest and the harvest skip, with single frames grabbed by `r.DumpingMovie 1` at chosen times.

The test launchers are `Saved/VideoCap/captest.bat` and `captest2.bat`.

| Question | Result |
|---|---|
| Size | 1920x1080, with `-ForceRes`. |
| Timing | Every 1/60 s of game time is one frame, however long the frame takes to render. Each `Leaf.Delay` fired on its exact frame (1 s at frame 60, 3.5 s at frame 210). The 6 s shot took exactly 360 frames. Shots, delays and slow motion all follow this clock. |
| Smoothness | 240 frames, no repeated or dropped frames (ffmpeg `freezedetect`: none). The motion in the contact sheet looks even. |
| HUD and cursor | The HUD is in the dumped frames (run 2). The mouse cursor and Slate widgets are not, according to the engine source; the game draws no Slate. |
| Speed | About 4.5 frames per second of real time. One second of film costs about 13 s of machine time, so a 60 s film with spare frames is about 25 minutes of dumping. It runs offscreen and never takes your screen. |
| Disk | About 3 MB per PNG frame. The 4 s test was 729 MB. |
| Frame folder | **Cannot be moved.** The `-ini:` override is overwritten in code (`GameEngine.cpp:919`). Frames land in `Saved/Screenshots/WindowsEditor/MovieFrameNNNNN.png`. So each shot is encoded to video straight after its run and its PNGs are deleted. That needs at most about 3 GB free on D: at a time. |
| Your save | Unchanged (checked by checksum). |
| Sound | The frame dump records **no sound**. |

I encoded the frames to H.264 and mixed a test track from the game's own WAVs: the wind and pad loops plus the cut sound placed at 0.5 s. earshot `measure` put the cut's peak at 0.505 s and read −15.2 LUFS with a −1.3 dBTP true peak. Timing a sound to a frame works.

### Sound: two ways

1. **Built from the game's sounds (no setup, tested above).** The edit lays the game's own WAVs (`Art/Audio`) at the frames where their events happen in the log: wind and pad beds, the gust when the log says it rises, the cut, the chime at each deposit, the bird, the day-end and upgrade sounds. Footsteps go only in close shots. It is the game's sound, arranged the way a trailer arranges it. It is not a recording of a play session. I think that is fine for a 60 s film, but it is your call.
2. **Recorded from the game (one setting from you, untested).** The game sends its sound to the VB-Audio cable, which is already installed. In Windows' per-app sound settings (Volume mixer), set `UnrealEditor.exe`'s output to **CABLE Input**. The packaged game, which you play, keeps your speakers. A second, real-time pass of each run then records its sound with ffmpeg from **CABLE Output**. A `Leaf.Sound` click before the shot lines it up with the frames. The game mutes itself when it has no focus, and an offscreen game never has focus, so the run sets `au.DisableAppVolume 1`. Every scripted event happens at the same game time in both passes, so they should agree to a frame or two. That is unverified until a test.

Not usable, for the record:
- Unreal's movie capture. In a packaged or `-game` run it has no audio pass, and its frame-rate flag is applied too late.
- `-benchmark`, which switches audio off.
- `-DeterministicAudio`, which writes no file.
- Unreal's own WAV recording, which has no console command (it would need new C++ before the freeze).
- Your OBS. Its websocket is off, and its profile is a 4K x264 setup bound to other games.

### The pipeline (no game code)

1. **`Tools/video_capture.sh <shot>`** (written 14 Nov evening; usage in `docs/tools.md`). It films one take of one shot to `Saved/VideoCap/shots/<shot>-take<N>.mp4`, with a contact sheet and the events log. `reel` joins all takes, each labelled, into `Saved/VideoCap/reel.mp4` for choosing.
   - The shots are the runs above, split one clip per shot: `ant tree climb cut wind ride harvest recap bird move spire end`.
   - `--probe <shot>` runs a shot without filming and prints its events with their game time. That places the window for shots that wait on an event (the recap).
   - The probe and the capture should hit the same frames under the fixed step, but that is **untested**. Compare the two `.log` files the first time.
2. `Tools/video_edit.py` (to be written), from a cut list (clip, in, out, caption):
   - the cuts, with a few frames of crossfade where a hard cut jars;
   - the captions and cards (`drawtext`, Roboto Medium);
   - the sound (way 1 or 2), loudness-normalised in two passes;
   - export to `Build/Video/leafline_megajam.mp4`: 1920x1080, 60 fps, H.264 High, AAC 256k, faststart.
3. Checks before you see it:
   - `ffprobe`: 30 s ≤ duration ≤ 60 s, 1920x1080, an audio stream;
   - earshot `timeline` and `measure`: loudness, peaks, sounds landing on their pictures;
   - a contact sheet: no build line, no debug text, no camera inside wood.
4. **Rehearse tonight** on the current build, so that only the final pass is left for Sunday. Do the final pass from the final package if `-dumpmovie` works there. Test that first; the source says it should. Otherwise use the editor binaries of the same commit, which is the same game. The packaged build shares its `Saved` folder with your play, so its save and settings are backed up and restored as `Tools/perf_packaged.sh` does.

### What a script cannot do

- **Feel.** The scripted ant walks with constant input and turns with even rates. If a shot looks mechanical, or the script cannot frame it (shot 8 most likely), play it yourself. Record with OBS at 1080p60 NVENC, and the clip drops into the same cut list.
- **Taste.** You pick between takes and approve the cut.
- **Anything with your accounts.** Uploading the video and submitting stay with you (§17 for the submission).

## What you need to take care of

**Tonight, before the 23:00 freeze**
1. **The bird decision at 20:00** decides shot 9.
2. **No game code is needed for the video, as far as tested.** The cut arc, read-out and recap stay HUD-on shots. One thing players see too: the harvest camera at the nest looks through a blurred grass blade from today's forest floor (run F). That is a pre-freeze fix if you want it. Say so if you want anything else changed in the game for the film; tonight is the last chance.
3. **Sound: way 1 or way 2?** For way 2, set `UnrealEditor.exe` to CABLE Input in the Volume mixer, and I test it tonight.
4. **A go for the two tools and a rehearsal tonight.** About an hour of machine time, offscreen, through the lock, in the `Video` save slot.
5. **Captions: keep or drop?** Also confirm the title **Leafline** for the cards; `design.md` still lists the title as yours to confirm.

**Sunday**
1. Watch the rehearsal cut. Pick takes and say what to change.
2. After the final package, I run the final capture and edit, about an hour.
3. Optionally, play any shot the script frames badly (likely 8) yourself with OBS.
4. Watch the final file once with sound, at full screen.
5. Upload it to YouTube as **unlisted**, wait until the 1080p version has processed, and check that it plays while signed out.
6. Put the link in the submission form and both itch pages (`docs/ship/submission-form.md`, `itch-megajam.md`, `itch-gameoff.md`).
7. Submit. Agents prepare everything but never submit (§17).

**Keep out of the frame:** logos (the UE logo only if you add it yourself, per Epic's guidelines), music you did not make, the desktop, the cursor, the console, the build line, and any recorder watermark.
