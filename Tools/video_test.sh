#!/usr/bin/env bash
# One short standalone run per tree, isolated save slot, shared Unreal lock, verified 1080p reference frames.
# Usage: Tools/video_test.sh [1|3] [--dry-run]
# 1 = first tree, seed 517192; 3 = two colony moves to the tall spire, seed 15161, day 3.
# Build this worktree first. ROOT, SEED and EXTRA can be supplied by the caller.
set -euo pipefail
ROOT=${ROOT:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)}
DAY=${1:-1}
[[ "$DAY" =~ ^[1-9][0-9]*$ ]] || { echo 'day must be a positive integer' >&2; exit 2; }
if (( DAY > 1 )); then
	SEED=${SEED:-15161}
	SETUP="Leaf.ResetSave,Leaf.OneTree 1,Leaf.Move,Leaf.Move,Leaf.StartDay $DAY $SEED"
else
	SEED=${SEED:-517192}
	SETUP="Leaf.ResetSave,Leaf.OneTree 1,Leaf.StartDay $DAY $SEED"
fi
CMDS="$SETUP,Leaf.Delay 1 Leaf.Cinematic 1"
CMDS+=",Leaf.Delay 2 Leaf.Teleport 0 0.2,Leaf.Delay 5 HighResShot 1"
CMDS+=",Leaf.Delay 6 Leaf.Shot orbit 8"
CMDS+=",Leaf.Delay 7.5 HighResShot 1,Leaf.Delay 10 HighResShot 1,Leaf.Delay 12.5 HighResShot 1"
CMDS+=",Leaf.Delay 15 Leaf.Shot crane 8"
CMDS+=",Leaf.Delay 16.5 HighResShot 1,Leaf.Delay 19 HighResShot 1,Leaf.Delay 21.5 HighResShot 1"
CMDS+=",Leaf.Delay 24 Leaf.Teleport 21 0.5,Leaf.Delay 26 Leaf.Shot dolly 8"
CMDS+=",Leaf.Delay 27.5 HighResShot 1,Leaf.Delay 30 HighResShot 1,Leaf.Delay 32.5 HighResShot 1"
CMDS+=",Leaf.Delay 35 Leaf.StartDay $DAY $SEED,Leaf.Delay 37 Leaf.Shot reveal 8"
CMDS+=",Leaf.Delay 39 HighResShot 1,Leaf.Delay 41 HighResShot 1,Leaf.Delay 43 HighResShot 1"
CMDS+=",Leaf.Delay 44.5 HighResShot 1,Leaf.Delay 45.5 HighResShot 1"
CMDS+=",Leaf.Delay 46 Leaf.Teleport 0 0.2,Leaf.Delay 48 Leaf.AutoMove 0.25,Leaf.Delay 48.1 Leaf.Shot follow 8"
CMDS+=",Leaf.Delay 49.5 HighResShot 1,Leaf.Delay 52 HighResShot 1,Leaf.Delay 54.5 HighResShot 1"
CMDS+=",Leaf.Delay 57 Leaf.AutoMove 0,Leaf.Delay 57 Leaf.Slomo 0.25,Leaf.Delay 58 Leaf.ShotList orbit dolly"
CMDS+=",Leaf.Delay 60 HighResShot 1,Leaf.Delay 63 HighResShot 1,Leaf.Delay 69 HighResShot 1,Leaf.Delay 73 HighResShot 1"
CMDS+=",Leaf.Delay 76 Leaf.Slomo 1,Leaf.Delay 76 Leaf.Cinematic 0,Leaf.Delay 77 HighResShot 1"
CMDS+=",Leaf.Delay 80 Leaf.Cinematic 1,Leaf.Delay 81 Leaf.Shot crane 4,Leaf.Delay 82 Leaf.Shot stop"
CMDS+=",Leaf.Delay 83 Leaf.Cinematic 0,Leaf.QuitAfter 90"
if [[ "${2:-}" == '--dry-run' ]]; then
	printf '%s\n' "$CMDS"
	exit 0
fi
RUN_GAME_WIN=$(wslpath -w "$ROOT/Scripts/RunGame.bat")
OUT="$ROOT/Saved/RefShots/video-day$DAY"
LOG="VideoTest-Day$DAY.log"
mkdir -p "$OUT" "$ROOT/Saved/Logs"
MARK="$OUT/.marker"
touch "$MARK"
save_sig() { if [[ -e "$ROOT/Saved/SaveGames/Leafline.sav" ]]; then sha256sum "$ROOT/Saved/SaveGames/Leafline.sav"; else echo absent; fi; }
SAVE_BEFORE=$(save_sig)
"$ROOT/Scripts/unreal-lock.sh" --status || true
(cd /mnt/c && "$ROOT/Scripts/unreal-lock.sh" "video-day$DAY" -- cmd.exe /c "$RUN_GAME_WIN" "$LOG" "$CMDS" \
	"-LeafSlot=VideoTest -unattended -ResX=1920 -ResY=1080 ${EXTRA:-}") >"$OUT/launch.log" 2>&1
[[ "$(save_sig)" == "$SAVE_BEFORE" ]] || { echo 'Owner save changed during run; stop and investigate.' >&2; exit 3; }
python3 - "$ROOT" "$DAY" "$MARK" "$SEED" <<'PY'
from pathlib import Path
import re
import shutil
import struct
import sys
root, day, marker = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
seed = sys.argv[4]
out = marker.parent
failures = []
labels = ['cinematic'] + [f'{s}-{i}' for s in ('orbit', 'crane', 'dolly', 'reveal') for i in (1, 2, 3)]
labels += ['reveal-hold-7_5s', 'reveal-player-return-8_5s'] + [f'follow-{i}' for i in (1, 2, 3)]
labels += ['list-orbit-1', 'list-orbit-2', 'list-dolly-1', 'list-dolly-2', 'hud-restored']
frames = sorted((p for p in (root/'Saved/Screenshots').rglob('*.png') if p.stat().st_mtime >= marker.stat().st_mtime), key=lambda p: (p.stat().st_mtime, p.name))
for i, p in enumerate(frames):
    with p.open('rb') as f:
        header = f.read(24)
    size = struct.unpack('>II', header[16:24]) if header[:8] == b'\x89PNG\r\n\x1a\n' else None
    if size != (1920, 1080): failures.append(f'{p.name}: expected 1920x1080, got {size}')
    target = out / f"{labels[i] if i < len(labels) else f'extra-{i}'}-day{day}.png"
    shutil.copy2(p, target)
    print(target)
log = (root/f'Saved/Logs/VideoTest-Day{day}.log').read_text(errors='replace')
for line in log.splitlines():
    if 'LogLeafline:' in line and any(s in line for s in ('Shot ', 'Cinematic ', 'Video slomo', 'Reveal plan:', 'Reveal candidate ', 'Reveal rail:', 'Reveal hold:', 'Reveal playback:')):
        print(line)
plans = re.findall(r'Reveal plan: height ([\d.]+) cm, pullback [\d.]+ cm \(cap 26000\), camera height ([\d.]+) cm \(1/3\), frame height ([\d.]+),', log)
if not plans: failures.append('missing reveal plan; check the worktree binaries')
if 'fallback-six 1' not in log:
    for height, camera_height, span in plans:
        if abs(float(camera_height) - float(height)/3) > 1 or abs(float(span) - 0.70) > 0.005:
            failures.append('reveal plan misses the one-third camera height or 70% framing target')
else:
    print('Reveal froze the six-second fallback composition; inspect its end hold.')
playback = re.findall(r'Reveal playback: max roll ([\d.]+) deg, stopped frames (\d+)', log)
if not playback: failures.append('missing step-5 reveal playback evidence; check the worktree binaries')
for roll, stopped in playback:
    if float(roll) > 0.01 or int(stopped): failures.append('reveal rolled or stopped on an invalid planned position')
hold_positions = re.findall(r'Reveal hold:.*position (V\([^\r\n]+\))', log)
end_positions = re.findall(r'Reveal playback:.*end (V\([^\r\n]+\));', log)
if not hold_positions or not end_positions or hold_positions[-1] != end_positions[-1]:
    failures.append('reveal end hold is missing or moved before handoff')
if 'entry hold: six-second pose unreachable' in log: failures.append('six-second fallback pose was unreachable')
for blockers in re.findall(r'Reveal plan:.*foreground blockers (\d+)', log):
    if int(blockers): print(f'Reveal used the fewest-blockers fallback ({blockers} foreground trees); inspect the silhouette.')
if int(day) > 1 and f'Day {day} starts: tree 3 (a tall spire, seed {seed})' not in log:
    failures.append('later-tree setup did not reach the tall spire with the requested seed')
if len(frames) != len(labels): failures.append(f'expected {len(labels)} frames, found {len(frames)}')
if re.search(r'Fatal error|Assertion failed|Unhandled Exception|Shot did not start|Shot list could not start|Shot: ', log):
    failures.append('runtime error or rejected shot; inspect the log')
for name in ('orbit', 'crane', 'dolly', 'reveal', 'follow'):
    if f'Shot {name} starts:' not in log: failures.append(f'{name} never started')
for cm, deg, fov in re.findall(r'return error ([\d.]+) cm / ([\d.]+) deg, FOV error ([\d.]+)', log):
    if max(float(cm), float(deg), float(fov)) > 0.01: failures.append('camera return mismatch')
for clearance in re.findall(r'min clearance ([\d.-]+) cm', log):
    if float(clearance) < 12: failures.append('camera clearance below 12 cm')
if failures:
    print('\n'.join(failures), file=sys.stderr)
    sys.exit(4)
print('Log checks passed. Inspect the frames for framing, wood, foliage and HUD; motion still needs a live recording check.')
PY
