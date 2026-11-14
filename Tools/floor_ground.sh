#!/usr/bin/env bash
# Ground-level views of the forest floor, as the player sees it: the nest camera in four directions (Leaf.Aim from the
# day-start position, so any SEED works) and, for seed 15161 only, walks out 25 m from the nest in three (fixed Leaf.Goto points). Seed 15161 (the owner's tree), RefShot slot, HUD hidden. Usage: ROOT=<tree> Tools/floor_ground.sh <label> [extra args]
# Files: Saved/RefShots/<label>-<view>.png
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: floor_ground.sh <label> [extra args]}
EXTRA=${2:-}
SEED=${SEED:-15161}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/FloorGround.log
mkdir -p "$SHOTDIR" "$OUT"
# POSE=goto stands the ant beside the nest (seed 15161, the owner's first nest pose) instead of on the entrance.
if [ "${POSE:-aim}" = goto ]; then P="Leaf.Goto -547 1550 7"; else P="Leaf.Aim"; fi
STEPS=(
 "0|Leaf.NewDay $SEED" "4|Leaf.Cinematic 1"
 "6|$P -165 -7" "10|HighResShot 1"
 "11|$P -100 -7" "15|HighResShot 1"
 "16|$P 20 -7" "20|HighResShot 1"
 "21|$P 120 -7" "25|HighResShot 1"
 "26|Leaf.Goto -2962 903 0 -165 -4" "30|HighResShot 1"
 "31|Leaf.Goto 1802 2405 0 20 -4" "35|HighResShot 1"
 "36|Leaf.Goto -1797 3715 0 120 -4" "40|HighResShot 1"
 "43|Leaf.QuitAfter 1"
)
VIEWS=(nest-away nest-south nest-east nest-west walk-away walk-east walk-west)
CMDS=""
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	if [ -z "$CMDS" ]; then CMDS="$c"; else CMDS="$CMDS,Leaf.Delay $t $c"; fi
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" floorground.XXXX)
sleep 1
(cd /mnt/c && timeout 400 "$MAIN/Scripts/unreal-lock.sh" floor-ground -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" FloorGround.log "$CMDS" "-LeafSlot=RefShot $EXTRA" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL)"
grep -a "Forest floor" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-300
