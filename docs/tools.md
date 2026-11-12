# tools

## strip_image_metadata.py

`Tools/strip_image_metadata.py` removes metadata (GPS location, camera model, timestamps, XMP, C2PA credentials and so on) from reference images. Run it on new photos before you commit or share them.

```
python3 Tools/strip_image_metadata.py            # everything under Art/Reference (subfolders too)
python3 Tools/strip_image_metadata.py PATH ...   # specific files or folders
python3 Tools/strip_image_metadata.py --check    # list what would be removed, change nothing
py Tools\strip_image_metadata.py                 # from Windows
```

- It needs only Python 3, with no extra libraries.
- It handles JPEG, PNG and WebP.
- It reports HEIC, TIFF, AVIF and GIF files as "not stripped". Convert those first, or turn off HEIC in the phone's camera settings.
- It doesn't re-compress anything, so the pixels stay identical. It keeps the colour profile, plus the rotation setting if a photo has one, so the image still displays correctly.
- On Pixel photos it also removes the Ultra HDR gain map, so those photos show as normal SDR images afterwards.
- It doesn't touch files that are already clean, so it's safe to re-run.
- It doesn't rename files. Pixel filenames like `PXL_20261112_081150350.jpg` still show the date and time each photo was taken.
- Exit codes: 0 = OK, 1 = `--check` found metadata, 2 = error or unsupported file.

## unreal-lock.sh

`Scripts/unreal-lock.sh` runs one command while holding this PC's Unreal lock, and always releases it, also when the command fails or is stopped. Only one Unreal process (build, cook, standalone game, editor) runs on this machine at a time; an editor takes about 12 GB idle and 20 GB in Play, and three at once crashed with "The paging file is too small". The lock is shared with other Unreal projects on this machine.

```
Scripts/unreal-lock.sh <holder> -- <command...>   # wait for the lock, run, release
Scripts/unreal-lock.sh --status                   # free, held (by whom, how long) or stale
Scripts/unreal-lock.sh --no-wait|--timeout <min>|--break-stale ...
```

- The lock is the directory `/mnt/d/UnrealProjects/wt/locks/unreal`; its `owner` file says who holds it. `FOREST_LOCK_DIR` overrides the location (the variable names come from the project the script was written for).
- Exit code: the command's own; 75 when held and not waiting; 76 when stale; `--status` gives 0 free, 1 held, 2 stale.
- Never take or release the lock by hand. A lock taken by hand is waited on, never broken.
- For the editor, the wrapped command must keep running until the editor exits, or the lock is released while it still runs. `cmd.exe /c start ""` should block that long, but that is untested with the editor.
- `Scripts/unreal-lock-test.sh` tests the script.

## RunGame.bat

`Scripts/RunGame.bat <logname> "<execcmds>"` starts the game standalone from the editor binaries, without packaging. Run it through `unreal-lock.sh`.
