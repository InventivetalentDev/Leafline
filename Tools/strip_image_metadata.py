#!/usr/bin/env python3
"""Strip identifying metadata from images without re-encoding them (stdlib only).

Usage (from the project root):
    python3 Tools/strip_image_metadata.py                   # default: Art/Reference, recursive
    python3 Tools/strip_image_metadata.py PATH [PATH ...]   # files and/or folders
    python3 Tools/strip_image_metadata.py --check [PATH...] # report only, change nothing

On Windows:  py Tools\\strip_image_metadata.py [PATH ...]

JPEG
    Keeps only what is needed to decode and display the image: the JFIF header,
    the ICC colour profile, the Adobe colour-transform marker, and the
    quantisation tables, Huffman tables, frame headers and scan data.
    Removes EXIF (GPS, camera model, timestamps, serials), XMP, IPTC and
    Photoshop data, C2PA/JUMBF content credentials, MPF, comments, and any data
    after the end of the main image (Ultra HDR gain maps, Motion Photo videos).
    The compressed image data is copied byte for byte, so the pixels do not change.

PNG
    Removes text, EXIF, timestamp and unknown ancillary chunks.
    Keeps chunks that affect rendering (transparency, gamma, colour profile...).

WebP
    Removes EXIF and XMP chunks and unknown chunks.

All formats: if the EXIF block had a non-default Orientation, the script writes
a minimal EXIF block that holds only that tag, so the image does not show up
rotated. Nothing else from the original EXIF is kept.

The script writes each changed file to a temporary file, then swaps it in.
It does not rewrite files that are already clean, so running it again is safe.

Exit codes: 0 = OK, 1 = --check found metadata, 2 = errors or unsupported files.
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
import tempfile
import zlib
from pathlib import Path

DEFAULT_TARGET = Path(__file__).resolve().parent.parent / "Art" / "Reference"

# Extensions picked up when scanning folders. Formats without a stripper are
# reported as unsupported instead of being silently ignored.
IMAGE_EXTS = {
    ".jpg", ".jpeg", ".jpe", ".jfif", ".png", ".webp",
    ".heic", ".heif", ".avif", ".tif", ".tiff", ".dng", ".gif", ".bmp",
}


class FormatError(Exception):
    pass


# --------------------------------------------------------------------------- EXIF


def exif_summary(tiff: bytes) -> tuple[int, bool]:
    """Return (orientation, has_gps) from a TIFF-structured EXIF payload."""
    if tiff[:2] == b"II":
        e = "<"
    elif tiff[:2] == b"MM":
        e = ">"
    else:
        return 1, False
    try:
        ifd = struct.unpack(e + "I", tiff[4:8])[0]
        count = struct.unpack(e + "H", tiff[ifd:ifd + 2])[0]
    except struct.error:
        return 1, False
    orientation, has_gps = 1, False
    for k in range(count):
        off = ifd + 2 + 12 * k
        if off + 12 > len(tiff):
            break
        tag, typ = struct.unpack(e + "HH", tiff[off:off + 4])
        if tag == 0x0112 and typ == 3:  # Orientation, SHORT
            value = struct.unpack(e + "H", tiff[off + 8:off + 10])[0]
            if 1 <= value <= 8:
                orientation = value
        elif tag == 0x8825:  # GPS IFD pointer
            has_gps = True
    return orientation, has_gps


def minimal_exif_tiff(orientation: int) -> bytes:
    """TIFF-structured EXIF block holding only the Orientation tag."""
    return (
        b"II*\x00" + struct.pack("<I", 8)          # header, IFD0 at offset 8
        + struct.pack("<H", 1)                       # 1 entry
        + struct.pack("<HHIHH", 0x0112, 3, 1, orientation, 0)
        + struct.pack("<I", 0)                       # no next IFD
    )


def minimal_exif_app1(orientation: int) -> bytes:
    payload = b"Exif\x00\x00" + minimal_exif_tiff(orientation)
    return b"\xff\xe1" + struct.pack(">H", len(payload) + 2) + payload


# --------------------------------------------------------------------------- JPEG

# Markers without a length field.
JPEG_STANDALONE = {0x01} | set(range(0xD0, 0xD8))


def classify_jpeg_app(marker: int, seg: bytes) -> tuple[bool, str]:
    """Return (keep, label) for an APPn or COM segment payload."""
    if marker == 0xE0 and seg.startswith(b"JFIF\x00"):
        return True, "JFIF"
    if marker == 0xE2 and seg.startswith(b"ICC_PROFILE\x00"):
        return True, "ICC"
    if marker == 0xEE and seg.startswith(b"Adobe"):
        return True, "Adobe"
    if marker == 0xE1 and seg.startswith(b"Exif\x00"):
        return False, "EXIF"
    if marker == 0xE1 and seg.startswith(b"http://ns.adobe.com/xap/1.0/\x00"):
        return False, "XMP"
    if marker == 0xE1 and seg.startswith(b"http://ns.adobe.com/xmp/extension/\x00"):
        return False, "XMP (extended)"
    if marker == 0xED:
        return False, "Photoshop/IPTC"
    if marker == 0xEB:
        return False, "C2PA/JUMBF"
    if marker == 0xE2 and seg.startswith(b"MPF\x00"):
        return False, "MPF"
    if marker == 0xE2 and seg.startswith(b"urn:iso:std:iso:ts:21496"):
        return False, "gain map info"
    if marker == 0xE0 and seg.startswith(b"JFXX\x00"):
        return False, "JFIF thumbnail"
    if marker == 0xFE:
        return False, "comment"
    return False, f"APP{marker - 0xE0}"


def strip_jpeg(data: bytes) -> tuple[bytes, list[str]]:
    if data[:2] != b"\xff\xd8":
        raise FormatError("not a JPEG")
    n = len(data)
    out: list[bytes] = [b"\xff\xd8"]
    removed: list[str] = []
    orientation = 1
    jfif_index = None
    pos = 2

    while True:
        if pos >= n:
            raise FormatError("truncated (no end-of-image marker)")
        if data[pos] != 0xFF:
            raise FormatError(f"expected a marker at byte {pos}")
        while pos < n and data[pos] == 0xFF:  # skip fill bytes
            pos += 1
        if pos >= n:
            raise FormatError("truncated")
        marker = data[pos]
        pos += 1

        if marker == 0xD9:  # EOI: end of the primary image
            out.append(b"\xff\xd9")
            break
        if marker == 0xD8:
            raise FormatError(f"unexpected start-of-image at byte {pos - 2}")
        if marker in JPEG_STANDALONE:
            out.append(bytes((0xFF, marker)))
            continue

        if pos + 2 > n:
            raise FormatError("truncated segment header")
        length = struct.unpack(">H", data[pos:pos + 2])[0]
        if length < 2 or pos + length > n:
            raise FormatError(f"bad segment length at byte {pos}")
        segment = bytes((0xFF, marker)) + data[pos:pos + length]
        payload = data[pos + 2:pos + length]
        pos += length

        if 0xE0 <= marker <= 0xEF or marker == 0xFE:
            keep, label = classify_jpeg_app(marker, payload)
            if keep and label == "JFIF" and jfif_index is not None:
                keep, label = False, "duplicate JFIF"
            if keep:
                if label == "JFIF":
                    jfif_index = len(out)
                out.append(segment)
            else:
                if label == "EXIF":
                    o, gps = exif_summary(payload[6:])
                    if o != 1:
                        orientation = o
                    if gps:
                        label = "EXIF (incl. GPS)"
                removed.append(label)
            continue

        # Structural segment (tables, frame header, scan header...): keep.
        out.append(segment)

        if marker == 0xDA:  # SOS: copy entropy-coded data up to the next real marker
            start = pos
            while True:
                j = data.find(b"\xff", pos)
                if j < 0 or j + 1 >= n:
                    raise FormatError("truncated scan data")
                nxt = data[j + 1]
                if nxt == 0x00 or 0xD0 <= nxt <= 0xD7:  # stuffed byte / restart marker
                    pos = j + 2
                elif nxt == 0xFF:  # fill byte before a marker
                    pos = j + 1
                else:
                    break
            out.append(data[start:j])
            pos = j

    trailing = n - pos
    if trailing:
        removed.append(f"{fmt_size(trailing)} after end of image (HDR gain map / motion photo / other)")

    if orientation != 1:
        out.insert(jfif_index + 1 if jfif_index is not None else 1, minimal_exif_app1(orientation))

    return b"".join(out), removed


# --------------------------------------------------------------------------- PNG

PNG_SIG = b"\x89PNG\r\n\x1a\n"
PNG_KEEP_ANCILLARY = {
    b"tRNS", b"gAMA", b"cHRM", b"sRGB", b"iCCP", b"sBIT", b"bKGD", b"pHYs",
    b"hIST", b"sPLT", b"acTL", b"fcTL", b"fdAT", b"cICP", b"mDCv", b"cLLI",
}


def strip_png(data: bytes) -> tuple[bytes, list[str]]:
    if not data.startswith(PNG_SIG):
        raise FormatError("not a PNG")
    n = len(data)
    out: list[bytes] = [PNG_SIG]
    removed: list[str] = []
    orientation = 1
    first_idat = None
    pos = len(PNG_SIG)
    while True:
        if pos + 8 > n:
            raise FormatError("truncated (no IEND chunk)")
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        end = pos + 12 + length
        if end > n:
            raise FormatError(f"truncated chunk {ctype!r}")
        critical = not (ctype[0] & 0x20)  # uppercase first letter
        if ctype == b"IDAT" and first_idat is None:
            first_idat = len(out)
        if critical or ctype in PNG_KEEP_ANCILLARY:
            out.append(data[pos:end])
        elif ctype in (b"tEXt", b"zTXt", b"iTXt"):
            keyword = data[pos + 8:end - 4].split(b"\x00", 1)[0].decode("latin-1", "replace")
            removed.append(f"text '{keyword}'")
        elif ctype == b"eXIf":
            o, gps = exif_summary(data[pos + 8:end - 4])
            if o != 1:
                orientation = o
            removed.append("EXIF (incl. GPS)" if gps else "EXIF")
        elif ctype == b"tIME":
            removed.append("timestamp")
        else:
            removed.append(ctype.decode("latin-1", "replace"))
        pos = end
        if ctype == b"IEND":
            break
    if n - pos:
        removed.append(f"{fmt_size(n - pos)} after end of image")
    if orientation != 1 and first_idat is not None:  # eXIf must come before IDAT
        body = b"eXIf" + minimal_exif_tiff(orientation)
        chunk = struct.pack(">I", len(body) - 4) + body + struct.pack(">I", zlib.crc32(body))
        out.insert(first_idat, chunk)
    return b"".join(out), removed


# --------------------------------------------------------------------------- WebP

WEBP_KEEP = {b"VP8 ", b"VP8L", b"VP8X", b"ALPH", b"ANIM", b"ANMF", b"ICCP"}


def strip_webp(data: bytes) -> tuple[bytes, list[str]]:
    if data[:4] != b"RIFF" or data[8:12] != b"WEBP":
        raise FormatError("not a WebP")
    n = len(data)
    end = min(8 + struct.unpack("<I", data[4:8])[0], n)
    chunks: list[bytes] = []
    removed: list[str] = []
    orientation = 1
    pos = 12
    while pos + 8 <= end:
        fourcc = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if pos + 8 + size > end:
            raise FormatError(f"truncated chunk {fourcc!r}")
        chunk_end = min(pos + 8 + size + (size & 1), end)
        if fourcc in WEBP_KEEP:
            chunk = data[pos:chunk_end]
            if fourcc == b"VP8X":  # clear the EXIF (0x08) and XMP (0x04) flags
                chunk = chunk[:8] + bytes((chunk[8] & ~0x0C,)) + chunk[9:]
            chunks.append(chunk)
        elif fourcc == b"EXIF":
            body = data[pos + 8:pos + 8 + size]
            if body.startswith(b"Exif\x00\x00"):
                body = body[6:]
            o, gps = exif_summary(body)
            if o != 1:
                orientation = o
            removed.append("EXIF (incl. GPS)" if gps else "EXIF")
        elif fourcc == b"XMP ":
            removed.append("XMP")
        else:
            removed.append(fourcc.decode("latin-1", "replace").strip())
        pos = chunk_end
    if n - end:
        removed.append(f"{fmt_size(n - end)} after end of image")
    vp8x = next((i for i, c in enumerate(chunks) if c[:4] == b"VP8X"), None)
    if orientation != 1 and vp8x is not None:
        # Keep orientation: EXIF chunk goes after the image data, flag back on.
        tiff = minimal_exif_tiff(orientation)
        chunks.append(b"EXIF" + struct.pack("<I", len(tiff)) + tiff)
        c = chunks[vp8x]
        chunks[vp8x] = c[:8] + bytes((c[8] | 0x08,)) + c[9:]
    body = b"WEBP" + b"".join(chunks)
    return b"RIFF" + struct.pack("<I", len(body)) + body, removed


# --------------------------------------------------------------------------- driver

STRIPPERS = {"JPEG": strip_jpeg, "PNG": strip_png, "WebP": strip_webp}


def detect(data: bytes) -> str | None:
    if data[:3] == b"\xff\xd8\xff":
        return "JPEG"
    if data.startswith(PNG_SIG):
        return "PNG"
    if data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        return "WebP"
    return None


def fmt_size(num: int) -> str:
    if num >= 1024 * 1024:
        return f"{num / (1024 * 1024):.1f} MB"
    if num >= 1024:
        return f"{num / 1024:.0f} KB"
    return f"{num} B"


def write_atomic(path: Path, data: bytes) -> None:
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix="." + path.name + ".", suffix=".tmp")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        try:
            shutil.copymode(path, tmp)
        except OSError:
            pass
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def collect(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for p in paths:
        if p.is_dir():
            files.extend(
                f for f in sorted(p.rglob("*"))
                if f.is_file() and f.suffix.lower() in IMAGE_EXTS and not f.name.startswith(".")
            )
        elif p.is_file():
            files.append(p)
        else:
            print(f"MISSING   {p}", file=sys.stderr)
    return files


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Strip metadata (EXIF/GPS, XMP, C2PA, ...) from JPEG, PNG and WebP files losslessly.")
    parser.add_argument("paths", nargs="*", type=Path,
                        help=f"files or folders (default: {DEFAULT_TARGET})")
    parser.add_argument("--check", action="store_true",
                        help="only report what would be removed; do not modify files")
    args = parser.parse_args()

    targets = args.paths or [DEFAULT_TARGET]
    if not args.paths:
        print(f"No paths given, using {DEFAULT_TARGET}")

    files = collect(targets)
    problems = sum(1 for p in targets if not p.exists())
    dirty = 0
    for path in files:
        try:
            data = path.read_bytes()
            kind = detect(data)
            if kind is None:
                print(f"SKIPPED   {path}: unsupported format, metadata NOT removed", file=sys.stderr)
                problems += 1
                continue
            new, removed = STRIPPERS[kind](data)
        except (OSError, FormatError) as exc:
            print(f"ERROR     {path}: {exc}", file=sys.stderr)
            problems += 1
            continue

        if new == data:
            print(f"clean     {path}")
            continue

        dirty += 1
        summary = f"{fmt_size(len(data))} -> {fmt_size(len(new))}; removed: {', '.join(dict.fromkeys(removed))}"
        if args.check:
            print(f"HAS META  {path}  ({summary})")
            continue
        try:
            write_atomic(path, new)
        except OSError as exc:
            print(f"ERROR     {path}: could not write: {exc}", file=sys.stderr)
            problems += 1
            continue
        print(f"stripped  {path}  ({summary})")

    if not files:
        print("No image files found.")
    if problems:
        return 2
    if args.check and dirty:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
