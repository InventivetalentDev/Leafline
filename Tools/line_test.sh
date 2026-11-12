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
	watch)     CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.6 any 11,Leaf.Delay 3.5 Leaf.Harvest"; T=${2:-400} ;;
	skip)      CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.6 any 11,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	lines1)    CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 1 0.5 any 5,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	lines3)    CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.5 any 5,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	exposed)   CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.9 exposed 3,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	sheltered) CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.9 sheltered 3,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	small)     CMDS="$DAY,Leaf.Delay 3 Leaf.TestLines 3 0.3 exposed 3,Leaf.Delay 3.5 Leaf.Harvest,Leaf.Delay 5 Leaf.SkipHarvest"; T=${2:-120} ;;
	auto)      CMDS="Leaf.Pool 6,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip quit"; T=${2:-1800} ;;
	*) echo "unknown scenario $SCEN"; exit 1 ;;
esac
LOG=LineTest-$SCEN.log
(cd /mnt/c && timeout $((T + 120)) "$ROOT/Scripts/unreal-lock.sh" "linetest-$SCEN" -- cmd.exe /c "D:\\UnrealProjects\\AntTreeGame\\Scripts\\RunGame.bat" "$LOG" \
	"$CMDS,Leaf.QuitAfter $T" "-LeafSlot=LineTest -unattended" >/dev/null 2>&1)
F=$ROOT/Saved/Logs/$LOG
grep -a "LogLeafline" "$F" | grep -a -E "Line [0-9]+ laid|Harvest|all workers home|AutoPlay|Pool [0-9]+:|^.*Line [0-9]+ \(worker|fallback|rescued|lost its ant|not ended|Recruits: pool|TestLines|Error|Warning" | sed 's/^.*LogLeafline: //' | cut -c1-220
echo "--- recruit trips: $(grep -a -c 'Recruit [0-9]* home with' "$F") home, $(grep -a -c 'Recruit [0-9]* blown off' "$F") blown off, $(grep -a -c 'turns round empty' "$F") empty returns"
grep -a -E "Fatal|Assertion failed|Unhandled Exception" "$F" | head -5
