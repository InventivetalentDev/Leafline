#!/usr/bin/env bash
# The grass material's look: the owner's backlit pose, the same spot looking the other way (sunlit side), two nest views and
# the view from 60 m up a limb (docs/briefs/floor-from-above.md). Seed 675856 (the old giant), RefShot slot, HUD hidden.
# Usage: ROOT=<tree> Tools/grass_look_shots.sh <label>   -> Saved/RefShots/<label>-<view>.png
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: grass_look_shots.sh <label>}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/GrassLook.log
mkdir -p "$SHOTDIR" "$OUT"
STEPS=(
 "0|Leaf.NewDay 675856" "4|Leaf.Cinematic 1"
 "5|Leaf.Goto 1204 3148 2 120 -6" "9|HighResShot 1"
 "10|Leaf.Goto 1204 3148 2 -60 -6" "14|HighResShot 1"
 "15|Leaf.Aim 20 -7" "17|HighResShot 1"
 "18|Leaf.Aim 120 -7" "20|HighResShot 1"
 "21|Leaf.Goto 504 1008 5966 -37 -14" "26|HighResShot 1"
 "29|Leaf.QuitAfter 1"
)
VIEWS=(backlit sunlit nest-east nest-west above)
CMDS=""
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	if [ -z "$CMDS" ]; then CMDS="$c"; else CMDS="$CMDS,Leaf.Delay $t $c"; fi
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" grasslook.XXXX)
sleep 1
(cd /mnt/c && timeout 300 "$MAIN/Scripts/unreal-lock.sh" grass-look -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" GrassLook.log "$CMDS" "-LeafSlot=RefShot" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL)"
grep -a "grass material\|non-finite" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -4
