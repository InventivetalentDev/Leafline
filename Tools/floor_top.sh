#!/usr/bin/env bash
# Top-down shots of the whole forest floor and the nest from four directions, seed 15161, RefShot slot (before/after
# comparisons of the floor's placement). Usage: ROOT=<tree> Tools/floor_top.sh <label> [seed]
# Files: Saved/RefShots/<label>-<view>.png
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: floor_top.sh <label> [seed]}
SEED=${2:-15161}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/FloorTop.log
mkdir -p "$SHOTDIR" "$OUT"
STEPS=(
 "0|Leaf.NewDay $SEED" "4|Leaf.Cinematic 1"
 "5|Leaf.Tune look FogDensity 0.004" "6|Leaf.TopDown 40000 50" "10|HighResShot 1"
 "11|Leaf.TopDown 15000 60" "15|HighResShot 1"
 "16|Leaf.TopDown 6000 70" "20|HighResShot 1"
 "21|Leaf.TopDown 0" "22|Leaf.Tune look FogDensity 0.06"
 "23|Leaf.Goto -547 1550 7 -165 -7" "27|HighResShot 1"
 "28|Leaf.Goto -547 1550 7 -70 -7" "32|HighResShot 1"
 "33|Leaf.Goto -547 1550 7 20 -7" "37|HighResShot 1"
 "38|Leaf.Goto -547 1550 7 120 -7" "42|HighResShot 1"
 "45|Leaf.QuitAfter 1"
)
VIEWS=(top-400m top-150m top-60m nest-away nest-trunk nest-east nest-west)
CMDS=""
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	if [ -z "$CMDS" ]; then CMDS="$c"; else CMDS="$CMDS,Leaf.Delay $t $c"; fi
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" floortop.XXXX)
sleep 1
(cd /mnt/c && timeout 400 "$MAIN/Scripts/unreal-lock.sh" floor-top -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" FloorTop.log "$CMDS" "-LeafSlot=RefShot" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL)"
grep -a "Forest floor:\|Leaf.TopDown" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-300
