#!/usr/bin/env bash
# One standalone run that takes the ant-look and forest-floor screenshots of docs/briefs/ant-and-ground.md (acceptance
# tests 1 and 2 partly) on seed 15161, in the RefShot slot. Runs this tree's binaries (ROOT) through the Unreal lock.
# Usage: ROOT=/mnt/d/UnrealProjects/leafline-floor Tools/floor_shots.sh <label> [extra game args]
# Files: Saved/RefShots/<label>-<view>.png, taken in the order of VIEWS below.
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: floor_shots.sh <label> [extra args]}
EXTRA=${2:-}
SEED=${SEED:-15161}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/FloorShots.log
mkdir -p "$SHOTDIR" "$OUT"
# time|command ; a HighResShot line is named by the next entry of VIEWS.
STEPS=(
 "0|Leaf.NewDay $SEED"
 "6|Leaf.Goto -547 1550 7 -165 -7"
 "10|HighResShot 1" "11|Leaf.AntLook 2" "13|HighResShot 1" "14|Leaf.AntLook 0" "16|HighResShot 1" "17|Leaf.AntLook 1"
 "18|Leaf.Floor 0" "20|HighResShot 1" "21|Leaf.Floor 1"
 "22|Leaf.Goto 3600 0 0 0 -4" "26|HighResShot 1"
 "27|Leaf.Goto 6000 0 0 0 -3" "31|HighResShot 1"
 "32|Leaf.Goto 5000 0 0 180 -3" "36|HighResShot 1"
 "37|Leaf.Teleport 0 0.3" "39|Leaf.Aim up 25" "42|HighResShot 1" "43|Leaf.AntLook 0" "45|HighResShot 1" "46|Leaf.AntLook 2" "48|HighResShot 1" "49|Leaf.AntLook 1"
 "50|Leaf.FaceWind sheltered" "52|Leaf.Aim 170 30" "55|HighResShot 1" "56|Leaf.AntLook 0" "58|HighResShot 1" "59|Leaf.AntLook 2" "61|HighResShot 1" "62|Leaf.AntLook 1"
 "63|Leaf.Goto -547 1550 7 -70 -7" "67|HighResShot 1" "68|Leaf.Goto -547 1550 7 120 -7" "72|HighResShot 1"
 "75|Leaf.QuitAfter 1"
)
VIEWS=(nest-A nest-B nest-old nest-nofloor floor-flat floor-outer floor-inward trunk-A trunk-old trunk-B shade-A shade-old shade-B nest-trunkward nest-left)
CMDS=""
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	if [ "$t" = 0 ]; then CMDS="$c"; else CMDS="$CMDS,Leaf.Delay $t $c"; fi
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" floorshots.XXXX)
sleep 1
(cd /mnt/c && timeout 400 "$MAIN/Scripts/unreal-lock.sh" floor-shots -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" FloorShots.log "$CMDS" "-LeafSlot=RefShot $EXTRA" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL)"
grep -a "Forest floor\|Leaf.AntLook\|Leaf.Floor\|Error\|Warning: AntLook" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -30
