#!/usr/bin/env bash
# Grass sway: the same cross-wind view (the day-1 wind of seed 15161 blows toward yaw 57) at calm, then in a gust (Leaf.Gust), seed 15161, RefShot slot, HUD hidden.
# Usage: ROOT=<tree> Tools/grass_sway_shots.sh <label>   -> Saved/RefShots/<label>-calm-1/2.png, <label>-gust-1/2/3.png
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: grass_sway_shots.sh <label>}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/GrassSway.log
mkdir -p "$SHOTDIR" "$OUT"
STEPS=(
 "0|Leaf.NewDay 15161" "4|Leaf.Cinematic 1" "5|Leaf.Goto -547 1550 7 140 -4"
 "9|HighResShot 1" "9.6|HighResShot 1"
 "11|Leaf.Gust 1" "17|HighResShot 1" "17.5|HighResShot 1" "18|HighResShot 1"
 "22|Leaf.QuitAfter 1"
)
VIEWS=(calm-1 calm-2 gust-1 gust-2 gust-3)
CMDS=""
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	if [ -z "$CMDS" ]; then CMDS="$c"; else CMDS="$CMDS,Leaf.Delay $t $c"; fi
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" grasssway.XXXX)
sleep 1
(cd /mnt/c && timeout 300 "$MAIN/Scripts/unreal-lock.sh" grass-sway -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" GrassSway.log "$CMDS" "-LeafSlot=RefShot" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL)"
grep -a "grass material\|Wind\b.*Gust\|Leaf.Gust" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -6
