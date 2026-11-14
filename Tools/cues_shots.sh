#!/usr/bin/env bash
# Screenshots of the in-world wind cues (docs/briefs/in-world-cues.md): one forced gust per pass, each pass on one spot
# with Leaf.Cues 0 or 1, shot from the normal camera at fixed times after the gust starts (warning 3 s, then rising
# 1.2 s, then holding). Standalone game from this tree's editor binaries, save slot CuesTest. Runs through the Unreal
# lock; the editor of this tree must be closed.
# Usage: Tools/cues_shots.sh <exposed|sheltered|both|grip> [label]
#   exposed / sheltered   Leaf.FaceWind on the trunk's windward / lee side, cues off then on
#   both                  exposed off, exposed on, sheltered off, sheltered on (one run)
#   grip                  windward side, cues off then on; the ant grips as the gust rises (Leaf.Hold grip)
# Environment: ROOT (default this script's tree), SEED, AIM (camera yaw relative to the wind: 0 downwind, 180 upwind,
#   default 90), PITCH (default -12), TUNE (commands run first, e.g. "Leaf.Tune wind CueFrontSpeed 500"),
#   AT (seconds after the gust for each shot, default "1.0 2.0 2.6 3.2 4.5"; grip "3.45 3.6 3.75 4.4 5.6"),
#   PASSES (spot:cues[:Field=Value/Field=Value] list, e.g. "sheltered:1 exposed:1:CueBrightness=0.3"; the fields are
#   set with Leaf.Tune wind just before that pass and stay set for the passes after it).
# Output: Saved/RefShots/cues-<label>-<spot>-<off|on>-<seconds>.png, and the wind cue lines of the log.
set -u
ROOT=${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
WINROOT=$(wslpath -w "$ROOT")
MODE=${1:?usage: cues_shots.sh <exposed|sheltered|both|grip> [label]}
LABEL=${2:-t}
SEED=${SEED:-517192}
AIM=${AIM:-90}
PITCH=${PITCH:--12}
case "$MODE" in
	exposed|sheltered) DEF="$MODE:0 $MODE:1" ;;
	both) DEF="exposed:0 exposed:1 sheltered:0 sheltered:1" ;;
	grip) DEF="grip:0 grip:1" ;;
	*) echo "unknown mode $MODE"; exit 1 ;;
esac
PASSES=${PASSES:-$DEF}
# Shot times: AT for every pass if given, else per spot (grip passes shoot round the grip at +3.3 s).
AT_GUST=${AT:-1.0 2.0 2.6 3.2 4.5}
AT_GRIP=${AT:-3.45 3.6 3.75 4.4 5.6}
CMDS="Leaf.ResetSave,Leaf.StartDay 1 $SEED,Leaf.CuesLog 1"
[ -n "${TUNE:-}" ] && CMDS="$CMDS,$TUNE"
# One pass every 14 s from 6 s: a forced gust is over within about 10 s, and day 1's calm after it lasts at least 9 s.
NAMES=()
k=0
for pass in $PASSES; do
	IFS=: read -r spot c tune <<< "$pass"
	face=$spot; [ "$spot" = grip ] && face=exposed
	onoff=off; [ "$c" = 1 ] && onoff=on
	B=$((6 + 14 * k))
	G=$(echo "$B + 0.2" | bc)
	CMDS="$CMDS,Leaf.Delay $((B - 2)) Leaf.FaceWind $face,Leaf.Delay $((B - 1)) Leaf.WindAim $AIM $PITCH,Leaf.Delay $B Leaf.Cues $c,Leaf.Delay $G Leaf.Gust 1"
	[ "$c" = 1 ] && CMDS="$CMDS,Leaf.Delay $G Leaf.CuesStat 8"
	if [ -n "${tune:-}" ]; then
		for fv in ${tune//\// }; do CMDS="$CMDS,Leaf.Delay $((B - 1)) Leaf.Tune wind ${fv%%=*} ${fv#*=}"; done
	fi
	[ "$spot" = grip ] && CMDS="$CMDS,Leaf.Delay $(echo "$G + 3.3" | bc) Leaf.Hold grip 4"
	ATS=$AT_GUST; [ "$spot" = grip ] && ATS=$AT_GRIP
	for a in $ATS; do
		CMDS="$CMDS,Leaf.Delay $(echo "$G + $a" | bc) HighResShot 1"
		NAMES+=("$spot-$onoff${tune:+-p$k}-$a")
	done
	k=$((k + 1))
done
T=$((6 + 14 * k))
LOG=CuesTest-$MODE.log
SHOTDIR=$ROOT/Saved/Screenshots/WindowsEditor
OUT=$ROOT/Saved/RefShots
MARK=$OUT/.cues-marker
mkdir -p "$OUT" "$SHOTDIR" && touch "$MARK"
# The timeout sits inside the lock, so waiting for the lock never counts against it.
(cd /mnt/c && /mnt/d/UnrealProjects/AntTreeGame/Scripts/unreal-lock.sh "cues-$MODE" -- timeout $((T + 150)) cmd.exe /c "$WINROOT\\Scripts\\RunGame.bat" "$LOG" \
	"$CMDS,Leaf.QuitAfter $T" "-LeafSlot=CuesTest -unattended" >"$ROOT/Saved/Logs/CuesTest-$MODE.lock.log" 2>&1)
tail -3 "$ROOT/Saved/Logs/CuesTest-$MODE.lock.log" | grep -a "unreal-lock" | grep -v "released by"
F=$ROOT/Saved/Logs/$LOG
grep -a "LogLeafline" "$F" | grep -a -E "FaceWind:|Leaf.Cues |Leaf.Tune|wind\.Cue|Wind cues: game|Error|Warning:" | sed 's/^.*LogLeafline: //' | cut -c1-200
grep -a -E "Fatal|Assertion failed|Unhandled Exception" "$F" | head -5
n=0
for f in $(find "$SHOTDIR" -maxdepth 1 \( -name 'HighresScreenshot*.png' -o -name 'ScreenShot*.png' \) -newer "$MARK" | sort); do
	name=${NAMES[$n]:-extra$n}
	cp "$f" "$OUT/cues-$LABEL-$name.png"; echo "$OUT/cues-$LABEL-$name.png"
	n=$((n + 1))
done
echo "$n shots (expected ${#NAMES[@]})"
