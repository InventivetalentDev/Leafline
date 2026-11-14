#!/usr/bin/env bash
# Screenshots for docs/briefs/floor-from-above.md (the forest floor seen from the tree) in one standalone run, slot RefShot,
# seed 675856 (the owner's shot: the old giant). Runs this tree's binaries (ROOT) through the Unreal lock.
# Usage: ROOT=/mnt/d/UnrealProjects/leafline-floorfar Tools/floorfar_shots.sh <label> <plan file> [extra game args]
# Plan file: one step per line, "<seconds>|<console command>" or "<seconds>|SHOT <name>" (HighResShot 1, saved as
# Saved/RefShots/<label>-<name>.png). Lines starting with # are skipped. Add a final "<seconds>|Leaf.QuitAfter 1".
# Example plans: Tools/floorfar_plans/*.txt.
set -u
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
MAIN=/mnt/d/UnrealProjects/AntTreeGame
LABEL=${1:?usage: floorfar_shots.sh <label> <plan> [extra args]}
PLAN=${2:?usage: floorfar_shots.sh <label> <plan> [extra args]}
EXTRA=${3:-}
WINROOT=$(wslpath -w "$ROOT")
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
LOG=$ROOT/Saved/Logs/FloorFar.log
mkdir -p "$SHOTDIR" "$OUT"
CMDS=""
VIEWS=()
LAST=0
while IFS= read -r line || [ -n "$line" ]; do
	case "$line" in ''|'#'*) continue;; esac
	t=${line%%|*}; c=${line#*|}
	if [ "${c%% *}" = SHOT ]; then VIEWS+=("${c#SHOT }"); c="HighResShot 1"; fi
	if [ "$t" = 0 ]; then step="$c"; else step="Leaf.Delay $t $c"; fi
	if [ -z "$CMDS" ]; then CMDS="$step"; else CMDS="$CMDS,$step"; fi
	LAST=${t%.*}
done < "$PLAN"
MARK=$(mktemp -p "${TMPDIR:-/tmp}" floorfar.XXXX)
sleep 1
(cd /mnt/c && timeout $((LAST + 400)) "$MAIN/Scripts/unreal-lock.sh" floor-far -- cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" FloorFar.log "$CMDS" "-LeafSlot=RefShot $EXTRA" >/dev/null 2>&1)
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	[ $i -ge ${#VIEWS[@]} ] && break
	cp "$f" "$OUT/$LABEL-${VIEWS[$i]}.png"; i=$((i+1))
done
rm -f "$MARK"
echo "$i of ${#VIEWS[@]} screenshots in $OUT (label $LABEL)"
grep -a "Forest floor\|Leaf.Ground\|Leaf.Relief\|Ground material\|M_Ground\|Grove:\|non-finite\|Error" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -30
