#!/usr/bin/env python3
"""Cuts the MegaJam video together from takes filmed by Tools/video_capture.sh (docs/ship/video.md).

    python3 Tools/video_edit.py                       draft: docs/ship/video_cut.txt -> Saved/VideoCap/draft_720p.mp4
    python3 Tools/video_edit.py --final               1920x1080 at 60 fps -> Build/Video/leafline_megajam.mp4
    python3 Tools/video_edit.py --cut FILE --out FILE --res 1080 --fps 60

The cut list has one clip per line: `<take> <in> <out> | <caption>`. The take is a file name in Saved/VideoCap/shots
without .mp4; in and out are seconds within the take. The caption, if any, sits lower left for the clip's length,
fading in and out. Instead of a caption:
    @title TEXT          a large centred title fading in a second into the clip
    @end A / B / C       an end card: the picture dimmed, line A large, B and C below it
After <out>, options may follow: `zoom=1.5` enlarges the picture (a punch-in for small HUD text), `zoomto=1.8` moves
the zoom steadily from `zoom` to that value over the clip (a push-in), `cx=0.5 cy=0.45` set the zoom's centre as
fractions of the frame (default the middle). Captions are drawn after the zoom, at their usual size. `box=1` puts a
dark box behind the caption, for clips where it lies over HUD text.
Lines starting with # are comments. Clips are joined with hard cuts; the film fades in from black and out to black.
A take with sound (added by `video_capture.sh --sound`) keeps it; a silent take gets silence. The finished sound is
normalised to -14 LUFS. Needs only ffmpeg and ffprobe (WSL's, with drawtext).
"""
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = '/mnt/d/UnrealProjects/AntTreeGame'
SHOTS = f'{ROOT}/Saved/VideoCap/shots'
FONT = '/mnt/c/Program Files/Epic Games/UE_5.8/Engine/Content/Slate/Fonts/Roboto-Medium.ttf'


def run(args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f'ffmpeg failed: {" ".join(args[:12])} ...\n{r.stderr[-2000:]}')
    return r.stdout


def probe(path, entries):
    return run(['ffprobe', '-v', 'error', '-show_entries', entries, '-of', 'csv=p=0', path]).strip()


def parse(cut_path):
    clips = []
    for n, raw in enumerate(open(cut_path, encoding='utf-8'), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        head, _, text = line.partition('|')
        parts = head.split()
        opts = {'zoom': 1.0, 'zoomto': None, 'cx': 0.5, 'cy': 0.5, 'box': 0.0}
        while len(parts) > 3 and '=' in parts[-1]:
            k, v = parts.pop().split('=', 1)
            if k not in opts:
                sys.exit(f'{cut_path}:{n}: unknown option {k} (zoom, zoomto, cx, cy, box)')
            opts[k] = float(v)
        if len(parts) != 3:
            sys.exit(f'{cut_path}:{n}: expected "<take> <in> <out> [zoom=..] | caption", got: {line}')
        take, t_in, t_out = parts[0], float(parts[1]), float(parts[2])
        path = f'{SHOTS}/{take}.mp4'
        if not os.path.exists(path):
            sys.exit(f'{cut_path}:{n}: no take {path}')
        length = float(probe(path, 'format=duration'))
        if not 0 <= t_in < t_out <= length + 0.01:
            sys.exit(f'{cut_path}:{n}: {t_in}-{t_out} is outside {take} (0-{length:.2f} s)')
        clips.append({'take': take, 'path': path, 'in': t_in, 'out': t_out, 'text': text.strip(), **opts})
    if not clips:
        sys.exit(f'{cut_path}: no clips')
    return clips


def text_filters(clip, w, h, tmp, idx):
    """drawtext filters for the clip's caption, title or end card (text from files, so nothing needs escaping)."""
    d = clip['out'] - clip['in']
    text = clip['text']
    out = []

    def textfile(s, k):
        p = f'{tmp}/text{idx}_{k}.txt'
        with open(p, 'w', encoding='utf-8') as f:
            f.write(s)
        return p

    def fade(start, fade_in=0.3, fade_out=0.3):
        return (f"alpha='if(lt(t,{start}),0,if(lt(t,{start + fade_in}),(t-{start})/{fade_in},"
                f"if(gt(t,{d - fade_out}),max(0,({d}-t)/{fade_out}),1)))'")

    common = f"fontfile='{FONT}':fontcolor=white:shadowcolor=black@0.55:shadowx=2:shadowy=2"
    if text.startswith('@title '):
        out.append(f"drawtext={common}:textfile='{textfile(text[7:].strip(), 0)}':fontsize={int(h * 0.11)}"
                   f":x=(w-text_w)/2:y=(h-text_h)/2-{int(h * 0.05)}:{fade(1.0, 0.8)}")
    elif text.startswith('@end '):
        lines = [s.strip() for s in text[5:].split('/')]
        out.append(f"drawbox=x=0:y=0:w=iw:h=ih:color=black@0.30:t=fill")
        y = h * 0.38
        for k, s in enumerate(lines):
            size = int(h * (0.11 if k == 0 else 0.042))
            out.append(f"drawtext={common}:textfile='{textfile(s, k)}':fontsize={size}:x=(w-text_w)/2:y={int(y)}"
                       f":{fade(0.4 + 0.5 * k, 0.6)}")
            y += size * (1.45 if k == 0 else 1.6)
    elif text:
        box = f":box=1:boxcolor=black@0.6:boxborderw={int(h * 0.018)}" if clip.get('box') else ''
        out.append(f"drawtext={common}:textfile='{textfile(text, 0)}':fontsize={int(h * 0.045)}"
                   f":x={int(w * 0.06)}:y={int(h * 0.84)}:{fade(0.25)}{box}")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--cut', default=f'{ROOT}/docs/ship/video_cut.txt')
    ap.add_argument('--out')
    ap.add_argument('--res', type=int, default=720, help='output height: 720 for drafts, 1080 for the final')
    ap.add_argument('--fps', type=int, default=30)
    ap.add_argument('--final', action='store_true', help='1080p, 60 fps, into Build/Video/leafline_megajam.mp4')
    a = ap.parse_args()
    if a.final:
        a.res, a.fps = 1080, 60
        a.out = a.out or f'{ROOT}/Build/Video/leafline_megajam.mp4'
    a.out = a.out or f'{ROOT}/Saved/VideoCap/draft_{a.res}p.mp4'
    h = a.res
    w = h * 16 // 9
    clips = parse(a.cut)
    crf = '16' if a.res >= 1080 else '22'
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        segs = []
        for i, c in enumerate(clips):
            d = c['out'] - c['in']
            has_audio = bool(probe(c['path'], 'stream=codec_type').count('audio'))
            z0, z1 = c['zoom'], c['zoomto'] if c['zoomto'] else c['zoom']
            if z0 != 1.0 or z1 != 1.0:
                # zoompan works on whole pixels: feed it a picture twice the output size so a slow push-in does not judder.
                frames = max(1, round(d * a.fps))
                z = f"{z0}+({z1 - z0})*on/{frames}"
                scale = [f'fps={a.fps}', f'scale={w * 2}:{h * 2}:flags=lanczos',
                         f"zoompan=z='{z}':x='(iw-iw/zoom)*{c['cx']}':y='(ih-ih/zoom)*{c['cy']}':d=1:s={w}x{h}:fps={a.fps}"]
            else:
                scale = [f'scale={w}:{h}:flags=lanczos', f'fps={a.fps}']
            vf = ','.join(scale + text_filters(c, w, h, tmp, i) + ['format=yuv420p'])
            seg = f'{tmp}/seg{i:02d}.mp4'
            args = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-ss', f'{c["in"]}', '-t', f'{d}', '-i', c['path']]
            if not has_audio:
                args += ['-f', 'lavfi', '-t', f'{d}', '-i', 'anullsrc=r=48000:cl=stereo']
            args += ['-vf', vf, '-map', '0:v', '-map', '0:a' if has_audio else '1:a',
                     '-af', 'aresample=48000,aformat=channel_layouts=stereo',
                     '-c:v', 'libx264', '-preset', 'medium', '-crf', crf, '-c:a', 'pcm_s16le', '-t', f'{d}', seg.replace('.mp4', '.mov')]
            run(args)
            segs.append(seg.replace('.mp4', '.mov'))
            print(f'  {i + 1:2d}. {c["take"]:<16} {c["in"]:5.2f}-{c["out"]:5.2f}  ({d:4.2f} s)  {c["text"]}')
        lst = f'{tmp}/list.txt'
        with open(lst, 'w') as f:
            f.writelines(f"file '{s}'\n" for s in segs)
        joined = f'{tmp}/joined.mov'
        run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', lst, '-c', 'copy', joined])
        total = float(probe(joined, 'format=duration'))
        fade_out = max(0.0, total - 0.6)
        run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', joined,
             '-vf', f'fade=t=in:st=0:d=0.15,fade=t=out:st={fade_out:.3f}:d=0.6',
             '-af', f'loudnorm=I=-14:TP=-1.5:LRA=11,aresample=48000,afade=t=in:st=0:d=0.15,afade=t=out:st={fade_out:.3f}:d=0.6',
             '-c:v', 'libx264', '-preset', 'medium', '-crf', crf, '-pix_fmt', 'yuv420p', '-r', str(a.fps),
             '-c:a', 'aac', '-b:a', '256k', '-movflags', '+faststart', a.out])
    total = float(probe(a.out, 'format=duration'))
    size = probe(a.out, 'stream=width,height').splitlines()[0]
    verdict = 'OK' if 30 <= total <= 60 else 'OUTSIDE the jam\'s 30-60 s'
    print(f'{a.out}: {total:.2f} s ({verdict}), {size.replace(",", "x")}, {a.fps} fps')


if __name__ == '__main__':
    main()
