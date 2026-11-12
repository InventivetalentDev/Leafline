#!/usr/bin/env python3
"""Summarise an Unreal CSV profiler capture (CsvProfile) as frame rates.

Skips the first frames (loading, the new day's tree growth, shader warm-up) and reports average fps, median,
1% low (99th percentile frame time) and, where the capture has them, the game, render and GPU times.
Needs only Python 3.
"""

import argparse
import csv
import statistics
import sys


def column(header, names):
    for n in names:
        if n in header:
            return header.index(n)
    return None


def main():
    parser = argparse.ArgumentParser(description="Summarise a CsvProfile capture.")
    parser.add_argument("csv")
    parser.add_argument("--skip", type=float, default=8.0, help="seconds to skip at the start (default 8)")
    parser.add_argument("--label", default="", help="name printed in front of the result")
    args = parser.parse_args()

    with open(args.csv, newline="", encoding="utf-8", errors="replace") as f:
        rows = list(csv.reader(f))
    if not rows:
        print(f"{args.label}: empty capture", file=sys.stderr)
        return 2
    header = rows[0]
    ft = column(header, ["FrameTime"])
    if ft is None:
        print(f"{args.label}: no FrameTime column", file=sys.stderr)
        return 2
    extra = {k: column(header, [k]) for k in ("GameThreadTime", "RenderThreadTime", "GPUTime")}

    frames, t = [], 0.0
    for r in rows[1:]:
        try:
            ms = float(r[ft])
        except (ValueError, IndexError):
            break  # metadata rows at the end of the file
        t += ms / 1000.0
        if t >= args.skip:
            frames.append(r)
    if len(frames) < 30:
        print(f"{args.label}: only {len(frames)} frames after skipping {args.skip} s", file=sys.stderr)
        return 2

    times = sorted(float(r[ft]) for r in frames)
    mean = statistics.fmean(times)
    p99 = times[min(len(times) - 1, int(len(times) * 0.99))]
    parts = [f"{len(times)} frames", f"avg {1000.0 / mean:.0f} fps", f"median {1000.0 / statistics.median(times):.0f} fps",
             f"1% low {1000.0 / p99:.0f} fps"]
    for k, i in extra.items():
        if i is None:
            continue
        vals = [float(r[i]) for r in frames if i < len(r) and r[i] not in ("", None)]
        if vals:
            parts.append(f"{k.replace('ThreadTime', '').replace('Time', '')} {statistics.fmean(vals):.1f} ms")
    print(f"{args.label}: " + ", ".join(parts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
