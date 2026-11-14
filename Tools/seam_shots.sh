#!/usr/bin/env bash
# Skirt seam shots (branch seams, 14 Nov night): one standalone run, seed 517192, HUD hidden, each view paused so frames
# compare pixel for pixel. Views: base (from the nest: the trunk's foot meeting the ground), junction (limb 21 leaving
# the trunk, from the side, -30) and junction-top (the same from above, -80, the look pass's view).
# Usage: ROOT=<tree> Tools/seam_shots.sh <label> [bark] [extra commands]
#   bark: the Leaf.Bark value (1 = M_TreeBark2, the default; 2 = M_TreeBark3 with the seam data). Extra commands run
#   right after the day starts, comma-separated (e.g. "Leaf.Fillets 1 -1 -1 -1 80" for the ground fillet).
# Files: Saved/RefShots/seam-<label>-<view>.png (2560x1440). The editor must be closed: runs go through the Unreal lock.
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: seam_shots.sh <label> [bark] [extra commands]}
BARK=${2:-1}
EXTRA=${3:-}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
mkdir -p "$SHOTDIR" "$OUT"
STEPS=(
 "4|Leaf.NewDay 517192" "6|Leaf.Bark $BARK" "7|showhud"
 "8|Leaf.Aim 170 -10" "11|pause" "12|HighResShot 2" "12.5|pause"
 "13|Leaf.Teleport 21 0.08" "15|Leaf.Aim trunk -30" "18|pause" "19|HighResShot 2" "19.5|pause"
 "20|Leaf.Aim trunk -80" "23|pause" "24|HighResShot 2"
 "26|Leaf.QuitAfter 1"
)
VIEWS=(base junction junction-top)
CMDS="Leaf.Delay 5 Leaf.Bark $BARK"
[ -n "$EXTRA" ] && CMDS="$CMDS,Leaf.Delay 5.5 ${EXTRA//,/,Leaf.Delay 5.5 }"
for s in "${STEPS[@]}"; do
	t=${s%%|*}; c=${s#*|}
	CMDS="$CMDS,Leaf.Delay $t $c"
done
MARK=$(mktemp -p "${TMPDIR:-/tmp}" seamshots.XXXX)
sleep 1
(cd /mnt/c && timeout 300 "$MAIN/Scripts/unreal-lock.sh" seam-shots -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" SeamShots.log "$CMDS" "-LeafSlot=SeamShot -unattended" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" -printf '%T@ %p\n' | sort -n | cut -d' ' -f2-); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/seam-$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i screenshots in $OUT (label $LABEL, Leaf.Bark $BARK)"
grep -a "LogLeafline" "$ROOT/Saved/Logs/SeamShots.log" | grep -a -E "TreeSkirts blend|M_TreeBark3|Bark material|Leaf.Bark" | sed 's/^.*LogLeafline: //' | cut -c1-260
