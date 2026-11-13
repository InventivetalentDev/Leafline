#!/usr/bin/env bash
# Writes Content/Leafline/UIRaw/buildinfo.txt (commit, "+dirty" if the tree has uncommitted changes, and the time) for
# the HUD's faint bottom-left line. Staged with the UIRaw folder (UFS); git ignores it. Run it before packaging.
set -eu
ROOT=/mnt/d/UnrealProjects/AntTreeGame
cd "$ROOT"
HASH=$(git rev-parse --short HEAD)
DIRTY=$(git status --porcelain --untracked-files=no | grep -v 'buildinfo.txt' | head -1)
printf '%s%s %s\n' "$HASH" "${DIRTY:++dirty}" "$(date '+%d %b %H:%M')" > Content/Leafline/UIRaw/buildinfo.txt
cat Content/Leafline/UIRaw/buildinfo.txt
