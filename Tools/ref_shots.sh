#!/usr/bin/env bash
# Reference screenshots for the look (lighting, fog, canopy, cutaway), from the editor binaries without packaging.
# Fixed views on one seed (docs/briefs/look-pass.md, part 0):
#   trunk     up the trunk into the crown (framed like concept image 01)
#   canopy    in the canopy
#   base      out from the trunk base over the ground
#   junction  limb 21 (92 cm radius) where it leaves the trunk, camera looking toward the trunk. The brief asked for
#             a 45 degree look up; on this seed the black wedge shows at -80 (looking down onto the junction), so that
#             is the default; the +60 end of a PITCHES sweep shows the hard-edged view into the limb
#   sunward   on the sun-facing side of the trunk in the first shift, looking toward the sun (the amber wash)
#   bough     (sweeps only, not in the default set) bough 55 at 30 cm radius
# Each set runs with hardware ray tracing and with -noraytracing (as on a GPU without it).
# Usage: Tools/ref_shots.sh <label>   ->  Saved/RefShots/<label>-<view>-<rt|nort>.png
#   ONLY="junction sunward" takes those views; NO_NORT=1 skips the -noraytracing runs.
#   LOOK="Leaf.Tune look FogDensity 0.05,Leaf.Tune cam ThickWoodRadius 40" runs any commands before placement:
#     tries tuning values without a rebuild.
#   SHIFT=2|3 takes the views in a later shift of day 1 (2 noon, 3 evening; 1, the default, is morning).
#   PITCHES="-80 -45 -10 25 60" sweeps the camera pitch at each view taken (files <label>-<view>-p<pitch>-<mode>.png).
#   The views move with TRUNK_T, TRUNK_PITCH, CANOPY_BRANCH, CANOPY_HEADING, BASE_HEADING, BASE_PITCH,
#   JUNCTION_BRANCH, JUNCTION_T, JUNCTION_HEADING, JUNCTION_PITCH, SUN_T, SUN_HEADING, SUN_PITCH,
#   BOUGH_BRANCH, BOUGH_T, BOUGH_HEADING, BOUGH_PITCH.
# Tools/ref_shots.sh --probe "<commands>" [seconds]  runs the game once with those commands (no screenshot) and
#   prints the LogLeafline lines, e.g. to read Leaf.LightInfo or a teleport's branch radius.
# Every run uses the save slot RefShot (-LeafSlot=RefShot), never the owner's colony (Saved/SaveGames/Leafline.sav);
# the script stops if that file changes while it runs.
# Runs through the Unreal lock; the editor must be closed. About 25 s per shot.
set -u
ROOT=/mnt/d/UnrealProjects/AntTreeGame
SEED=${SEED:-517192}
SLOT=RefShot
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/RefShot.log
SAVE=$ROOT/Saved/SaveGames/Leafline.sav
mkdir -p "$OUT" "$SHOTDIR"

save_sig() { [ -e "$SAVE" ] && md5sum "$SAVE" | cut -d' ' -f1 || echo none; }
SAVE_SIG=$(save_sig)
check_save() {
	if [ "$(save_sig)" != "$SAVE_SIG" ]; then
		echo "STOP: $SAVE changed during the run (the owner's colony). Nothing else was run." >&2
		exit 3
	fi
}

# Starts the game with one ExecCmds string through the Unreal lock and waits for it to quit.
run_game() {
	local holder=$1 cmds=$2 extra=$3
	(cd /mnt/c && timeout 240 "$ROOT/Scripts/unreal-lock.sh" "$holder" -- cmd.exe /c "D:\\UnrealProjects\\AntTreeGame\\Scripts\\RunGame.bat" RefShot.log \
		"$cmds" "-LeafSlot=$SLOT $extra" >/dev/null 2>&1)
	check_save
}

if [ "${1:-}" = "--probe" ]; then
	CMDS=${2:?usage: ref_shots.sh --probe "<commands>" [seconds]}
	SECS=${3:-15}
	run_game refshot-probe "Leaf.NewDay $SEED,$CMDS,Leaf.QuitAfter $SECS" "${EXTRA:-}"
	grep -a "LogLeafline" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//'
	exit 0
fi

LABEL=${1:?usage: ref_shots.sh <label> | --probe "<commands>" [seconds]}

# The placement commands of a view (comma separated, no spaces after commas); $2 overrides its pitch.
place() {
	case $1 in
		trunk) echo "Leaf.Teleport 0 ${TRUNK_T:-0.3},Leaf.Delay 2 Leaf.Aim up ${2:-${TRUNK_PITCH:-25}}" ;;
		canopy) echo "Leaf.Teleport ${CANOPY_BRANCH:-400} 0.5,Leaf.Delay 2 Leaf.Aim ${CANOPY_HEADING:-170} ${2:--10}" ;;
		base) echo "Leaf.Delay 2 Leaf.Aim ${BASE_HEADING:-170} ${2:-${BASE_PITCH:--10}}" ;;
		junction) echo "Leaf.Teleport ${JUNCTION_BRANCH:-21} ${JUNCTION_T:-0.08},Leaf.Delay 2 Leaf.Aim ${JUNCTION_HEADING:-trunk} ${2:-${JUNCTION_PITCH:--80}}" ;;
		sunward) echo "Leaf.FaceWind sheltered,Leaf.Delay 2 Leaf.Aim ${SUN_HEADING:-170} ${2:-${SUN_PITCH:-30}}" ;;
		bough) echo "Leaf.Teleport ${BOUGH_BRANCH:-55} ${BOUGH_T:-0.37},Leaf.Delay 2 Leaf.Aim ${BOUGH_HEADING:-trunk} ${2:-${BOUGH_PITCH:-25}}" ;;
	esac
}

# Delays every command of a comma list by $1 seconds (Leaf.Delay cannot hold a comma list itself).
delay_cmds() {
	local off=$1 out="" c rest t
	IFS=',' read -ra parts <<< "$2"
	for c in "${parts[@]}"; do
		if [[ $c == "Leaf.Delay "* ]]; then
			rest=${c#Leaf.Delay }
			t=${rest%% *}
			c="Leaf.Delay $(awk "BEGIN{print $t + $off}") ${rest#* }"
		else
			c="Leaf.Delay $off $c"
		fi
		out="$out,$c"
	done
	echo "${out#,}"
}

shoot() {
	local mode=$1 extra=$2 name=$3 place=$4
	local marker=$OUT/.marker
	local pre="" off=0 k
	# SHIFT=n: send n-1 workers home with a fragment first, so the sun stands where it does in shift n (of 3 on day 1:
	# 1 morning, 2 noon, 3 evening); the next worker comes out 1.4 s after each send, then the view is placed.
	for ((k = 1; k < ${SHIFT:-1}; ++k)); do
		pre="$pre,Leaf.Delay $off.1 Leaf.Carry 1,Leaf.Delay $off.3 Leaf.SendHome"
		off=$((off + 2))
	done
	[ "$off" -gt 0 ] && place="${pre#,},$(delay_cmds "$off" "$place")"
	touch "$marker"
	run_game "refshot-$LABEL" "Leaf.NewDay $SEED${LOOK:+,$LOOK},$place,Leaf.Delay $((12 + off)) shot showui,Leaf.QuitAfter $((15 + off))" "$extra"
	local shot
	shot=$(find "$SHOTDIR" -maxdepth 1 -name 'ScreenShot*.png' -newer "$marker" | sort | tail -1)
	if [ -z "$shot" ]; then
		echo "$name-$mode: no screenshot (see Saved/Logs/RefShot.log)"
		return
	fi
	cp "$shot" "$OUT/$LABEL-$name-$mode.png"
	echo "$OUT/$LABEL-$name-$mode.png"
}

for view in ${ONLY:-trunk canopy base junction sunward}; do
	for pitch in ${PITCHES:-default}; do
		if [ "$pitch" = default ]; then
			name=$view
			cmds=$(place "$view")
		else
			name=$view-p$pitch
			cmds=$(place "$view" "$pitch")
		fi
		[ -z "$cmds" ] && { echo "unknown view $view"; continue; }
		shoot rt "" "$name" "$cmds"
		[ -z "${NO_NORT:-}" ] && shoot nort "-noraytracing" "$name" "$cmds"
	done
done
