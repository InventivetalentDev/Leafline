#!/usr/bin/env python3
"""Zip the packaged Windows build for sharing, without local state or debug symbols.

Leaves out:
  - AntTreeGame/Saved/, Engine/Saved/  (logs with local paths, the save game and config written by test runs)
  - *.pdb               (debug symbols; not needed to play)
  - Manifest_*.txt      (staging lists written by the packager)

Everything sits under one top-level folder in the zip, so unzipping never spills files.
Needs only Python 3.
"""

import argparse
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_BUILD = os.path.join(ROOT, "Build", "Windows")
DEFAULT_OUT = os.path.join(ROOT, "Build", "inventivetalent_Leafline.zip")
LIMIT_BYTES = 1024 ** 3  # the MegaJam caps the upload at 1 GB


def excluded(rel_path, keep_pdb):
    parts = rel_path.replace("\\", "/").split("/")
    if len(parts) >= 2 and parts[0] in ("AntTreeGame", "Engine") and parts[1] == "Saved":
        return True
    name = parts[-1]
    if name.startswith("Manifest_") and name.endswith(".txt") and len(parts) == 1:
        return True
    if not keep_pdb and name.lower().endswith(".pdb"):
        return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Zip the packaged build for sharing.")
    parser.add_argument("--build", default=DEFAULT_BUILD, help="packaged build folder (default: Build/Windows)")
    parser.add_argument("--out", default=DEFAULT_OUT, help="zip to write (default: Build/inventivetalent_Leafline.zip)")
    parser.add_argument("--folder", default="Leafline", help="top-level folder inside the zip (default: Leafline)")
    parser.add_argument("--keep-pdb", action="store_true", help="include debug symbols (about 360 MB more)")
    args = parser.parse_args()

    build = os.path.abspath(args.build)
    exe = os.path.join(build, "AntTreeGame.exe")
    if not os.path.isfile(exe):
        print(f"No packaged build at {build} (AntTreeGame.exe missing). Run Scripts/package_win64.bat first.", file=sys.stderr)
        return 2

    out = os.path.abspath(args.out)
    if out.startswith(build + os.sep):
        print("The zip must not be written inside the build folder.", file=sys.stderr)
        return 2
    os.makedirs(os.path.dirname(out), exist_ok=True)
    tmp = out + ".part"

    count = skipped = 0
    raw = 0
    with zipfile.ZipFile(tmp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
        for dirpath, dirnames, filenames in os.walk(build):
            dirnames.sort()
            for name in sorted(filenames):
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, build)
                if excluded(rel, args.keep_pdb):
                    skipped += 1
                    continue
                z.write(full, arcname=f"{args.folder}/{rel.replace(os.sep, '/')}")
                raw += os.path.getsize(full)
                count += 1
    os.replace(tmp, out)

    size = os.path.getsize(out)
    print(f"Wrote {out}")
    print(f"{count} files ({raw / 1024 ** 3:.2f} GB unpacked), {skipped} left out, zip {size / 1024 ** 3:.2f} GB")
    if size >= LIMIT_BYTES:
        print("Warning: the zip is 1 GB or larger, over the MegaJam upload limit.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
