#!/usr/bin/env bash
# Films one shot of the MegaJam video (docs/ship/video.md) to its own clip, so takes can be judged one by one.
#
#   bash Tools/video_capture.sh <shot> [label]     film one take -> Saved/VideoCap/shots/<shot>-<label>.mp4 (+ .jpg, .log)
#   bash Tools/video_capture.sh all                film each shot in SHOTS_ALL once (default: spire ant tree climb
#                                                  cut wind recap end)
#   bash Tools/video_capture.sh --probe <shot>     run the shot's steps without filming; print the game's events with
#                                                  their game time, to place DUMP_AT for shots that wait on an event
#   bash Tools/video_capture.sh --stills <shot> "<t1 t2 ...>" [label]   one frame at each game time, as a labelled
#                                                  sheet in Saved/VideoCap/stills (fast; RUN_FOR=<s> runs a probe or
#                                                  stills run that long)
#   bash Tools/video_capture.sh --dry-run <shot>   print the console commands only
#   bash Tools/video_capture.sh reel               join every take in the shots folder into Saved/VideoCap/reel.mp4,
#                                                  each labelled with its file name, for choosing takes
#   bash Tools/video_capture.sh --sound <shot> <take>  add sound to an existing take (e.g. --sound cut cut-take4)
#   bash Tools/video_capture.sh list               the shots
#
# How: the editor binaries run the game standalone and offscreen at a forced 1920x1080 (-ForceRes), with a fixed
# 1/60 s step (-UseFixedTimeStep -FPS=60) and Unreal's frame dump (-dumpmovie): every rendered frame is exactly 1/60 s
# of game time however slowly it renders (about 4.5 frames per real second), so shots, Leaf.Delay and Leaf.Slomo keep
# their timing. Dumping is off until DUMP_AT, then r.DumpingMovie films DUMP_LEN seconds. The PNGs (about 3 MB each)
# land in Saved/Screenshots/WindowsEditor (the engine ignores a configured folder), are encoded to H.264 straight after
# the run and deleted. Save slot "Video"; the owner's Leafline.sav is checked unchanged. Runs go through the Unreal lock.
#
# Sound (AUDIO=1, off by default): after the frame dump, a second, real-time run of the same steps plays its sound while
# ffmpeg records the VB-Audio cable; three marker clicks just after the window line the recording up with the frames.
# Unreal has no setting a -game run reads to pick its output device (the editor's own audio-device setting is not
# loaded in -game), so Windows must send UnrealEditor.exe to the cable: Settings > System > Sound > Volume mixer >
# UnrealEditor.exe > Output device "CABLE In 16ch (VB-Audio Virtual Cable)" (it is listed while an UnrealEditor.exe
# runs). Without that the sound pass plays on the default device and finds no marker clicks in the recording.
#
# Day-3 shots (spire, ant, end) switch the bird off: since 15 Nov a new colony has recruits, so day 3 is a bird day.
#
# Environment: SEED (default 517192; spire, ant and end use SPIRE_SEED, default 15161), DUMP_AT / DUMP_LEN (override
# the shot's window, seconds of game time), EXTRA (more console commands, comma separated, run at the start), AUDIO,
# SHOTS_ALL, WIND_AIM / WIND_PIECE (the wind shot's camera and piece size).
set -euo pipefail
ROOT=/mnt/d/UnrealProjects/AntTreeGame
ROOT_WIN='D:\UnrealProjects\AntTreeGame'
OUT=$ROOT/Saved/VideoCap/shots
FRAMEDIR=$ROOT/Saved/Screenshots/WindowsEditor
SAVE=$ROOT/Saved/SaveGames/Leafline.sav
FONT="/mnt/c/Program Files/Epic Games/UE_5.8/Engine/Content/Slate/Fonts/Roboto-Medium.ttf"
SHOTS="spire ant tree climb climb2 cut wind colony ride harvest recap recap2 bird move end"
SHOTS_ALL=${SHOTS_ALL:-"spire ant tree climb cut wind recap end"}   # what `all` films (owner, 14 Nov: ride, harvest,
                                                                    # move and bird left out)

# A shot: DAY, POOL (recruit pool forced for the day, empty = none), PRE (commands before Leaf.StartDay), CALM (1 =
# long calm spells, so no gust streaks cross a calm shot), HUD (1 = visible, 0 = Leaf.Cinematic 1), STEPS
# ("<seconds>|<command>", game time from launch), DUMP_AT, DUMP_LEN. The filming window starts at least 1.5 s after a
# camera move starts and ends at least 1 s before it ends: a move eases in, and at its end the camera returns to the
# player's view, which jumps in a clip (owner, 14 Nov).
shot_def() {
	DAY=1; SEED_USE=${SEED:-517192}; POOL=""; PRE=""; CALM=0; HUD=0; STEPS=()
	local evening=("2|Leaf.Carry 0.5" "4.5|Leaf.Carry 0.5")   # two pieces delivered at the nest: shift 3, evening sun
	case $1 in
		spire) # opener: the tall spire in the evening, a slow crane from the ant up into the crown
			DAY=3; SEED_USE=${SPIRE_SEED:-15161}; PRE="Leaf.Bird 0,Leaf.Move,Leaf.Move"; CALM=1
			STEPS=("${evening[@]}" "6|Leaf.Teleport 0 0.15" "6.1|Leaf.Aim up 25" "7.5|Leaf.Shot crane 15"); DUMP_AT=8.5; DUMP_LEN=13 ;;
		ant) # 1: orbit round the ant on limb 20 of the spire, which is sunlit in the evening (random limbs were mostly
			# shaded; ANT_LIMB="L1 0.5" picks a random one)
			DAY=3; SEED_USE=${SPIRE_SEED:-15161}; PRE="Leaf.Bird 0,Leaf.Move,Leaf.Move"; CALM=1
			STEPS=("${evening[@]}" "6|Leaf.Teleport ${ANT_LIMB:-20 0.5}" "7|Leaf.Shot orbit 14"); DUMP_AT=8.5; DUMP_LEN=10 ;;
		tree) # 2: up the trunk past the ant, then a slow crane into the crown
			CALM=1; STEPS=("1.5|Leaf.Teleport 0 0.2" "1.6|Leaf.Aim up 25" "4|Leaf.Shot crane 13"); DUMP_AT=2.5; DUMP_LEN=13 ;;
		climb) # 3: chase up the trunk with the trail behind
			CALM=1; STEPS=("1.5|Leaf.Teleport 0 0.2" "2|Leaf.AutoMove 0.6" "2.2|Leaf.Shot follow 12"); DUMP_AT=3.7; DUMP_LEN=7 ;;
		climb2) # 3, closer (owner, 15 Nov: the trail was all but invisible from the chase camera): the player's own
			# camera close behind the ant (cinematic damping), the fresh pale trail in the foreground. CLIMB_BOOM /
			# CLIMB_AIM vary it.
			CALM=1; STEPS=("1.5|Leaf.Teleport 0 0.2" "1.6|Leaf.Tune cam BoomLength ${CLIMB_BOOM:-170}" "1.7|Leaf.Aim ${CLIMB_AIM:-up -25}"
				"2|Leaf.AutoMove 0.6"); DUMP_AT=4; DUMP_LEN=7 ;;
		cut) # 4 and 6, HUD: a random leaf (each run differs), hold to cut, send home, the hand-over at the nest
			# CUT_POOL=0 for the hand-over at the nest: the scout is teleported to its leaf, so its route home is one
			# jump, and the day's recruits cannot follow it and pile up in the nest entrance (owner, 15 Nov).
			HUD=1; POOL=${CUT_POOL-}; STEPS=("2|Leaf.GoLeaf" "4.5|Leaf.Hold cut 3" "9|Leaf.SendHome"); DUMP_AT=3; DUMP_LEN=11 ;;
		wind) # 5: exposed side, a piece, a full gust (warning 3 s, rising 1.2 s), grip as it rises; camera from above
			# (14 Nov: from -12 the piece filled the frame). WIND_AIM and WIND_PIECE vary it.
			STEPS=("1.5|Leaf.FaceWind exposed" "1.6|Leaf.Carry ${WIND_PIECE:-0.7}" "2|Leaf.WindAim ${WIND_AIM:-90 -35}" "2.5|Leaf.Gust 1"
				"5|Leaf.Hold grip 6"); DUMP_AT=3; DUMP_LEN=9 ;;
		ride) # 7 and 8: two lines with six recruits on them; the third worker rides one from the nest, chase camera
			POOL=6; STEPS=("2|Leaf.TestLines 2 0.6 any" "9|Leaf.Hold follow 8" "9.1|Leaf.Shot follow 8"); DUMP_AT=9; DUMP_LEN=8 ;;
		harvest) # 8, HUD: the harvest view at the nest, slowed, then the next view (a patch)
			HUD=1; POOL=6; STEPS=("2|Leaf.TestLines 2 0.6 any" "16|Leaf.Harvest" "16.1|Leaf.Slomo 0.4" "21.5|Leaf.Cycle")
			DUMP_AT=16.5; DUMP_LEN=10 ;;
		recap) # 10, HUD: a piece handed to each worker at the nest is delivered at once (probe, 14 Nov: the recap
			# starts at 7.5 s); the recap, then confirm (grip) to the nest screen and buy Brood
			HUD=1; STEPS=("2|Leaf.Carry 0.7" "4|Leaf.Carry 0.7" "6|Leaf.Carry 0.7" "13|Leaf.Hold grip 0.2" "13.5|Leaf.Give 200"
				"15|Leaf.Buy 4"); DUMP_AT=7; DUMP_LEN=11 ;;
		colony) # delegation (Codex review of draft 2; owner: recruits walking back down with leaves): two lines to the
			# lowest leaves on different boughs ("climb"; "any" and "sheltered" find a single leaf on day 1's sparse
			# low boughs) and the colony's own recruits (two on day 1 since 15 Nov; VIDEO_POOL=<n> forces a pool).
			# The third scout rides a few seconds up a line, steps off it (COLONY_STRAFE, so the recruits are seen
			# apart) and the camera looks up the trunk: the two recruits come down past it with their pieces between
			# 25 and 29.5 s. Until about 24.5 s the shared trunk stretch is magenta, then cyan (the brighter line's
			# colour), so the window starts after that. LINES is the Leaf.TestLines arguments; COLONY_AT / COLONY_AIM /
			# COLONY_RIDE / COLONY_BOOM vary the view. (The take in draft 7 was filmed with VIDEO_POOL=6, COLONY_AT=23.)
			POOL=${VIDEO_POOL-}; CALM=1
			STEPS=("2|Leaf.TestLines ${LINES:-2 0.6 climb}"
				"9|Leaf.Hold follow ${COLONY_RIDE:-3}" "12.3|Leaf.AutoStrafe ${COLONY_STRAFE:-0.5}" "12.9|Leaf.AutoStrafe 0"
				"13|Leaf.Tune cam BoomLength ${COLONY_BOOM:-240}"
				"13.1|Leaf.Tune cam DepthOfFieldFstop 0" "13.2|Leaf.Aim ${COLONY_AIM:-up 20}"); DUMP_AT=${COLONY_AT:-24.8}; DUMP_LEN=5 ;;
		recap2) # the earned recap (Codex review of draft 2): three real lines, the day's own recruits (two on day 1
			# since 15 Nov: 31.5 hauled from 8 pieces, one lost to the wind), the harvest skipped, the recap with a
			# real trail length and haul, then the nest screen and Brood bought from that haul. RECAP_AT is the
			# recap's game time (from --probe recap2: the "Recap, day 1" line).
			HUD=1; POOL=${VIDEO_POOL-}; CALM=1; local r=${RECAP_AT:-9.7}   # probe, 15 Nov (two recruits from day 1): the recap starts at 9.73 s
			STEPS=("2|Leaf.TestLines ${LINES:-3 0.7 climb}" "6|Leaf.Harvest" "6.5|Leaf.SkipHarvest"
				"$(python3 -c "print($r+3.8)")|Leaf.Hold grip 0.2" "$(python3 -c "print($r+5.3)")|Leaf.Buy 4"
				"$(python3 -c "print($r+5.3)")|Leaf.Sound upgrade 0.75")   # the console purchase is silent: the game's own
			DUMP_AT=$(python3 -c "print($r+0.2)"); DUMP_LEN=8 ;;          # purchase sound at the menu's volume (DayDirector)
		bird) # 9 (only if the bird ships): a laden ant on exposed bark, the bird sent at once, the camera toward it
			POOL=2; STEPS=("1.5|Leaf.FaceWind exposed" "1.6|Leaf.Carry 0.8" "1.7|Leaf.Tune cam SurfaceFollow 0" "3|Leaf.BirdNow"
				"3.3|Leaf.BirdInfo aim"); DUMP_AT=3; DUMP_LEN=8 ;;
		move) # 11, HUD: two moves; the second card is the tall spire's
			HUD=1; STEPS=("2|Leaf.Move" "7|Leaf.Move"); DUMP_AT=8; DUMP_LEN=5 ;;
		end) # 12: evening on the spire, a random limb (each run differs), half speed, slow orbit
			DAY=3; SEED_USE=${SPIRE_SEED:-15161}; PRE="Leaf.Bird 0,Leaf.Move,Leaf.Move"; CALM=1
			STEPS=("${evening[@]}" "7.5|Leaf.Teleport L1 0.6" "8|Leaf.Slomo 0.5" "8.5|Leaf.Shot orbit 16"); DUMP_AT=10; DUMP_LEN=9 ;;
		*) echo "unknown shot '$1' (shots: $SHOTS)" >&2; exit 2 ;;
	esac
	DUMP_AT=${DUMP_AT_OVERRIDE:-$DUMP_AT}; DUMP_LEN=${DUMP_LEN_OVERRIDE:-$DUMP_LEN}
	# Sound-pass marker: three clicks just after the window. (Before it they were lost: the game's first two seconds
	# play no sound, and game time and the clock only run together once the startup hitches are over.)
	MARK_AT=$(python3 -c "print(round($DUMP_AT+$DUMP_LEN+0.6,3))")
	FRAMES=$(python3 -c "print(round($DUMP_LEN*60))")
	local last=0 s t
	for s in "${STEPS[@]}"; do t=${s%%|*}; last=$(python3 -c "print(max($last,$t))"); done
	END=$(python3 -c "print(round(max($DUMP_AT+$DUMP_LEN,$last)+1.5,2))")
	[[ -n "${RUN_FOR:-}" ]] && END=$RUN_FOR   # probes and stills: run this long (game seconds) instead
	return 0
}

# The ExecCmds string. $2: 1 = the frame-dump pass (adds the dump window), 0 = a probe, 2 = the real-time sound pass
# (sound plays although the offscreen game has no focus, three marker clicks after the window, ~60 fps).
MARK_GAP=0.35
build_cmds() {
	local cmds="r.DumpingMovie 0,Leaf.ResetSave,Leaf.OneTree 1" s
	[[ "$2" == 2 ]] && cmds="au.DisableAppVolume 1,t.MaxFPS 60,$cmds"
	(( CALM == 1 )) && cmds+=",Leaf.Tune wind CalmMin 90,Leaf.Tune wind CalmMax 90"
	[[ -n "$POOL" ]] && cmds+=",Leaf.Pool $POOL"
	[[ -n "$PRE" ]] && cmds+=",$PRE"
	cmds+=",Leaf.StartDay $DAY $SEED_USE,Leaf.BuildLine 0,Leaf.PadIcons 0"
	[[ -n "${EXTRA:-}" ]] && cmds+=",$EXTRA"
	(( HUD == 0 )) && cmds+=",Leaf.Delay 1 Leaf.Cinematic 1"
	for s in "${STEPS[@]}"; do cmds+=",Leaf.Delay ${s%%|*} ${s#*|}"; done
	if [[ "$2" == 1 && -n "${STILLS:-}" ]]; then
		local st; for st in $STILLS; do cmds+=",Leaf.Delay $st r.DumpingMovie 1"; done
	elif [[ "$2" == 1 ]]; then
		cmds+=",Leaf.Delay $DUMP_AT r.DumpingMovie $FRAMES"
	fi
	if [[ "$2" == 2 ]]; then
		local k t
		for k in 0 1 2; do t=$(python3 -c "print(round($MARK_AT+$k*$MARK_GAP,3))"); cmds+=",Leaf.Delay $t Leaf.Sound ui_confirm 8"; done   # loud: a rising gust masked the third click at volume 3
		cmds+=",Leaf.QuitAfter $(python3 -c "print(round(max($END,$MARK_AT+2*$MARK_GAP+1.5),2))")"
	else
		cmds+=",Leaf.QuitAfter $END"
	fi
	printf '%s' "$cmds"
}

save_sig() { [[ -e "$SAVE" ]] && md5sum "$SAVE" | cut -d' ' -f1 || echo none; }

# Runs the game once; $1 shot, $2 log name, $3 mode (1 = frame dump, 0 = probe, 2 = real-time sound pass recorded
# from the VB-Audio cable into $4, a Windows path).
run_game() {
	local shot=$1 log=$2 dump=$3 rec=${4:-} cmds bat flags
	cmds=$(build_cmds "$shot" "$dump")
	flags="-game -windowed -ResX=1920 -ResY=1080 -ForceRes -RenderOffscreen -nosplash -unattended -LeafSlot=Video -log=$log"
	if (( dump == 2 )); then
		# The game plays to the Windows default device; Windows routes UnrealEditor.exe to the cable (see the header).
		# A fixed frame rate of 60: every frame is exactly 1/60 s of game time AND the engine waits for the clock, so
		# game time runs with the recording and, as in the frame dump, the slow first frame counts as one frame.
		# (Without it the first frame's seconds fired the early steps at once and left the sim behind them.)
		flags+=" -ini:Engine:[/Script/Engine.Engine]:bUseFixedFrameRate=True -ini:Engine:[/Script/Engine.Engine]:FixedFrameRate=60"
	else
		flags+=" -UseFixedTimeStep -FPS=60"
		(( dump == 1 )) && flags+=" -dumpmovie"
	fi
	bat=$ROOT/Saved/VideoCap/run_$shot.bat
	mkdir -p "$ROOT/Saved/VideoCap"
	# In the sound pass the recorder starts inside the lock, just before the game; the .bat then waits for it to finish.
	{
		printf '@echo off\r\nrem Written by Tools/video_capture.sh for shot %s.\r\n' "$shot"
		(( dump == 2 )) && printf 'start "" /b ffmpeg -hide_banner -loglevel error -y -f dshow -audio_buffer_size 20 -i audio="CABLE Output (VB-Audio Virtual Cable)" -t %s -ac 2 -ar 48000 "%s"\r\n' \
			"$(python3 -c "print(round($END+50))")" "$rec"
		printf '"C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s\\AntTreeGame.uproject" %s -ExecCmds="%s"\r\n' \
			"$ROOT_WIN" "$flags" "$cmds"
		(( dump == 2 )) && printf ':wait\r\ntasklist /fi "imagename eq ffmpeg.exe" | find /i "ffmpeg.exe" >nul && (ping -n 2 127.0.0.1 >nul & goto wait)\r\n'
		true
	} > "$bat"
	local before; before=$(save_sig)
	(cd /mnt/c && timeout $((1200 + FRAMES / 2)) bash "$ROOT/Scripts/unreal-lock.sh" "video-$shot" -- \
		cmd.exe /c "$ROOT_WIN\\Saved\\VideoCap\\run_$shot.bat") >"$ROOT/Saved/VideoCap/run_$shot.lock.log" 2>&1 || true
	if [[ "$(save_sig)" != "$before" ]]; then echo "STOP: the owner's save $SAVE changed during the run." >&2; exit 3; fi
}

# LogLeafline lines with game time (the frame counter in the log wraps at 1000; 60 frames per second).
events() {
	python3 - "$1" <<'PY'
import re, sys
wraps, prev = 0, 0
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.match(r'\[[^\]]*\]\[\s*(\d+)\](LogLeafline: .*)', line.rstrip())
    if not m: continue
    f = int(m.group(1))
    if f < prev: wraps += 1000   # frames only grow, so any drop is a wrap (gaps over 1000 frames would be missed)
    prev = f
    print(f"{(f + wraps) / 60:7.2f} s  {m.group(2)[13:]}")
PY
}

# Finds the three marker clicks in a recording (onsets MARK_GAP apart) and prints the first one's time in seconds.
align() {
	python3 - "$1" "$MARK_GAP" <<'PY'
import array, sys, wave
path, gap = sys.argv[1], float(sys.argv[2])
w = wave.open(path)
sr, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
assert width == 2, 'expected 16-bit PCM'
data = array.array('h', w.readframes(w.getnframes()))
blk = int(sr * 0.002) * ch                      # 2 ms blocks, peak level per block
env = [max(-min(data[i:i + blk]), max(data[i:i + blk])) for i in range(0, len(data) - blk, blk)]
floor = max(200, max(env) // 12)                # an onset is at least a twelfth of the recording's peak
onsets, last = [], -1.0
for i in range(25, len(env)):
    before = max(env[i - 25:i - 2])             # the 46 ms before
    if env[i] > floor and env[i] > 2.0 * before + floor / 4:
        t = i * 0.002
        if t - last > 0.08: onsets.append(t)
        last = t
tol = 0.02
for t in reversed(onsets):                      # the markers come last in the run
    if any(abs(u - t - gap) < tol for u in onsets) and any(abs(u - t - 2 * gap) < tol for u in onsets):
        print(f'{t:.4f}'); sys.exit(0)
print('onsets: ' + ' '.join(f'{t:.3f}' for t in onsets[:40]), file=sys.stderr)
sys.exit(1)
PY
}

# The real-time pass of a shot for its sound: the game plays into the VB-Audio cable, ffmpeg records the cable, and the
# marker clicks place the shot's window in the recording. Game time runs with the wall clock in this pass, and every
# scripted step happens at the same game time as in the frame dump, so the sound lands on the picture.
sound_pass() {
	local shot=$1 name=$2 adir=$ROOT/Saved/VideoCap/audio t0 start got
	mkdir -p "$adir"; rm -f "$adir/$name.rec.wav"
	echo "  sound pass (real time, recorded from the VB-Audio cable)"
	run_game "$shot" "VideoSound-$shot.log" 2 "$ROOT_WIN\\Saved\\VideoCap\\audio\\$name.rec.wav"
	[[ -s "$adir/$name.rec.wav" ]] || { echo "  sound pass: nothing recorded (Saved/VideoCap/run_$shot.lock.log)" >&2; return 1; }
	t0=$(align "$adir/$name.rec.wav") || { echo "  sound pass: marker clicks not found; recording kept: $adir/$name.rec.wav" >&2; return 1; }
	start=$(python3 -c "print(round($t0+$DUMP_AT-$MARK_AT,4))")
	ffmpeg -hide_banner -loglevel error -y -i "$adir/$name.rec.wav" -ss "$start" -t "$DUMP_LEN" -c:a pcm_s16le "$adir/$name.wav"
	got=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$adir/$name.wav")
	python3 -c "import sys; sys.exit(0 if abs($got-$DUMP_LEN) < 0.05 else 1)" || { echo "  sound pass: only $got s of sound (the recording ended early)" >&2; return 1; }
	ffmpeg -hide_banner -loglevel error -y -i "$OUT/$name.mp4" -i "$adir/$name.wav" -map 0:v -map 1:a -c:v copy -c:a aac -b:a 256k \
		-shortest -movflags +faststart "$OUT/$name.tmp.mp4" && mv "$OUT/$name.tmp.mp4" "$OUT/$name.mp4"
	echo "  sound: markers at $t0 s in the recording; the clip's sound is also in Saved/VideoCap/audio/$name.wav"
	# Did game time run with the clock through the window? Compare the log's clock with its frame numbers from the
	# last line before the window to the first marker. More than a frame or two of drift moves sounds off the picture.
	python3 - "$ROOT/Saved/Logs/VideoSound-$shot.log" "$DUMP_AT" "$MARK_AT" <<'PY'
import re, sys
from datetime import datetime
path, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
rows, wraps, prev = [], 0, 0
for line in open(path, encoding='utf-8', errors='replace'):
    m = re.match(r'\[(\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d:\d{3})\]\[\s*(\d+)\]', line)
    if not m: continue
    f = int(m.group(2))
    if f < prev: wraps += 1000
    prev = f
    rows.append(((f + wraps) / 60, datetime.strptime(m.group(1), '%Y.%m.%d-%H.%M.%S:%f')))
before = [r for r in rows if r[0] <= t0]
after = [r for r in rows if r[0] >= t1 - 0.01]
if before and after:
    a, b = before[-1], after[0]
    game, wall = b[0] - a[0], (b[1] - a[1]).total_seconds()
    drift = (wall - game) * 1000
    print(f"  sound: {game:.2f} s of game time took {wall:.2f} s ({drift:+.0f} ms drift from {a[0]:.1f} s to the markers)"
          + ("  <- CHECK: sounds may sit off the picture" if abs(drift) > 50 else ""))
PY
}

capture() {
	local shot=$1 label=${2:-} n=1
	shot_def "$shot"
	mkdir -p "$OUT"
	if [[ -z "$label" ]]; then while [[ -e "$OUT/$shot-take$n.mp4" ]]; do n=$((n + 1)); done; label=take$n; fi
	local name=$shot-$label
	local free_mb need_mb; free_mb=$(df -m --output=avail /mnt/d | tail -1); need_mb=$((FRAMES * 4 + 1500))
	(( free_mb > need_mb )) || { echo "STOP: D: has $free_mb MB free, this shot needs about $need_mb MB of frames." >&2; exit 4; }
	local old; old=$(find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' 2>/dev/null | wc -l)
	if (( old > 0 )); then echo "Removing $old leftover MovieFrame PNGs from an earlier dump."; find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' -delete; fi
	echo "Filming $name: $DUMP_LEN s from game time $DUMP_AT ($FRAMES frames, about $((FRAMES * 10 / 45 / 60 + 1)) min of dumping after startup)."
	run_game "$shot" "VideoCapture-$shot.log" 1
	local log=$ROOT/Saved/Logs/VideoCapture-$shot.log
	mapfile -t frames < <(find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' | sort)
	if (( ${#frames[@]} < FRAMES )); then
		echo "STOP: only ${#frames[@]} of $FRAMES frames were written. Lock output: Saved/VideoCap/run_$shot.lock.log; game log: $log" >&2
		exit 5
	fi
	local first=$(( ${#frames[@]} - FRAMES )) size
	size=$(python3 -c "import struct,sys; h=open(sys.argv[1],'rb').read(24); print('%dx%d' % struct.unpack('>II', h[16:24]))" "${frames[$first]}")
	[[ "$size" == 1920x1080 ]] || { echo "STOP: frames are $size, not 1920x1080 (frames kept in $FRAMEDIR)." >&2; exit 6; }
	local startnum; startnum=$(basename "${frames[$first]}" .png); startnum=$((10#${startnum#MovieFrame}))
	ffmpeg -hide_banner -loglevel error -y -framerate 60 -start_number "$startnum" -i "$FRAMEDIR/MovieFrame%05d.png" -frames:v "$FRAMES" \
		-c:v libx264 -preset medium -crf 14 -pix_fmt yuv420p -movflags +faststart "$OUT/$name.mp4"
	ffmpeg -hide_banner -loglevel error -y -i "$OUT/$name.mp4" -vf "select='not(mod(n\,$(( FRAMES / 6 ))))',scale=640:-1,tile=3x2" \
		-frames:v 1 "$OUT/$name.jpg"
	find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' -delete
	events "$log" > "$OUT/$name.log"
	if [[ "${AUDIO:-0}" == 1 ]]; then ( sound_pass "$shot" "$name" ) || echo "  (no sound for $name; the silent clip stays)"; fi
	local dur; dur=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$OUT/$name.mp4")
	echo "Done: Saved/VideoCap/shots/$name.mp4 ($dur s, 1920x1080, 60 fps), contact sheet $name.jpg, events $name.log"
	grep -E 'Shot .* ends|clipped frames [1-9]|Warning|no cuttable|could not|ignored' "$OUT/$name.log" | sed 's/^/  /' || true
}

reel() {
	local list=() s f tmp
	for s in $SHOTS; do for f in "$OUT/$s"-*.mp4; do [[ -e "$f" ]] && list+=("$f"); done; done
	(( ${#list[@]} )) || { echo "No takes in $OUT yet." >&2; exit 2; }
	tmp=$(mktemp -d); local concat=$tmp/list.txt i=0
	for f in "${list[@]}"; do
		local label; label=$(basename "$f" .mp4)
		ffmpeg -hide_banner -loglevel error -y -i "$f" -vf "drawtext=fontfile='$FONT':text='$label':x=40:y=40:fontsize=42:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=14" \
			-c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p -r 60 "$tmp/$i.mp4"
		echo "file '$tmp/$i.mp4'" >> "$concat"; i=$((i + 1))
	done
	ffmpeg -hide_banner -loglevel error -y -f concat -safe 0 -i "$concat" -c copy -movflags +faststart "$ROOT/Saved/VideoCap/reel.mp4"
	rm -rf "$tmp"
	echo "Reel: Saved/VideoCap/reel.mp4 (${#list[@]} takes, labelled)"
}

case ${1:-} in
	list) for s in $SHOTS; do shot_def "$s"; printf '%-8s day %s  %s  dump %s s from %s s\n' "$s" "$DAY" "$( (( HUD )) && echo 'HUD on ' || echo 'no HUD ')" "$DUMP_LEN" "$DUMP_AT"; done ;;
	--dry-run) shot_def "${2:?shot}"; build_cmds "$2" 1; echo ;;
	--probe)
		shot_def "${2:?shot}"
		run_game "$2" "VideoProbe-$2.log" 0
		events "$ROOT/Saved/Logs/VideoProbe-$2.log" | grep -v -E 'TreeSkeleton|TreeField|TreeMesh|TreeSkirts|LeafSet|Grove|Skin:' ;;
	--stills) # --stills <shot> "<t1 t2 ...>" [label]: one frame at each game time, as one labelled sheet (fast: no
		# continuous dump), to find a shot's framing and timing before filming it
		shot_def "${2:?shot}"; STILLS=$(for t in ${3:?times}; do echo "$t"; done | sort -n | tr '\n' ' ')
		END=${RUN_FOR:-$(python3 -c "print(max(float(t) for t in '$STILLS'.split())+1)")}
		find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' -delete 2>/dev/null || true
		run_game "$2" "VideoStills-$2.log" 1
		mapfile -t frames < <(find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' | sort)
		read -ra times <<< "$STILLS"
		# The engine dumps a frame or two at startup before "r.DumpingMovie 0" takes effect: keep the last ones.
		(( ${#frames[@]} > ${#times[@]} )) && frames=("${frames[@]: -${#times[@]}}")
		(( ${#frames[@]} == ${#times[@]} )) || echo "note: ${#frames[@]} frames for ${#times[@]} times (labels may be off)" >&2
		(( ${#frames[@]} )) || { echo "no frames (Saved/VideoCap/run_$2.lock.log)" >&2; exit 5; }
		sdir=$ROOT/Saved/VideoCap/stills; mkdir -p "$sdir"; tmp=$(mktemp -d)
		for i in "${!frames[@]}"; do
			ffmpeg -hide_banner -loglevel error -y -i "${frames[$i]}" -vf "scale=640:-1,drawtext=fontfile='$FONT':text='$2 ${times[$i]:-?} s':x=8:y=8:fontsize=22:fontcolor=yellow:box=1:boxcolor=black@0.6" "$tmp/$(printf '%03d' "$i").png"
		done
		cols=4; rows=$(( (${#frames[@]} + cols - 1) / cols ))
		ffmpeg -hide_banner -loglevel error -y -i "$tmp/%03d.png" -vf "tile=${cols}x${rows}" -frames:v 1 "$sdir/$2-${4:-stills}.jpg"
		rm -rf "$tmp"; find "$FRAMEDIR" -maxdepth 1 -name 'MovieFrame*.png' -delete
		events "$ROOT/Saved/Logs/VideoStills-$2.log" > "$sdir/$2-${4:-stills}.log"
		echo "Stills: Saved/VideoCap/stills/$2-${4:-stills}.jpg (${#frames[@]} frames), events in the .log beside it" ;;
	--sound) # add sound to an existing take: --sound <shot> <take name, e.g. cut-take4>
		shot_def "${2:?shot}"; [[ -e "$OUT/${3:?take name}.mp4" ]] || { echo "no $OUT/$3.mp4" >&2; exit 2; }
		sound_pass "$2" "$3" ;;
	reel) reel ;;
	all) for s in $SHOTS_ALL; do ( capture "$s" ) || echo "($s failed, going on)"; done ;;
	""|-h|--help) sed -n '2,24p' "$0" ;;
	*) DUMP_AT_OVERRIDE=${DUMP_AT:-} DUMP_LEN_OVERRIDE=${DUMP_LEN:-} capture "$1" "${2:-}" ;;
esac
