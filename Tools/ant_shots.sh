#!/usr/bin/env bash
# One standalone run: the ant with recruits (acceptance test 3) and the wind map (test 2), on seed 517192 in the RefShot
# slot, pool 4, three lines. Usage: ROOT=<tree> Tools/ant_shots.sh <label> [extra args]. Files Saved/RefShots/<label>-<view>.png
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: ant_shots.sh <label> [extra args]}
EXTRA=${2:-}
SEED=${SEED:-517192}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/AntShots.log
mkdir -p "$SHOTDIR" "$OUT"
STEPS=(
 "0|Leaf.ResetSave" "0.1|Leaf.Pool 4" "0.2|Leaf.StartDay 1 $SEED"
 "3|Leaf.TestLines 3 0.5 any 5"
 "7|HighResShot 1" "12|HighResShot 1"
 "14|Leaf.Teleport 0 0.12" "16|Leaf.Aim up 20" "22|HighResShot 1" "30|HighResShot 1"
 "32|Leaf.WindMap 1" "34|HighResShot 1" "35|Leaf.WindMap 0"
 "36|Leaf.AntLook 0" "38|HighResShot 1"
 "40|Leaf.AntLook 1" "42|Leaf.Goto 0 0 0 180 -10" "46|HighResShot 1"
 "48|Leaf.QuitAfter 1"
)
VIEWS=(nest-recruits-1 nest-recruits-2 trunk-recruits-1 trunk-recruits-2 windmap trunk-recruits-old base-recruits)
CMDS=""
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	if [ "$t" = 0 ]; then CMDS="$c"; else CMDS="$CMDS,Leaf.Delay $t $c"; fi
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" antshots.XXXX)
sleep 1
(cd /mnt/c && timeout 400 "$MAIN/Scripts/unreal-lock.sh" ant-shots -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" AntShots.log "$CMDS" "-LeafSlot=RefShot $EXTRA" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL)"
grep -a "Leaf.AntLook\|Leaf.WindMap\|Recruit\|Wind map\|AntLook" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -30
