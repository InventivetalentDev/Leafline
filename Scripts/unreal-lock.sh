#!/usr/bin/env bash
# unreal-lock.sh: run one command while holding the machine's Unreal lock, and always release it.
#
#   Scripts/unreal-lock.sh [--no-wait] [--timeout <min>] [--break-stale] <holder> -- <command...>
#   Scripts/unreal-lock.sh --status
#
# At most UNREAL_LOCK_SLOTS Unreal processes (build, cook, headless run, game, editor) run on this PC at a time
# (default 2, since 14 Nov; it was 1). Slot 1 is the directory /mnt/d/UnrealProjects/wt/locks/unreal
# (FOREST_LOCK_DIR overrides it), slot N the same path with "-N" appended: whoever creates a slot's directory
# holds that slot, its file "owner" says who. Copies of this script with one slot only ever take slot 1. This script waits for it, writes the
# owner, runs the command, and releases in a trap, also when the command fails or is interrupted.
# Details, exit codes and examples: docs/tools.md.

set -u

PROG=unreal-lock
BASE_DIR="${FOREST_LOCK_DIR:-/mnt/d/UnrealProjects/wt/locks/unreal}"
BASE_DIR="${BASE_DIR%/}"
SLOTS="${UNREAL_LOCK_SLOTS:-2}"
[[ $SLOTS =~ ^[1-9]$ ]] || SLOTS=1
LOCK_DIR="$BASE_DIR"
OWNER="$LOCK_DIR/owner"
LOG="${FOREST_LOCK_LOG:-$BASE_DIR.log}"
# set_slot <n>: point LOCK_DIR and OWNER at slot n.
set_slot() { if (( $1 == 1 )); then LOCK_DIR="$BASE_DIR"; else LOCK_DIR="$BASE_DIR-$1"; fi; OWNER="$LOCK_DIR/owner"; }
POLL="${FOREST_LOCK_POLL:-10}"        # seconds between tries while waiting
REPORT="${FOREST_LOCK_REPORT:-180}"   # seconds between "still waiting" lines
# Windows programs the lock is about. Stopping this script stops the command's WSL side, but a Windows process it
# started (an editor, a build) keeps running, so after a signal the lock is held until the ones that weren't
# running when the lock was taken have exited. Empty: don't check. UBT and UAT run as dotnet.exe, which is left out
# because other tools (editors' C# servers) run it too; their compilers (cl.exe, link.exe) are in.
WATCH="${FOREST_LOCK_WATCH-UnrealEditor.exe UnrealEditor-Cmd.exe UnrealBuildTool.exe AutomationTool.exe ShaderCompileWorker.exe UnrealPak.exe cl.exe link.exe AntTreeGame.exe AntTreeGame-Cmd.exe AntTreeGame-Win64-Shipping.exe ForestDrivingGame.exe ForestDrivingGame-Cmd.exe ForestDrivingGame-Win64-Shipping.exe}"

EX_USAGE=64
EX_HELD=75    # held by someone else: --no-wait, or --timeout ran out
EX_STALE=76   # the holder is gone without releasing; --break-stale takes it over

say() { printf '%s: %s\n' "$PROG" "$*" >&2; }

usage() {
    cat >&2 <<'EOF'
Usage:
  unreal-lock.sh [options] <holder> -- <command...>   wait for the Unreal lock, run, release
  unreal-lock.sh --status                             who holds it, for how long, alive or stale

Options:
  --no-wait         fail at once (exit 75) if the lock is held
  --timeout <min>   give up (exit 75) after waiting this many minutes (decimals allowed)
  --break-stale     if the holder's process is gone without releasing, take the lock over
  -h, --help        this text

Exit code: the command's own; 128+N if this script got signal N; 75 lock held (no wait or
timeout); 76 lock stale (see --break-stale); 64 usage. --status: 0 free, 1 held, 2 stale.
Environment: FOREST_LOCK_DIR (lock directory), FOREST_LOCK_POLL, FOREST_LOCK_REPORT (seconds),
FOREST_LOCK_WATCH (Windows images to wait for after a signal).
See docs/tools.md.
EOF
}

# --- reading the owner file -------------------------------------------------------------------

# field <name> <content>: the value of a "name: value" line.
field() { printf '%s\n' "$2" | sed -n "s/^$1: //p" | head -n 1; }

BOOT_ID=$(cat /proc/sys/kernel/random/boot_id 2>/dev/null || echo unknown)
PIDNS=$(readlink /proc/self/ns/pid 2>/dev/null || echo unknown)

# pid_start <pid>: the process's start time in clock ticks since boot (field 22 of /proc/PID/stat),
# so a PID number reused by a new process isn't taken for the holder.
pid_start() {
    local stat
    stat=$(cat "/proc/$1/stat" 2>/dev/null) || return 1
    [[ -n $stat ]] || return 1
    stat=${stat##*) }          # the command name (field 2) may hold spaces; cut up to its ')'
    # shellcheck disable=SC2086
    set -- $stat               # now $1 is field 3, so field 22 is ${20}
    printf '%s\n' "${20}"
}

OWNER_CONTENT=""
read_owner() { OWNER_CONTENT=$(cat "$OWNER" 2>/dev/null) || OWNER_CONTENT=""; }

# check_holder: sets HSTATE (alive | stale | unknown) and HWHY from OWNER_CONTENT.
check_holder() {
    local pid boot ns start now_start
    pid=$(field pid "$OWNER_CONTENT")
    boot=$(field boot "$OWNER_CONTENT")
    ns=$(field pidns "$OWNER_CONTENT")
    start=$(field pidstart "$OWNER_CONTENT")
    if [[ ! $pid =~ ^[0-9]+$ ]]; then
        HSTATE=unknown; HWHY="no PID recorded (taken by hand?), so it can't be checked"; return
    fi
    if [[ -z $boot || -z $ns ]]; then
        HSTATE=unknown; HWHY="PID $pid recorded without its WSL instance, so it can't be checked"; return
    fi
    if [[ $boot != "$BOOT_ID" ]]; then
        HSTATE=stale; HWHY="taken before WSL last restarted (PID $pid is long gone)"; return
    fi
    if [[ $ns != "$PIDNS" ]]; then
        HSTATE=unknown; HWHY="taken in another WSL distro or PID namespace; PID $pid can't be checked from here"; return
    fi
    if ! now_start=$(pid_start "$pid"); then
        HSTATE=stale; HWHY="its process (PID $pid) is no longer running"; return
    fi
    if [[ -n $start && $now_start != "$start" ]]; then
        HSTATE=stale; HWHY="its process (PID $pid) is no longer running (the number belongs to a newer process)"; return
    fi
    HSTATE=alive; HWHY="its process (PID $pid) is running"
}

# held_seconds: how long the current holder has had the lock.
held_seconds() {
    local e
    e=$(field epoch "$OWNER_CONTENT")
    [[ $e =~ ^[0-9]+$ ]] || e=$(stat -c %Y "$OWNER" 2>/dev/null || stat -c %Y "$LOCK_DIR" 2>/dev/null || date +%s)
    e=$(( $(date +%s) - e )); echo $(( e < 0 ? 0 : e ))
}

fmt_dur() {
    local s=$1
    if (( s < 60 )); then echo "${s} s"
    elif (( s < 3600 )); then echo "$(( s / 60 )) min"
    else printf '%d h %02d min\n' $(( s / 3600 )) $(( s % 3600 / 60 )); fi
}

# describe: sets DESC to one line about the current holder, or "free" (reads the owner file;
# sets HSTATE / HWHY too). Not called in $(...), so those globals survive.
describe() {
    read_owner
    if [[ -z $OWNER_CONTENT ]]; then
        HSTATE=unknown; HWHY="no owner file"
        if [[ -d $LOCK_DIR ]]; then
            DESC="(nobody named), held for $(fmt_dur "$(held_seconds)") with no owner file (being taken right now, or taken by hand without one; if it stays like this, nobody holds it: look inside, then rmdir $LOCK_DIR)"
        else
            DESC=free
        fi
        return
    fi
    local who cmd since note
    who=$(field holder "$OWNER_CONTENT")
    if [[ -z $who ]]; then
        who="\"$(printf '%s\n' "$OWNER_CONTENT" | head -n 1)\" (owner file written by hand)"
    fi
    check_holder
    cmd=$(field command "$OWNER_CONTENT")
    since=$(field started "$OWNER_CONTENT")
    note=$(field note "$OWNER_CONTENT")
    DESC="$who, held for $(fmt_dur "$(held_seconds)")${since:+ (since $since)}; $HWHY${cmd:+; running: $cmd}${note:+; NOTE: $note}"
}

log_event() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$LOG" 2>/dev/null || true; }

# --- options ----------------------------------------------------------------------------------

NO_WAIT=0
BREAK_STALE=0
TIMEOUT_S=-1
STATUS=0
while (( $# > 0 )); do
    case $1 in
        --no-wait) NO_WAIT=1; shift ;;
        --break-stale) BREAK_STALE=1; shift ;;
        --status) STATUS=1; shift ;;
        --timeout)
            (( $# >= 2 )) || { say "--timeout needs minutes"; usage; exit $EX_USAGE; }
            TIMEOUT_S=$(awk -v m="$2" 'BEGIN { if (m !~ /^[0-9]*\.?[0-9]+$/) exit 1; printf "%d", m * 60 }') \
                || { say "--timeout: '$2' is not a number of minutes"; exit $EX_USAGE; }
            shift 2 ;;
        --timeout=*)
            TIMEOUT_S=$(awk -v m="${1#--timeout=}" 'BEGIN { if (m !~ /^[0-9]*\.?[0-9]+$/) exit 1; printf "%d", m * 60 }') \
                || { say "--timeout: '${1#--timeout=}' is not a number of minutes"; exit $EX_USAGE; }
            shift ;;
        -h|--help) usage; exit 0 ;;
        --) break ;;
        -*) say "unknown option $1"; usage; exit $EX_USAGE ;;
        *) break ;;
    esac
done

if (( STATUS )); then
    anyfree=0; anystale=0
    for (( i = 1; i <= SLOTS; i++ )); do
        set_slot $i
        describe
        pre=""; (( SLOTS > 1 )) && pre="slot $i: "
        if [[ $DESC == free ]]; then echo "${pre}free ($LOCK_DIR)"; anyfree=1; continue; fi
        echo "${pre}held by $DESC"
        [[ $HSTATE == stale ]] && { echo "${pre}STALE: run with --break-stale to take it over (see docs/tools.md)"; anystale=1; }
    done
    (( anystale )) && exit 2
    (( anyfree )) && exit 0
    exit 1
fi

(( $# >= 1 )) && [[ $1 != -- ]] || { say "missing <holder>"; usage; exit $EX_USAGE; }
HOLDER=$(printf '%s' "$1" | tr '\n\r' '  ')
shift
[[ ${1:-} == -- ]] || { say "expected -- before the command"; usage; exit $EX_USAGE; }
shift
(( $# >= 1 )) || { say "missing the command after --"; usage; exit $EX_USAGE; }

# Inside a command that already runs under this lock (a wrapped script calling the wrapper again):
# run directly instead of waiting for ourselves.
if [[ -n ${FOREST_LOCK_TOKEN:-} ]]; then
    for (( i = 1; i <= SLOTS; i++ )); do
        set_slot $i
        read_owner
        if [[ -n $OWNER_CONTENT && $(field token "$OWNER_CONTENT") == "$FOREST_LOCK_TOKEN" ]]; then
            say "already held by this command's caller ($(field holder "$OWNER_CONTENT")); running without taking it again"
            exec "$@"
        fi
    done
fi

# --- taking and releasing ---------------------------------------------------------------------

TOKEN="$$-$(date +%s%N)-$RANDOM$RANDOM"
TAKEN=0          # we created the lock directory
WROTE_OWNER=0    # and wrote our owner file into it
TAKEN_AT=0
COMMAND_LINE=$(printf '%q ' "$@"); COMMAND_LINE=${COMMAND_LINE% }

release() {
    (( TAKEN )) || return 0
    TAKEN=0
    local c rc_msg
    c=$(cat "$OWNER" 2>/dev/null) || c=""
    if (( ! WROTE_OWNER )); then
        # Interrupted between mkdir and writing the owner: the directory is still ours.
        rm -f -- "${LOCK_DIR:?}/.owner.$$"
        rmdir -- "${LOCK_DIR:?}" 2>/dev/null
        return 0
    fi
    if [[ -z $c ]]; then
        say "NOT releasing: our owner file is gone (someone removed it while '$HOLDER' held the lock); left $LOCK_DIR alone"
        log_event "$HOLDER: owner file gone at release; left alone"
        return 0
    fi
    if [[ $(field token "$c") != "$TOKEN" ]]; then
        say "NOT releasing: the lock now belongs to $(field holder "$c") (someone broke it while '$HOLDER' held it)"
        log_event "$HOLDER: lock belonged to $(field holder "$c") at release; left alone"
        return 0
    fi
    rm -f -- "${LOCK_DIR:?}/owner"
    rc_msg="released by $HOLDER after $(fmt_dur $(( $(date +%s) - TAKEN_AT )))"
    if rmdir -- "${LOCK_DIR:?}" 2>/dev/null; then
        say "$rc_msg"
        log_event "$rc_msg"
    else
        say "WARNING: removed the owner file but $LOCK_DIR isn't empty, so it is still locked: look inside and remove it by hand"
        log_event "$rc_msg, but the directory wasn't empty"
    fi
}

# write_owner [note]: (re)write our owner file in one rename, so readers never see half of it.
write_owner() {
    local tmp="$LOCK_DIR/.owner.$$"
    {
        printf 'holder: %s\n' "$HOLDER"
        printf 'started: %s\n' "$(date '+%Y-%m-%d %H:%M:%S %z' -d "@$TAKEN_AT")"
        printf 'epoch: %s\n' "$TAKEN_AT"
        printf 'command: %s\n' "$COMMAND_LINE"
        printf 'cwd: %s\n' "$PWD"
        printf 'pid: %s\n' "$$"
        printf 'pidstart: %s\n' "$(pid_start $$)"
        printf 'pidns: %s\n' "$PIDNS"
        printf 'boot: %s\n' "$BOOT_ID"
        printf 'host: %s %s\n' "$(hostname 2>/dev/null)" "${WSL_DISTRO_NAME:-}"
        printf 'token: %s\n' "$TOKEN"
        [[ -n ${1:-} ]] && printf 'note: %s\n' "$1"
        printf 'via: Scripts/unreal-lock.sh (releases by itself; see docs/tools.md)\n'
    } > "$tmp" && mv -f -- "$tmp" "$OWNER"
}

try_take() {
    local i got=0
    for (( i = 1; i <= SLOTS; i++ )); do
        set_slot $i
        if mkdir -- "$LOCK_DIR" 2>/dev/null; then got=1; break; fi
    done
    (( got )) || return 1
    TAKEN=1
    TAKEN_AT=$(date +%s)
    write_owner || {
        say "could not write $OWNER"
        rm -f -- "${LOCK_DIR:?}/.owner.$$"
        rmdir -- "${LOCK_DIR:?}" 2>/dev/null
        TAKEN=0
        exit 70
    }
    WROTE_OWNER=1
    log_event "taken by $HOLDER (pid $$): $COMMAND_LINE"
    return 0
}

# windows_watched: "PID Image" of every running Windows process whose image is in WATCH (nothing without tasklist.exe).
windows_watched() {
    [[ -n $WATCH ]] && command -v tasklist.exe >/dev/null 2>&1 || return 0
    timeout 30 tasklist.exe /FO CSV /NH 2>/dev/null | tr -d '\r' | awk -F'","' -v watch="$WATCH" '
        BEGIN { n = split(tolower(watch), w, " "); for (i = 1; i <= n; i++) want[w[i]] = 1 }
        { img = $1; sub(/^"/, "", img); if (tolower(img) in want) print $2, img }'
}

# windows_new: the watched processes that weren't running when the lock was taken (WIN_SNAPSHOT).
windows_new() {
    windows_watched | awk -v snap="$WIN_SNAPSHOT" '
        BEGIN { n = split(snap, s, " "); for (i = 1; i <= n; i++) old[s[i]] = 1 }
        !($1 in old) { printf "%s%s (%s)", sep, $2, $1; sep = ", " }'
}

# break_stale: move the stale lock aside (one rename, so two breakers can't both win), check that
# what we moved is still the lock we judged stale, then remove it.
break_stale() {
    local judged=$OWNER_CONTENT why=$HWHY aside="$LOCK_DIR.stale.$$"
    mv -T -- "$LOCK_DIR" "$aside" 2>/dev/null || return 1
    if [[ "$(cat "$aside/owner" 2>/dev/null)" != "$judged" ]]; then
        mv -T -- "$aside" "$LOCK_DIR" 2>/dev/null \
            || say "ERROR: moved a lock that changed under us to $aside and couldn't put it back: move it back by hand"
        return 1
    fi
    say "broke the stale lock of $(field holder "$judged"): $why"
    log_event "stale lock of $(field holder "$judged") broken by $HOLDER: $why"
    rm -f -- "${aside:?}/owner"
    rmdir -- "${aside:?}" 2>/dev/null || say "left $aside behind (not empty): look inside and remove it by hand"
    return 0
}

CHILD=0
SIGNALED=0
HOLDING_WINDOWS=""
on_signal() {   # on_signal <name> <number>
    if [[ -n $HOLDING_WINDOWS ]]; then
        say "SIG$1 again: releasing the lock although these Windows processes still run: $HOLDING_WINDOWS"
        say "WARNING: nobody holds the lock for them now. Stop them (Task Manager) or tell whoever takes the lock next."
        log_event "$HOLDER released after a second signal while still running on Windows: $HOLDING_WINDOWS"
        exit $(( 128 + $2 ))
    fi
    if (( CHILD > 0 )) && kill -0 "$CHILD" 2>/dev/null; then
        if (( SIGNALED )); then
            say "SIG$1 again: killing the command (pid $CHILD)"
            kill -KILL "$CHILD" 2>/dev/null
        else
            say "SIG$1: stopping the command (pid $CHILD); the lock is released once it has ended"
            kill -TERM "$CHILD" 2>/dev/null
        fi
        SIGNALED=$2
    else
        exit $(( 128 + $2 ))    # the EXIT trap releases
    fi
}
trap release EXIT
trap 'on_signal INT 2' INT
trap 'on_signal TERM 15' TERM
trap 'on_signal HUP 1' HUP

mkdir -p -- "$(dirname -- "$BASE_DIR")" 2>/dev/null

WAIT_START=$(date +%s)
LAST_REPORT=0
while ! try_take; do
    # Every slot is held: describe them all (a stale one is handled below; with several, the last stale slot).
    line=""; stale_slot=0; freed=0
    for (( i = 1; i <= SLOTS; i++ )); do
        set_slot $i
        describe
        if [[ $DESC == free ]]; then freed=1; break; fi
        line="${line:+$line | }${DESC}"
        [[ $HSTATE == stale ]] && { stale_slot=$i; stale_owner=$OWNER_CONTENT; stale_why=$HWHY; }
    done
    (( freed )) && continue                             # released between our mkdir and the read
    if (( stale_slot )); then
        set_slot $stale_slot; OWNER_CONTENT=$stale_owner; HSTATE=stale; HWHY=$stale_why
    fi
    if [[ $HSTATE == stale ]]; then
        if (( BREAK_STALE )); then
            break_stale || sleep 1
            continue
        fi
        say "STALE lock, held by $line"
        say "Its holder is gone without releasing. Check that nothing it started still runs on Windows"
        say "(UnrealEditor, UnrealEditor-Cmd, UnrealBuildTool, cl.exe, link.exe in the Task Manager),"
        say "then run again with --break-stale to take it over."
        exit $EX_STALE
    fi
    if (( NO_WAIT )); then
        say "held by $line; not waiting (--no-wait)"
        exit $EX_HELD
    fi
    now=$(date +%s)
    if (( TIMEOUT_S >= 0 && now - WAIT_START >= TIMEOUT_S )); then
        say "gave up after $(fmt_dur $(( now - WAIT_START ))) (--timeout); still held by $line"
        exit $EX_HELD
    fi
    if (( LAST_REPORT == 0 )); then
        say "waiting for $LOCK_DIR, held by $line"
        LAST_REPORT=$now
    elif (( now - LAST_REPORT >= REPORT )); then
        say "still waiting ($(fmt_dur $(( now - WAIT_START )))), held by $line"
        LAST_REPORT=$now
    fi
    sleep "$POLL" & wait $!
done

(( LAST_REPORT )) && say "got the lock after $(fmt_dur $(( $(date +%s) - WAIT_START )))"
say "holding the lock as $HOLDER: $COMMAND_LINE"

WIN_SNAPSHOT=$(windows_watched | awk '{ printf "%s ", $1 }')
export FOREST_LOCK_TOKEN="$TOKEN"
"$@" <&0 &      # in the background so the traps run while it works; <&0 keeps our stdin
CHILD=$!
while :; do
    wait "$CHILD"
    rc=$?
    kill -0 "$CHILD" 2>/dev/null || break   # still running: wait was cut short by a signal
done
CHILD=0

if (( SIGNALED )); then
    # The command's WSL side is gone, but a Windows program it started keeps running when its WSL side is stopped:
    # hold the lock until the watched processes that started after the lock was taken have exited.
    last=0
    while :; do
        HOLDING_WINDOWS=$(windows_new)
        [[ -n $HOLDING_WINDOWS ]] || break
        now=$(date +%s)
        if (( last == 0 )); then
            say "the command stopped, but Windows processes started under the lock still run: $HOLDING_WINDOWS"
            say "holding the lock until they exit (another signal releases it anyway)"
            write_owner "the command was stopped; waiting for its Windows processes to exit: $HOLDING_WINDOWS" 2>/dev/null
            log_event "$HOLDER: command stopped by a signal; holding for $HOLDING_WINDOWS"
            last=$now
        elif (( now - last >= REPORT )); then
            say "still holding the lock for: $HOLDING_WINDOWS"
            write_owner "the command was stopped; waiting for its Windows processes to exit: $HOLDING_WINDOWS" 2>/dev/null
            last=$now
        fi
        sleep "$POLL" & wait $!     # in the background, so a signal is handled at once
    done
    HOLDING_WINDOWS=""
    exit $(( 128 + SIGNALED ))
fi
exit "$rc"
