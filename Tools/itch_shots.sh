#!/usr/bin/env bash
# The itch page screenshots, taken from the PACKAGED build (Build/Windows in the main checkout), so the page shows exactly
# what players download.
#
#   Tools/itch_shots.sh [label]          one run of Tools/itch_plans/main.txt -> Saved/ItchShots/<label>/<nn>-<name>.png
#   BIRD=1 Tools/itch_shots.sh [label]   also a second run of Tools/itch_plans/bird.txt (only if the bird ships)
#   PLAN=<file> Tools/itch_shots.sh ...  another plan instead of main.txt (same format)
#
# Plan format (as Tools/floorfar_shots.sh): one step per line, "<seconds>|<console command>" or "<seconds>|SHOT <name>"
# (HighResShot 1 at the window's 1920x1080); several "0|" lines run in order at launch; # starts a comment. Times are game
# seconds from launch. End a plan with "<seconds>|Leaf.QuitAfter 1".
#
# The run: AntTreeGame.exe -windowed -ResX=1920 -ResY=1080 -RenderOffscreen -nosplash (never takes the focus), save slot
# ItchShots (never the owner's colony), through Scripts/unreal-lock.sh. It waits while an AntTreeGame.exe is running (the
# owner playing), and backs up the packaged game's Saved/SaveGames and Saved/Config first and restores them after, also on
# failure. The shots get their metadata stripped (Tools/strip_image_metadata.py) and a contact sheet sheet.jpg (Pillow).
set -u
ROOT=/mnt/d/UnrealProjects/AntTreeGame          # the packaged build and the lock live in the main checkout
HERE=$(cd "$(dirname "$0")/.." && pwd)          # this tree: plans and tools
EXE='D:\UnrealProjects\AntTreeGame\Build\Windows\AntTreeGame.exe'
SAVED=$ROOT/Build/Windows/AntTreeGame/Saved
SHOTDIR=$SAVED/Screenshots
LOG=$SAVED/Logs/ItchShots.log
LABEL=${1:-$(date +%Y%m%d-%H%M)}
OUT=$ROOT/Saved/ItchShots/$LABEL                  # always the main checkout, whichever tree runs the script
PLAN=${PLAN:-$HERE/Tools/itch_plans/main.txt}

[ -e "$ROOT/Build/Windows/AntTreeGame.exe" ] || { echo "no packaged build in $ROOT/Build/Windows" >&2; exit 2; }
command -v tasklist.exe >/dev/null || { echo "tasklist.exe not reachable from WSL" >&2; exit 2; }
mkdir -p "$OUT" "$SHOTDIR"

# Never run while someone plays the packaged (or any) AntTreeGame.exe.
wait_free() {
	local said=0
	while tasklist.exe 2>/dev/null | grep -qi 'AntTreeGame.exe'; do
		[ $said = 0 ] && echo "AntTreeGame.exe is running (the owner playing?): waiting..." && said=1
		sleep 15
	done
}

BACKUP=$ROOT/Build/itchshots-backup-$(date +%Y%m%d-%H%M%S)
mkdir -p "$BACKUP"
for d in SaveGames Config; do [ -d "$SAVED/$d" ] && cp -a "$SAVED/$d" "$BACKUP/"; done
restore() {
	for d in SaveGames Config; do
		rm -rf "${SAVED:?}/$d"
		[ -d "$BACKUP/$d" ] && cp -a "$BACKUP/$d" "$SAVED/"
	done
	echo "Restored the packaged game's SaveGames and Config from $BACKUP"
}
trap restore EXIT

# The packaged game's own settings (Saved/Config/Windows/GameUserSettings.ini, restored after) win over -ResX/-ResY: set
# 1920x1080 windowed for the run. With the owner's 1280x720 windowed-fullscreen settings the shots came out 888x500.
GUS=$SAVED/Config/Windows/GameUserSettings.ini
if [ -f "$GUS" ]; then
	sed -i -E 's/^(ResolutionSizeX|LastUserConfirmedResolutionSizeX|DesiredScreenWidth|LastUserConfirmedDesiredScreenWidth)=.*/\1=1920/;
		s/^(ResolutionSizeY|LastUserConfirmedResolutionSizeY|DesiredScreenHeight|LastUserConfirmedDesiredScreenHeight)=.*/\1=1080/;
		s/^(FullscreenMode|LastConfirmedFullscreenMode|PreferredFullscreenMode)=.*/\1=2/' "$GUS"
fi

N=0   # running shot number across runs
run_plan() {
	local plan=$1 cmds="" views=() last=0 line t c step
	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in ''|'#'*) continue;; esac
		t=${line%%|*}; c=${line#*|}
		if [ "${c%% *}" = SHOT ]; then views+=("${c#SHOT }"); c="HighResShot 1"; fi
		if [ "$t" = 0 ]; then step="$c"; else step="Leaf.Delay $t $c"; fi
		if [ -z "$cmds" ]; then cmds="$step"; else cmds="$cmds,$step"; fi
		last=${t%.*}
	done < "$plan"
	local mark; mark=$(mktemp -p "${TMPDIR:-/tmp}" itchshots.XXXX)
	sleep 1
	wait_free
	local start=$SECONDS
	(cd /mnt/c && timeout $((last + 400)) "$ROOT/Scripts/unreal-lock.sh" itch-shots -- cmd.exe /c "$EXE" -windowed -ResX=1920 -ResY=1080 \
		-ForceRes -RenderOffscreen -nosplash -LeafSlot=ItchShots -log=ItchShots.log "-ExecCmds=r.SetRes 1920x1080w,$cmds" >/dev/null 2>&1)
	echo "$(basename "$plan"): game run took $((SECONDS - start)) s (${#views[@]} shots planned)"
	local i=0 f
	for f in $(find "$SHOTDIR" -name '*.png' -newer "$mark" | sort); do
		[ $i -ge ${#views[@]} ] && break
		N=$((N + 1))
		cp "$f" "$OUT/$(printf '%02d' $N)-${views[$i]}.png"
		i=$((i + 1))
	done
	rm -f "$mark"
	[ $i -lt ${#views[@]} ] && echo "WARNING: only $i of ${#views[@]} screenshots found (see $LOG)"
	grep -a "LogLeafline: \(Warning\|Error\)\|Fatal\|Shot did not start" "$LOG" 2>/dev/null | sed 's/^\[[^]]*\]\[[^]]*\]//' | sort | uniq -c | head -15
}

run_plan "$PLAN"
[ "${BIRD:-0}" = 1 ] && run_plan "$HERE/Tools/itch_plans/bird.txt"

python3 "$HERE/Tools/strip_image_metadata.py" "$OUT" >/dev/null || echo "WARNING: metadata strip reported a problem"
python3 - "$OUT" <<'EOF'
import sys, os
from PIL import Image, ImageDraw
out = sys.argv[1]
files = sorted(f for f in os.listdir(out) if f.endswith('.png'))
bad = [f for f in files if Image.open(os.path.join(out, f)).size != (1920, 1080)]
if bad:
    print('WARNING: not 1920x1080:', ', '.join(bad))
if not files:
    sys.exit('no screenshots')
w, h, cols = 480, 270, 3
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (cols * w, rows * h), (20, 20, 20))
for i, f in enumerate(files):
    im = Image.open(os.path.join(out, f)).convert('RGB').resize((w, h), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 8 * len(f) + 4, 13], fill=(0, 0, 0))
    d.text((2, 1), f, fill=(255, 230, 120))
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
sheet.save(os.path.join(out, 'sheet.jpg'), quality=88)
print('%d shots, sheet: %s' % (len(files), os.path.join(out, 'sheet.jpg')))
EOF
