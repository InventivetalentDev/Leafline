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
