#!/usr/bin/env bash
# Reference screenshots for the look (lighting, fog, canopy), from the editor binaries without packaging. Three fixed
# views on one seed: up the trunk into the crown (framed like concept image 01), in the canopy, and out from the
# trunk base over the ground. Each set runs with hardware ray tracing and with -noraytracing (as on a GPU without it).
# Usage: Tools/ref_shots.sh <label>   ->  Saved/RefShots/<label>-<view>-<rt|nort>.png
#   ONLY=trunk|canopy|base takes one view; NO_NORT=1 skips the -noraytracing runs.
#   LOOK="Leaf.Tune look FogDensity 0.05,Leaf.Tune look ..." tries look values without a rebuild.
# Runs through the Unreal lock; the editor must be closed. Takes about two minutes.
set -u
ROOT=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: ref_shots.sh <label>}
SEED=${SEED:-517192}
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
mkdir -p "$OUT" "$SHOTDIR"

# view name, placement commands (comma separated, no spaces after commas)
VIEWS=(
	"trunk|Leaf.Teleport 0 ${TRUNK_T:-0.3},Leaf.Delay 2 Leaf.Aim up ${TRUNK_PITCH:-25}"
	"canopy|Leaf.Teleport ${CANOPY_BRANCH:-400} 0.5,Leaf.Delay 2 Leaf.Aim ${CANOPY_HEADING:-170} -10"
	"base|Leaf.Delay 2 Leaf.Aim ${BASE_HEADING:-170} ${BASE_PITCH:--10}"
)

shoot() {
	local mode=$1 extra=$2 view=$3 place=$4
	local marker=$OUT/.marker
	touch "$marker"
	(cd /mnt/c && timeout 240 "$ROOT/Scripts/unreal-lock.sh" "refshot-$LABEL" -- cmd.exe /c "D:\\UnrealProjects\\AntTreeGame\\Scripts\\RunGame.bat" RefShot.log \
		"Leaf.NewDay $SEED${LOOK:+,$LOOK},$place,Leaf.Delay 12 shot showui,Leaf.QuitAfter 15" "$extra" >/dev/null 2>&1)
	local shot
	shot=$(find "$SHOTDIR" -maxdepth 1 -name 'ScreenShot*.png' -newer "$marker" | sort | tail -1)
	if [ -z "$shot" ]; then
		echo "$view-$mode: no screenshot (see Saved/Logs/RefShot.log)"
		return
	fi
	cp "$shot" "$OUT/$LABEL-$view-$mode.png"
	echo "$OUT/$LABEL-$view-$mode.png"
}

for v in "${VIEWS[@]}"; do
	name=${v%%|*}
	place=${v#*|}
	[ -n "${ONLY:-}" ] && [ "$ONLY" != "$name" ] && continue
	shoot rt "" "$name" "$place"
	[ -z "${NO_NORT:-}" ] && shoot nort "-noraytracing" "$name" "$place"
done
