#!/usr/bin/env bash
# Unattended runs for one tree per colony stage (docs/briefs/one-tree.md, acceptance tests). Standalone game from the
# editor binaries, in its own save slot (-LeafSlot=TreeTest unless noted) so the owner's colony is never touched. Runs
# through the Unreal lock; the editor must be closed.
# Usage: Tools/tree_test.sh <scenario> [seconds]
#   flagoff    one tree off: days 1 and 2 with the old trees (test 9; compare leaf counts with pre-one-tree)
#   fresh      a fresh colony's first tree: Leaf.TreeReport (density by height)
#   play2      fresh colony, two unattended days (test 1, first half)
#   relaunch   no reset: load the slot and report (test 1 second half, test 14 second half)
#   midquit    no reset: lay three lines, then quit in the middle of the day (test 14 first half)
#   strip      Leaf.Strip 40, report, Leaf.Move, report (debug commands, move)
#   auto20     twenty unattended days, buying, save reloaded and tree regrown before each day (tests 2, 3, 4, 7, 11)
#   moves      seven days with 100 haul given each morning: both moves and the end card (test 4)
#   combo      OT=0|1 RC=0|1: seven days with 100 haul given each morning, to the end card (all four flag combinations)
#   recoff     recruits off, twenty days (test 13)
#   auto20hold / recoffhold   auto20 / recoff with AutoPlay's hold<S> (HOLD, default 240): the last scout stays out with its
#              piece for S sim seconds of shift, so the bird (only active in shifts) has time to act (docs/briefs/bird.md);
#              HOLDGRIP=holdgrip makes that scout grip meanwhile (a player who grips when the bird dives)
#   offauto    one tree off, twenty days        offrecoff   one tree and recruits off, twenty days
#   v2         a copy of a real version-2 save (Saved/SaveGames/TreeTestV2.sav.orig) loads and plays a day (test 8)
#   shots      screenshots: move card, stripped bough, faint trails
#   custom     the commands in $TCMDS (comma-separated)
# The log is Saved/Logs/TreeTest-<scenario>.log; a summary of the Leafline lines is printed.
set -u
# ROOT (environment) runs another checkout, e.g. a worktree: ROOT=/mnt/d/UnrealProjects/leafline-bird. PRE: commands run
# first (e.g. PRE="Leaf.Bird 0"). HOLDER: the name on the Unreal lock.
ROOT=${ROOT:-/mnt/d/UnrealProjects/AntTreeGame}
WROOT=$(wslpath -w "$ROOT")
SCEN=${1:?usage: tree_test.sh <scenario> [seconds]}
SLOT=${SLOT:-TreeTest}
FRESH="Leaf.ResetSave,Leaf.OneTree 1,Leaf.Recruits 1,Leaf.StartDay 1"
case "$SCEN" in
	flagoff)   CMDS="Leaf.ResetSave,Leaf.OneTree 0,Leaf.StartDay 1 517192,Leaf.Delay 3 Leaf.TreeReport,Leaf.Delay 5 Leaf.StartDay 2,Leaf.Delay 8 Leaf.TreeReport"; T=${2:-30} ;;
	fresh)     CMDS="$FRESH,Leaf.Delay 3 Leaf.TreeReport,Leaf.Delay 4 Leaf.PatchSurvey 40"; T=${2:-40} ;;
	play2)     CMDS="$FRESH,Leaf.Delay 2 Leaf.AutoPlay 2 3 skip buy report quit"; T=${2:-300} ;;
	relaunch)  CMDS="Leaf.Delay 3 Leaf.TreeReport relaunch"; T=${2:-20} ;;
	midquit)   CMDS="Leaf.Delay 1 Leaf.StartDay ${DAYN:-3},Leaf.Delay 3 Leaf.TestLines 3 0.6 climb 5,Leaf.Delay 6 Leaf.TreeReport mid-day"; T=${2:-20} ;;
	strip)     CMDS="$FRESH,Leaf.Delay 3 Leaf.TreeReport before,Leaf.Delay 4 Leaf.Strip 40,Leaf.Delay 5 Leaf.TreeReport stripped,Leaf.Delay 7 Leaf.Move,Leaf.Delay 10 Leaf.TreeReport moved"; T=${2:-30} ;;
	auto20)    CMDS="$FRESH,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy reload report quit"; T=${2:-2400} ;;
	auto20hold) CMDS="$FRESH,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy reload report hold${HOLD:-240} ${HOLDGRIP:-} quit"; T=${2:-3000} ;;
	recoffhold) CMDS="Leaf.ResetSave,Leaf.OneTree 1,Leaf.Recruits 0,Leaf.StartDay 1,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy report hold${HOLD:-240} quit"; T=${2:-3000} ;;
	moves)     CMDS="$FRESH,Leaf.Delay 2 Leaf.AutoPlay 7 3 skip buy report give100 quit"; T=${2:-900} ;;
	combo)     CMDS="Leaf.ResetSave,Leaf.OneTree ${OT:-1},Leaf.Recruits ${RC:-1},Leaf.StartDay 1,Leaf.Delay 2 Leaf.AutoPlay 7 3 skip buy give100 quit"; T=${2:-900}; LOGSUF="-ot${OT:-1}-rc${RC:-1}" ;;
	recoff)    CMDS="Leaf.ResetSave,Leaf.OneTree 1,Leaf.Recruits 0,Leaf.StartDay 1,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy report quit"; T=${2:-2400} ;;
	offauto)   CMDS="Leaf.ResetSave,Leaf.OneTree 0,Leaf.Recruits 1,Leaf.StartDay 1,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy quit"; T=${2:-2400} ;;
	offrecoff) CMDS="Leaf.ResetSave,Leaf.OneTree 0,Leaf.Recruits 0,Leaf.StartDay 1,Leaf.Delay 2 Leaf.AutoPlay 20 3 skip buy quit"; T=${2:-2400} ;;
	v2)        cp -p "$ROOT/Saved/SaveGames/TreeTestV2.sav.orig" "$ROOT/Saved/SaveGames/TreeTestV2.sav"; SLOT=TreeTestV2
	           CMDS="Leaf.Delay 3 Leaf.Upgrades,Leaf.Delay 4 Leaf.TreeReport v2,Leaf.Delay 5 Leaf.AutoPlay 1 3 skip quit"; T=${2:-200} ;;
	shots)     CMDS="$FRESH,Leaf.Delay 3 Leaf.TestLines 3 0.6 climb 5,Leaf.Delay 5 Leaf.Strip 30,Leaf.Delay 8 shot showui,Leaf.Delay 10 Leaf.Move,Leaf.Delay 14 shot showui"; T=${2:-30}; SHOTS=1 ;;
	custom)    CMDS="${TCMDS:?set TCMDS to the commands}"; T=${2:-60} ;;
	*) echo "unknown scenario $SCEN"; exit 1 ;;
esac
LOG=TreeTest-$SCEN${LOGSUF:-}.log
MARK=$ROOT/Saved/RefShots/.tree-marker
mkdir -p "$ROOT/Saved/RefShots" && touch "$MARK"
[ -n "${PRE:-}" ] && CMDS="$PRE,$CMDS"
(cd /mnt/c && timeout $((T + 120)) "$ROOT/Scripts/unreal-lock.sh" "${HOLDER:-treetest-$SCEN}" -- cmd.exe /c "$WROOT\\Scripts\\RunGame.bat" "$LOG" \
	"$CMDS,Leaf.QuitAfter $T" "-LeafSlot=$SLOT -unattended" >/dev/null 2>&1)
F=$ROOT/Saved/Logs/$LOG
grep -a "LogLeafline" "$F" | grep -a -v "Wind: EWindPhase" | grep -a -E "Day [0-9]+ starts|all workers home|Day [0-9]+ cuts|TreeReport|Tree saved|Tree:|Colony loaded|Colony moves|Move card|milestone|AutoPlay:|Bird: day|Bird: pass|knocked off by the bird|rescued|Migrated|Wind for day|Trails: loaded|LeafSet:|Leaf.Strip|Leaf.Move|One tree|not ended|Error|Warning" | sed 's/^.*LogLeafline: //' | cut -c1-230
grep -a -E "Fatal|Assertion failed|Unhandled Exception" "$F" | head -5
if [ -n "${SHOTS:-}" ]; then
	n=0
	for f in $(find "$ROOT/Saved/Screenshots/WindowsEditor" -maxdepth 1 -name 'ScreenShot*.png' -newer "$MARK" | sort); do
		n=$((n + 1)); cp "$f" "$ROOT/Saved/RefShots/tree-$SCEN-$n.png"; echo "$ROOT/Saved/RefShots/tree-$SCEN-$n.png"
	done
fi
