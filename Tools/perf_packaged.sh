#!/usr/bin/env bash
# Frame rates of the packaged build at 1920x1080: on the trunk and in the canopy, with hardware ray tracing and with it
# unavailable (-noraytracing makes the D3D12 adapter report no ray tracing, like a GPU without it), plus a DX11 start.
# Each run walks the ant with Leaf.AutoMove and a wandering camera, captures FRAMES frames with the CSV profiler, then
# the game quits by itself. Runs go through the Unreal lock, so close the editor first, and nobody may be playing the
# packaged build meanwhile (its save and settings are backed up and restored). Takes about 5 minutes.
set -u
ROOT=/mnt/d/UnrealProjects/AntTreeGame
EXE='D:\UnrealProjects\AntTreeGame\Build\Windows\AntTreeGame.exe'
SAVED=$ROOT/Build/Windows/AntTreeGame/Saved
CSVDIR=$SAVED/Profiling/CSV
FRAMES=${FRAMES:-2400}
SEED=${SEED:-517192}

run() {
	local name=$1 extra=$2 place=$3
	local before after
	before=$(ls -t "$CSVDIR"/*.csv 2>/dev/null | head -1)
	(cd /mnt/c && timeout 400 "$ROOT/Scripts/unreal-lock.sh" "perf-$name" -- cmd.exe /c "$EXE" -windowed -ResX=1920 -ResY=1080 $extra \
		"-ExecCmds=t.MaxFPS 0,r.VSync 0,Leaf.NewDay $SEED,$place,Leaf.AutoMove 1,Leaf.AutoTurn 20 wander,CsvProfile EXITONCOMPLETION,CsvProfile FRAMES=$FRAMES,Leaf.QuitAfter 300" \
		>/dev/null 2>&1)
	local log=$SAVED/Logs/AntTreeGame.log
	local rhi rt
	rhi=$(grep -a -m1 -o "Using Highest Feature Level of [^\r]*" "$log" | head -1)
	rt=$(grep -a -m1 -o "Ray tracing is [^\r]*" "$log" | head -1)
	after=$(ls -t "$CSVDIR"/*.csv 2>/dev/null | head -1)
	if [ -z "$after" ] || [ "$after" = "$before" ]; then
		echo "$name: no capture written (game failed to start or quit early). RHI: ${rhi:-?}"
		grep -a -m3 "Error\|Fatal" "$log" | grep -v LogAutomationTest | cut -c 1-200
		return
	fi
	python3 "$ROOT/Tools/csv_fps.py" --label "$name" "$after"
	echo "    RHI: ${rhi:-?}; ${rt:-no ray tracing line in log}"
}

# The runs share the packaged game's Saved folder with whoever plays it: keep their save and settings untouched.
BACKUP=$ROOT/Build/perf-backup-$(date +%Y%m%d-%H%M%S)
mkdir -p "$BACKUP"
for d in SaveGames Config; do [ -d "$SAVED/$d" ] && cp -a "$SAVED/$d" "$BACKUP/"; done
restore() {
	for d in SaveGames Config; do
		rm -rf "${SAVED:?}/$d"
		[ -d "$BACKUP/$d" ] && cp -a "$BACKUP/$d" "$SAVED/"
	done
	echo "Restored SaveGames and Config from $BACKUP"
}
trap restore EXIT

run trunk-rt "" "Leaf.Teleport 0 0.3"
run canopy-rt "" "Leaf.Teleport L3 0.5"
run trunk-nort "-noraytracing" "Leaf.Teleport 0 0.3"
run canopy-nort "-noraytracing" "Leaf.Teleport L3 0.5"
FRAMES=900 run trunk-dx11 "-dx11" "Leaf.Teleport 0 0.3"
