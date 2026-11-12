#!/usr/bin/env bash
# Tests for Scripts/unreal-lock.sh (docs/tools.md). Run from WSL: Scripts/unreal-lock-test.sh
# Uses a lock under this tree's Saved/locktest (git-ignored), never the real one, and only harmless commands
# (sleep, true, false; cmd.exe running ping for the Windows part). Takes about a minute. It removes nothing:
# leftovers are moved into Saved/locktest/trash.
cd "$(dirname "$0")/.." || exit 1
T=$PWD/Saved/locktest; mkdir -p "$T/trash" "$T/fakebin"
printf '#!/bin/sh\nsleep 3; echo slowhost\n' > "$T/fakebin/hostname"; chmod +x "$T/fakebin/hostname"
export FOREST_LOCK_DIR=$T/unreal FOREST_LOCK_POLL=1 FOREST_LOCK_REPORT=3 FOREST_LOCK_WATCH=
L=$PWD/Scripts/unreal-lock.sh; D=$FOREST_LOCK_DIR
pass=0; fail=0; n=0
ok() { if [[ $1 == "$2" ]]; then pass=$((pass+1)); echo "  ok   $3 ($1)"; else fail=$((fail+1)); echo "  FAIL $3: got '$1', want '$2'"; fi; }
free() { [[ ! -e $D ]] && echo free || echo held; }
trash() { n=$((n+1)); [[ -e $D/owner ]] && mv "$D/owner" "$T/trash/owner.$n"; for f in "$D"/.owner.*; do [[ -e $f ]] && mv "$f" "$T/trash/"; done; rmdir "$D" 2>/dev/null; true; }
mkowner() { mkdir "$D" && printf '%s\n' "$1" > "$D/owner"; }
BOOT=$(cat /proc/sys/kernel/random/boot_id); NS=$(readlink /proc/self/ns/pid)

echo "== exit codes"
$L t -- true 2>/dev/null; ok $? 0 "true"; ok $(free) free "released after true"
$L t -- false 2>/dev/null; ok $? 1 "false"
$L t -- bash -c 'exit 42' 2>/dev/null; ok $? 42 "exit 42"
$L t -- /nonexistent/cmd 2>/dev/null; ok $? 127 "missing command"; ok $(free) free "released after missing command"
$L t sleep 1 2>/dev/null; ok $? 64 "no --"
out=$(cd /tmp && echo piped | $L "a b" -- bash -c 'printf "%s|" "$@"; cat; pwd' _ "x y" z 2>/dev/null); ok "$out" "x y|z|piped
/tmp" "args, stdin and cwd pass through"

echo "== competing"
( $L A -- bash -c 'echo A-start; sleep 3; echo A-end' 2>/dev/null ) > "$T/order" &
sleep 0.7
$L --no-wait C -- true 2>/dev/null; ok $? 75 "--no-wait while held"
$L --status >/dev/null; ok $? 1 "--status while held"
$L --timeout 0.02 C -- true 2>/dev/null; ok $? 75 "--timeout runs out"
$L B -- echo B-ran 2>/dev/null >> "$T/order"; ok $? 0 "B waited and ran"
wait; ok "$(tr '\n' ' ' < "$T/order")" "A-start A-end B-ran " "B ran only after A ended"
ok $(free) free "free after both"

echo "== signals"
$L s -- sleep 30 2>/dev/null & W=$!; sleep 1.5; C=$(pgrep -P $W sleep); kill -TERM $W; wait $W; ok $? 143 "TERM to wrapper"
ps -p "$C" >/dev/null; ok $? 1 "child stopped"; ok $(free) free "released after TERM"
set -m   # a script's background jobs ignore SIGINT (and bash can't trap a signal ignored at entry); job control avoids that
$L s -- sleep 30 2>/dev/null & W=$!; sleep 1.5; kill -INT $W; wait $W; ok $? 130 "INT to wrapper"; ok $(free) free "released after INT"
set +m
$L s -- sleep 30 2>/dev/null & W=$!; sleep 1.5; kill -KILL $(pgrep -P $W sleep); wait $W; ok $? 137 "child killed"; ok $(free) free "released after child killed"
$L s -- bash -c 'trap "" TERM; sleep 30' 2>/dev/null & W=$!; sleep 1.5; kill -TERM $W; sleep 1; ok $(free) held "child ignoring TERM: still held"
kill -TERM $W; wait $W; ok $? 143 "second TERM kills it"; ok $(free) free "released"
pkill -x -f 'sleep 30'

echo "== signal between mkdir and owner (slow hostname)"
PATH=$T/fakebin:$PATH $L slow -- true 2>/dev/null & W=$!
sleep 1; ok $(free) held "taken, owner not written yet"; kill -TERM $W; wait $W; ok $? 143 "TERM during owner write"; ok $(free) free "dir and temp owner removed"

echo "== stale"
$L k -- sleep 4 2>/dev/null & W=$!; sleep 1.5; kill -KILL $W; wait $W 2>/dev/null; sleep 3
$L --status >/dev/null; ok $? 2 "--status: stale"
$L k2 -- true 2>/dev/null; ok $? 76 "waiting on a stale lock fails"
$L --break-stale k3 -- true 2>/dev/null; ok $? 0 "--break-stale takes it"; ok $(free) free "free after"
mkowner "holder: x
pid: $$
pidstart: 1
pidns: $NS
boot: $BOOT"; $L --status >/dev/null; ok $? 2 "reused PID number: stale"; trash
mkowner "holder: x
pid: $$
pidstart: $(awk '{print $22}' /proc/$$/stat)
pidns: $NS
boot: $BOOT"; $L --status >/dev/null; ok $? 1 "live PID: held"; $L --break-stale --no-wait y -- true 2>/dev/null; ok $? 75 "live holder isn't broken"; trash
mkowner "holder: x
pid: 12
pidns: $NS
boot: 0000-old"; $L --status >/dev/null; ok $? 2 "other boot: stale"; trash
mkowner "holder: x
pid: 999999
pidns: pid:[1]
boot: $BOOT"; $L --break-stale --no-wait y -- true 2>/dev/null; ok $? 75 "other PID namespace: unknown, not broken"; trash
mkowner "rv-world 21:30 headless"; $L --break-stale --no-wait y -- true 2>/dev/null; ok $? 75 "hand-written owner: not broken"; trash
mkdir "$D"; $L --status >/dev/null; ok $? 1 "no owner file: held"; trash

echo "== release safety"
$L r -- bash -c 'printf "holder: intruder\ntoken: other\n" > "$FOREST_LOCK_DIR/owner"' 2>/dev/null; ok "$(cat "$D/owner" | head -1)" "holder: intruder" "someone else's owner isn't removed"; trash
$L r -- bash -c 'mv "$FOREST_LOCK_DIR/owner" "$FOREST_LOCK_DIR/../trash/moved"' 2>/dev/null; ok $(free) held "removed owner: directory left alone"; trash
$L r -- bash -c 'touch "$FOREST_LOCK_DIR/stray"; exit 5' 2>/dev/null; ok $? 5 "exit code kept when rmdir fails"; mv "$D/stray" "$T/trash/stray.$n"; trash

echo "== nested"
out=$($L outer -- bash -c "$L inner -- bash -c 'echo inner; exit 3'; echo rc=\$?" 2>/dev/null); ok "$out" "inner
rc=3" "nested call runs without waiting"
ok $(free) free "free after nested"

echo "== Windows: a stopped cmd.exe leaves ping running; the lock waits for it"
if command -v tasklist.exe >/dev/null && command -v cmd.exe >/dev/null; then
    winping() { tasklist.exe /FI "IMAGENAME eq PING.EXE" /NH | tr -d '\r' | grep -ci '^ping'; }
    (cd /mnt/c && FOREST_LOCK_WATCH=PING.EXE exec $L w -- cmd.exe /c ping -n 8 127.0.0.1 >/dev/null 2>&1) & W=$!
    sleep 3; kill -TERM $W; sleep 2
    ok $(free) held "after TERM, held while ping runs"
    grep -q '^note: .*PING.EXE' "$D/owner"; ok $? 0 "owner notes what it waits for"
    wait $W; ok $? 143 "wrapper exits 143 once ping is done"; ok $(winping) 0 "ping ended"; ok $(free) free "released"
    (cd /mnt/c && FOREST_LOCK_WATCH=PING.EXE exec $L w -- cmd.exe /c ping -n 10 127.0.0.1 >/dev/null 2>&1) & W=$!
    sleep 3; kill -TERM $W; sleep 2; kill -TERM $W; wait $W
    ok $? 143 "a second TERM gives up waiting"; ok $(free) free "released"
    grep -q 'released after a second signal while still running on Windows' "$FOREST_LOCK_DIR.log"; ok $? 0 "logged"
    sleep 6
else
    echo "  (skipped: no tasklist.exe / cmd.exe)"
fi

echo "== $pass passed, $fail failed"
(( fail == 0 ))
