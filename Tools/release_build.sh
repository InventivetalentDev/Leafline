#!/usr/bin/env bash
# release_build.sh: package the submission build from this checkout and check it, in one go.
#
#   Tools/release_build.sh                  # the real thing: refuses uncommitted changes and debug flags left on
#   DEV=1 Tools/release_build.sh            # a trial run: allows uncommitted changes and the debug flags
#   Tools/release_build.sh --check [zip]    # only the fresh-folder test, on a zip (default Build/inventivetalent_Leafline.zip),
#                                           #   e.g. the zip downloaded back from the itch page; prints its SHA-256
#
# Steps: preflight (tree clean, flags off, nobody playing, editor closed, disk) -> editor build -> package -> zip
# (the previous zip moves to Build/archive/) -> frame rates (Tools/perf_packaged.sh) -> twenty unattended days from a
# fresh folder with spaces in its path, starting with no save -> report.
# Everything lands in Saved/Release/<commit>-<time>/: report.md, the build, package and test logs.
# Exit codes: 0 all checks passed; 1 preflight refused; 2 build or package failed; 3 zip is 1 GB or larger;
# 4 a test found a problem (see report.md).
# Takes about an hour: build a few minutes, package 10-15, perf 5, twenty days 25-40.
set -u
ROOT=/mnt/d/UnrealProjects/AntTreeGame
ZIP=$ROOT/Build/inventivetalent_Leafline.zip
FRESH_WIN='D:\UnrealProjects\Leafline release test'
FRESH=/mnt/d/UnrealProjects/'Leafline release test'
LOCK=$ROOT/Scripts/unreal-lock.sh
DAYS=${DAYS:-20}   # days of the fresh-folder run; fewer only for trying the script out
cd "$ROOT" || exit 1

HASH=$(git rev-parse --short HEAD)
ID=$HASH-$(date +%H%M)
OUT=$ROOT/Saved/Release/$ID
OUT_WIN='D:\UnrealProjects\AntTreeGame\Saved\Release\'$ID
mkdir -p "$OUT"
REPORT=$OUT/report.md
FAIL=0

say() { echo "$(date +%H:%M) $*"; }
rep() { echo "$*" >> "$REPORT"; }
problem() { rep "- **Problem:** $*"; echo "PROBLEM: $*"; FAIL=1; }

# Unreal processes on Windows, one "name|command line" per line.
procs() {
	(cd /mnt/c && powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"name='AntTreeGame.exe' or name='UnrealEditor.exe'\" | ForEach-Object { \$_.Name + '|' + \$_.CommandLine }" 2>/dev/null | tr -d '\r')
}
# Someone watching a game: a packaged or -game run that is not offscreen (the owner, or a real-time video sound take).
someone_playing() { procs | grep -iE '^AntTreeGame\.exe|^UnrealEditor\.exe\|.* -game' | grep -viq 'RenderOffscreen'; }
# Any process started from the packaged build folder (perf or screenshot runs hold its files).
build_folder_busy() { procs | grep -iq 'Build\\Windows'; }
# An editor open on this checkout's project (it holds the module DLLs the build replaces).
editor_open() { procs | grep -i '^UnrealEditor\.exe|' | grep -vi ' -game' | grep -iq 'AntTreeGame\\AntTreeGame\.uproject'; }

wait_for() { # wait_for <description> <test function>: wait up to 30 min while the test is true
	local i
	for i in $(seq 1 60); do
		"$2" || return 0
		[ "$i" = 1 ] && say "waiting: $1"
		sleep 30
	done
	say "still $1 after 30 min, giving up"; return 1
}

fresh_test() { # fresh_test <zip>: unzip into a new folder with spaces, play twenty unattended days from no save
	local zip=$1 log
	say "fresh folder: $FRESH"
	rm -rf "$FRESH"; mkdir -p "$FRESH"
	unzip -q "$zip" -d "$FRESH" || { problem "unzip of $zip failed"; return; }
	[ -f "$FRESH/Leafline/AntTreeGame.exe" ] || { problem "no Leafline/AntTreeGame.exe in the zip"; return; }
	# A .bat file: a quoted Windows path with spaces does not survive cmd.exe /c from bash.
	printf '@echo off\r\n"%s\\Leafline\\AntTreeGame.exe" -windowed -ResX=1280 -ResY=720 -RenderOffscreen -nosplash -unattended "-ExecCmds=Leaf.StartDay 1,Leaf.Delay 3 Leaf.AutoPlay %s 3 skip buy reload report quit,Leaf.QuitAfter 2700"\r\n' "$FRESH_WIN" "$DAYS" > "$OUT/fresh.bat"
	wait_for "someone is playing" someone_playing || { problem "fresh-folder test skipped: someone kept playing"; return; }
	say "$DAYS unattended days from the fresh folder (20 days take 25-40 min)"
	(cd /mnt/c && timeout 3000 "$LOCK" release-fresh -- cmd.exe /c "$OUT_WIN\\fresh.bat" > "$OUT/fresh.lock.log" 2>&1)
	log="$FRESH/Leafline/AntTreeGame/Saved/Logs/AntTreeGame.log"
	[ -f "$log" ] || { problem "the fresh-folder run wrote no log (did it start?)"; return; }
	cp "$log" "$OUT/fresh.log"
	local fatal endcard day20 rescues failed nonfinite
	fatal=$(grep -a -cE 'Fatal|Assertion failed|Unhandled Exception' "$log")
	endcard=$(grep -a -m1 -o 'AutoPlay: end card after [0-9]* days[^[:cntrl:]]*' "$log")
	day20=$(grep -a -o "Colony loaded from slot[^[:cntrl:]]*day $DAYS,[^[:cntrl:]]*" "$log" | tail -1 | cut -c1-200)
	rescues=$(grep -a -c 'rescued' "$log")
	failed=$(grep -a 'Failed to load' "$log" | grep -avciE 'profil|\.dll')
	nonfinite=$(grep -a -iE 'non-finite|missing material|fallback material' "$log" | grep -avic 'no non-finite')
	rep ""
	rep "## $DAYS unattended days from a fresh folder with spaces"
	rep ""
	rep "Folder: \`$FRESH_WIN\`, no save at the start, shipped defaults (no tuning commands)."
	rep ""
	rep "- Crash lines (Fatal, assertion, unhandled exception): $fatal"
	rep "- End card: ${endcard:-not reached}"
	rep "- Day $DAYS: ${day20:-no day-$DAYS load line}"
	rep "- Rescues: $rescues"
	rep "- 'Failed to load' lines outside profiler DLLs: $failed; non-finite / missing / fallback material lines: $nonfinite"
	rep "- Day 1: $(grep -a -m1 -o 'Day 1: all workers home[^[:cntrl:]]*' "$log" | cut -c1-200)"
	rep "- Bird, first lines: $(grep -a -o 'Bird: day [^[:cntrl:]]*' "$log" | head -3 | cut -c1-90 | tr '\n' ' ')"
	rep "- Milestones and moves: $(grep -a -oE 'Colony (milestone [0-9] reached|moves to tree)[^[:cntrl:]]*' "$log" | cut -c1-70 | tr '\n' ';')"
	[ "$fatal" = 0 ] || problem "$fatal crash lines in the fresh-folder run"
	[ -n "$endcard" ] || [ "$DAYS" -lt 20 ] || problem "the fresh-folder run never reached the end card"
	[ "$rescues" = 0 ] || problem "$rescues rescues in $DAYS days"
	[ "$failed" = 0 ] || problem "'Failed to load' lines in the fresh-folder run"
	[ "$nonfinite" = 0 ] || problem "non-finite / missing / fallback material lines in the fresh-folder run"
	rm -rf "$FRESH"
}

# --check: only the fresh-folder test on a given zip.
if [ "${1:-}" = "--check" ]; then
	Z=${2:-$ZIP}
	[ -f "$Z" ] || { echo "no zip at $Z"; exit 1; }
	echo "# Zip check $(date '+%a %d %b %H:%M')" > "$REPORT"
	rep ""
	rep "- Zip: \`$Z\`, $(stat -c %s "$Z") bytes, SHA-256 \`$(sha256sum "$Z" | cut -d' ' -f1)\`"
	LAST=$(ls -t "$ROOT"/Saved/Release/*/zip.sha256 2>/dev/null | head -1)
	[ -n "$LAST" ] && rep "- Last packaged zip ($(basename "$(dirname "$LAST")")): \`$(cat "$LAST")\`"
	fresh_test "$Z"
	rep ""; rep "Result: $([ $FAIL = 0 ] && echo 'all checks passed' || echo 'PROBLEMS, see above')"
	echo; cat "$REPORT"; echo; echo "Report: $REPORT"
	[ $FAIL = 0 ] || exit 4; exit 0
fi

echo "# Release build $ID ($(date '+%a %d %b %H:%M'))" > "$REPORT"
rep ""
rep "Commit \`$(git log -1 --format='%h %s' | cut -c1-120)\`$([ "${DEV:-0}" = 1 ] && echo ', **DEV=1 trial run**')"
rep ""
rep "## Preflight"
rep ""

# ---- preflight ----
REFUSE=0
refuse() { rep "- **Refused:** $*"; echo "REFUSED: $*"; REFUSE=1; }
note() { rep "- $*"; echo "note: $*"; }
ini_value() { # ini_value <section> <key>: the last value of key in [section] of Config/DefaultGame.ini
	awk -v s="[$1]" -v k="$2" '{ sub(/\r$/, "") } /^\[/ { in_s = ($0 == s) } in_s && index($0, k "=") == 1 { v = substr($0, length(k) + 2) } END { print v }' Config/DefaultGame.ini
}
BRANCH=$(git rev-parse --abbrev-ref HEAD)
[ "$BRANCH" = main ] && note "branch main" || note "branch $BRANCH (not main)"
DIRTY=$(git status --porcelain --untracked-files=no | grep -v buildinfo.txt)
UNTRACKED=$(git status --porcelain | grep '^??' | grep -E ' (Source|Config|Content|Plugins)/')
if [ -n "$DIRTY$UNTRACKED" ]; then
	if [ "${DEV:-0}" = 1 ]; then note "uncommitted changes allowed (DEV=1): $(echo $DIRTY $UNTRACKED | cut -c1-200)"
	else refuse "uncommitted changes (the build line would say +dirty, or untracked files would go into the build): $(echo $DIRTY $UNTRACKED | cut -c1-300)"; fi
else note "tree clean"; fi
BL=$(ini_value /Script/AntTreeGame.LeaflineHUD bShowBuildLine)
IV=$(ini_value /Script/AntTreeGame.LeaflinePlayerController bInputVizKey)
for pair in "bShowBuildLine:$BL" "bInputVizKey:$IV"; do
	k=${pair%%:*}; v=${pair#*:}
	if [ "$v" = False ]; then note "$k=False"
	elif [ "${DEV:-0}" = 1 ]; then note "$k=${v:-unset (code default True)}, allowed (DEV=1)"
	else refuse "$k=${v:-unset (code default True)} in Config/DefaultGame.ini; set it False for the submission build"; fi
done
editor_open && refuse "an editor is open on this project; close it (QUIT_EDITOR, CLAUDE.md)"
FREE=$(df --output=avail -BG /mnt/d | tail -1 | tr -dc 0-9)
if [ "$FREE" -lt 8 ]; then refuse "only ${FREE} GB free on D: (needs 8 at least)"
elif [ "$FREE" -lt 15 ]; then note "${FREE} GB free on D: (tight; Build/archive and Build/perf-backup-* can go)"
else note "${FREE} GB free on D:"; fi
note "Unreal lock: $("$LOCK" --status 2>&1 | tr '\n' ' ')"
[ $REFUSE = 0 ] || { echo; cat "$REPORT"; exit 1; }
wait_for "someone is playing" someone_playing || { echo "someone kept playing; nothing built"; exit 1; }

# ---- build and package ----
OLDINFO=$(cut -d' ' -f1 Content/Leafline/UIRaw/buildinfo.txt 2>/dev/null)
bash Tools/write_buildinfo.sh > /dev/null
INFO=$(cat Content/Leafline/UIRaw/buildinfo.txt)
rep "- Build line text (shown only if bShowBuildLine is on): \`$INFO\`"
say "editor build"
for i in $(seq 1 20); do
	(cd /mnt/c && timeout 1500 "$LOCK" release-build -- cmd.exe /c 'D:\UnrealProjects\AntTreeGame\Scripts\build_editor.bat' > "$OUT/build.log" 2>&1)
	grep -q 'Live Coding is active\|ConflictingInstance\|cannot open file.*\.dll\|LNK1104' "$OUT/build.log" || break
	sleep 30
done
grep -q 'Result: Succeeded' "$OUT/build.log" || { rep "- **Editor build failed**, see build.log"; grep -E 'error|Result:' "$OUT/build.log" | head -8; exit 2; }
wait_for "a run is using Build/Windows" build_folder_busy || { rep "- **Build/Windows stayed busy; not packaged**"; exit 2; }
say "package (10-15 min)"
for i in $(seq 1 20); do
	wait_for "someone is playing" someone_playing || exit 2
	(cd /mnt/c && timeout 2700 "$LOCK" release-package -- cmd.exe /c 'D:\UnrealProjects\AntTreeGame\Scripts\package_win64.bat' > "$OUT/package.log" 2>&1)
	grep -q 'ConflictingInstance\|Live Coding is active' "$OUT/package.log" || break
	sleep 30
done
grep -q 'AutomationTool exiting with ExitCode=0' "$OUT/package.log" || { rep "- **Package failed**, see package.log"; grep -E 'Error:|error ' "$OUT/package.log" | head -8; exit 2; }
rep "- Packaged: $(grep -o 'BuildCookRun time: [0-9.]* s' "$OUT/package.log" | tail -1)"

# ---- zip ----
if [ -f "$ZIP" ]; then
	mkdir -p Build/archive
	mv "$ZIP" "Build/archive/inventivetalent_Leafline-${OLDINFO:-previous}-$(date +%H%M).zip"
	rep "- Previous zip moved to Build/archive/ (its build: ${OLDINFO:-unknown})"
fi
python3 Tools/zip_build.py > "$OUT/zip.log" 2>&1; ZRC=$?
[ -f "$ZIP" ] || { rep "- **No zip written**, see zip.log"; exit 2; }
SIZE=$(stat -c %s "$ZIP")
sha256sum "$ZIP" | cut -d' ' -f1 > "$OUT/zip.sha256"
rep ""
rep "## Zip"
rep ""
rep "- \`Build/inventivetalent_Leafline.zip\`: $SIZE bytes ($(awk -v b="$SIZE" 'BEGIN { printf "%.2f", b / 1073741824 }') GiB), SHA-256 \`$(cat "$OUT/zip.sha256")\`"
if [ "$ZRC" = 1 ] || [ "$SIZE" -ge 1073741824 ]; then rep "- **Over the 1 GB limit**"; echo; cat "$REPORT"; exit 3; fi

# ---- frame rates ----
say "frame rates (about 5 min)"
wait_for "someone is playing" someone_playing || problem "frame rates skipped: someone kept playing"
someone_playing || {
	rep ""
	rep "## Frame rates, 1920x1080 (Tools/perf_packaged.sh)"
	rep ""
	rep "Lock before: $("$LOCK" --status 2>&1 | tr '\n' ' ') (another run at the same time lowers these)"
	rep ""
	rep '```'
	bash Tools/perf_packaged.sh > "$OUT/perf.log" 2>&1
	grep -vE '^Restored' "$OUT/perf.log" | tail -16 >> "$REPORT"
	rep '```'
	grep -qi 'no capture written' "$OUT/perf.log" && problem "a frame-rate run wrote no capture (failed to start?)"
}
PL=$ROOT/Build/Windows/AntTreeGame/Saved/Logs/AntTreeGame.log
[ -f "$PL" ] && [ "$(grep -a -cE 'Fatal|Assertion failed|Unhandled Exception' "$PL")" != 0 ] && problem "crash lines in the last frame-rate run's log"

# ---- twenty days from a fresh folder ----
fresh_test "$ZIP"

rep ""
rep "Result: $([ $FAIL = 0 ] && echo 'all checks passed' || echo 'PROBLEMS, see above')"
echo; cat "$REPORT"; echo; echo "Report: $REPORT"
[ $FAIL = 0 ] || exit 4
exit 0
