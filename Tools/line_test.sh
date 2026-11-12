#!/usr/bin/env bash
# Unattended runs for the line (docs/briefs/the-line.md, acceptance tests). Standalone game from the editor binaries,
# in its own save slot (-LeafSlot=LineTest) so the owner's colony is never touched. Runs through the Unreal lock; the
# editor must be closed.
# Usage: Tools/line_test.sh <scenario> [seconds]
#   smoke     one day: pool 4, three test lines, harvest watched, line info logged
#   watch     the same day with the harvest watched, for comparison with skip (test 6)
#   skip      the same day with the harvest skipped (test 6)
#   lines1    pool 4, one line (test 1)        lines3   pool 4, three lines (test 1)
#   exposed   pool 4, three large-piece lines on the windward side (test 3)   sheltered  the same on the lee side
#   small     pool 4, three small-piece lines on the windward side (test 3)
#   dry       pool 6: one line, its patch runs dry while scouts wait, then a second line (test 2)
#   windwatch / windskip   large pieces (more blow-offs), harvest watched / skipped (test 6 with losses; DAYN=3)
#   shots     screenshots of the HUD: line rows and patch read-out, harvest at the nest and at a patch, recap
#   perfbase / perfpool   frame times with two lines and one scout idle at the nest: pool 0 / pool 10 at work (test 8)
#   perfharvest           frame times during the 5x harvest with a pool of 10
#   migrate   load a copy of a version-1 save from Saved/SaveGames/LineTestV1.sav (it is migrated in place)
#   auto      twenty unattended days, harvests skipped (test 7)
# The log is Saved/Logs/LineTest-<scenario>.log; a summary of the Leafline lines is printed.
set -u
ROOT=/mnt/d/UnrealProjects/AntTreeGame
SCEN=${1:?usage: line_test.sh <scenario> [seconds]}
SEED=${SEED:-517192}
POOL=${POOL:-4}
DAYN=${DAYN:-1}
DAY="Leaf.ResetSave,Leaf.Pool $POOL,Leaf.StartDay $DAYN $SEED"
INFO="Leaf.Delay 20 Leaf.LineInfo,Leaf.Delay 60 Leaf.LineInfo,Leaf.Delay 120 Leaf.LineInfo"
case "$SCEN" in
	smoke)     CMDS="$DAY,Leaf.Delay 2 Leaf.PatchSurvey 40,Leaf.Delay 3 Leaf.TestLines 3 0.5 any 7,Leaf.Delay 4 Leaf.Harvest,$INFO"; T=${2:-200} ;;
	watch)     CMDS="$DAY,Leaf.Delay 1 Leaf.AtSim 3 Leaf.TestLines 3 0.6 any 11,Leaf.Delay 1 Leaf.AtSim 3 Leaf.Harvest"; T=${2:-400} ;;
	skip)      CMDS="$DAY,Leaf.Delay 1 Leaf.AtSim 3 Leaf.TestLines 3 0.6 any 11,Leaf.Delay 1 Leaf.AtSim 3 Leaf.Harvest,Leaf.Delay 6 Leaf.SkipHarvest"; T=${2:-120} ;;
	lines1)    CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 1 0.5 any 5,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	lines3)    CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.5 any 5,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	exposed)   CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.9 exposed 3,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	sheltered) CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.9 sheltered 3,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	small)     CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.3 exposed 3,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	dry)       CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 1 0.5 any 5,Leaf.Delay 100 Leaf.LineInfo,Leaf.Delay 130 Leaf.LineInfo,Leaf.Delay 135 Leaf.TestLines 1 0.5 any 9,Leaf.Delay 200 Leaf.LineInfo,Leaf.Delay 205 Leaf.Harvest,Leaf.Delay 207 Leaf.SkipHarvest"; T=${2:-260} ;;
	windwatch) CMDS="$DAY,Leaf.Delay 1 Leaf.AtSim 3 Leaf.TestLines 3 0.9 any 11,Leaf.Delay 1 Leaf.AtSim 3 Leaf.Harvest"; T=${2:-400} ;;
	windskip)  CMDS="$DAY,Leaf.Delay 1 Leaf.AtSim 3 Leaf.TestLines 3 0.9 any 11,Leaf.Delay 1 Leaf.AtSim 3 Leaf.Harvest,Leaf.Delay 6 Leaf.SkipHarvest"; T=${2:-120} ;;
	shots)     CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 2 0.5 any 5,Leaf.Delay 6 Leaf.GoLeaf,Leaf.Delay 9 Leaf.Aim trunk -20,Leaf.Delay 12 shot showui,Leaf.Delay 40 shot showui,Leaf.Delay 42 Leaf.Harvest,Leaf.Delay 50 shot showui,Leaf.Delay 60 Leaf.Cycle,Leaf.Delay 66 shot showui,Leaf.Delay 70 Leaf.SkipHarvest,Leaf.Delay 80 shot showui,Leaf.Delay 86 shot showui"; T=${2:-95}; SHOTS=1 ;;
	perfbase)  CMDS="Leaf.ResetSave,Leaf.Pool 0,Leaf.StartDay 1 $SEED,t.MaxFPS 0,r.VSync 0,Leaf.Delay 3 Leaf.TestLines 2 0.4 sheltered 5,Leaf.Delay 10 CsvProfile FRAMES=1500"; T=${2:-60}; CSV=1 ;;
	perfpool)  CMDS="Leaf.ResetSave,Leaf.Pool 10,Leaf.StartDay 1 $SEED,t.MaxFPS 0,r.VSync 0,Leaf.Delay 3 Leaf.TestLines 2 0.4 sheltered 5,Leaf.Delay 10 CsvProfile FRAMES=1500"; T=${2:-60}; CSV=1 ;;
	perfharvest) CMDS="Leaf.ResetSave,Leaf.Pool 10,Leaf.StartDay 1 $SEED,t.MaxFPS 0,r.VSync 0,Leaf.Delay 3 Leaf.TestLines 3 0.4 sheltered 5,Leaf.Delay 4 Leaf.Harvest,Leaf.Delay 8 CsvProfile FRAMES=1500"; T=${2:-60}; CSV=1 ;;
	migrate)   CMDS="Leaf.Upgrades"; T=${2:-25}; SLOT=LineTestV1 ;;
	auto)      CMDS="Leaf.ResetSave,Leaf.Pool 6,Leaf.StartDay 1 $SEED,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy quit"; T=${2:-1800} ;;
	*) echo "unknown scenario $SCEN"; exit 1 ;;
esac
LOG=LineTest-$SCEN.log
MARK=$ROOT/Saved/RefShots/.line-marker
mkdir -p "$ROOT/Saved/RefShots" && touch "$MARK"
(cd /mnt/c && timeout $((T + 120)) "$ROOT/Scripts/unreal-lock.sh" "linetest-$SCEN" -- cmd.exe /c "D:\\UnrealProjects\\AntTreeGame\\Scripts\\RunGame.bat" "$LOG" \
	"$CMDS,Leaf.QuitAfter $T" "-LeafSlot=${SLOT:-LineTest} -unattended" >/dev/null 2>&1)
F=$ROOT/Saved/Logs/$LOG
grep -a "LogLeafline" "$F" | grep -a -E "Line [0-9]+ laid|Harvest|all workers home|AutoPlay|Pool [0-9]+:|^.*Line [0-9]+ \(worker|fallback|rescued|lost its ant|not ended|Recruits: pool|TestLines|Error|Warning" | sed 's/^.*LogLeafline: //' | cut -c1-220
echo "--- recruit trips: $(grep -a -c 'Recruit [0-9]* home with' "$F") home, $(grep -a -c 'Recruit [0-9]* blown off' "$F") blown off, $(grep -a -c 'turns round empty' "$F") empty returns"
grep -a -E "Fatal|Assertion failed|Unhandled Exception" "$F" | head -5
if [ -n "${CSV:-}" ]; then
	C=$(find "$ROOT/Saved/Profiling/CSV" -maxdepth 1 -name '*.csv' -newer "$MARK" 2>/dev/null | sort | tail -1)
	if [ -n "$C" ]; then python3 "$ROOT/Tools/csv_fps.py" --label "$SCEN" "$C"; else echo "$SCEN: no CSV capture written"; fi
fi
if [ -n "${SHOTS:-}" ]; then
	n=0
	for f in $(find "$ROOT/Saved/Screenshots/WindowsEditor" -maxdepth 1 -name 'ScreenShot*.png' -newer "$MARK" | sort); do
		n=$((n + 1)); cp "$f" "$ROOT/Saved/RefShots/line-$SCEN-$n.png"; echo "$ROOT/Saved/RefShots/line-$SCEN-$n.png"
	done
fi
