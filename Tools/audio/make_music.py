"""Leafline background music options. Pure synthesis (numpy only), fixed seeds, all in C major / C pentatonic.
Every option is a seamless stereo loop: the buffer is circular (notes that ring past the end wrap to the start,
sustained tones use windows that are zero at their edges, filters are circular FFT filters).
Run: uv run --with numpy python Tools/audio/make_music.py <outdir> [A B C D]   (listening renders and previews)
     uv run --with numpy python Tools/audio/make_music.py Art/Audio --game  (music_a/b/c.wav for the game; copy them to
     Content/Leafline/AudioRaw as .wavraw; day 1 plays A, day 2 B, day 3 C, day 4 A again)
Writes <letter>-<name>.wav (normalised to about -20 LUFS, true peak <= -3 dBTP) and <letter>-preview-with-wind.wav
(20 s at in-game volumes while the music plays: music 0.25, wind_calm_loop 0.125, leaves_rustle_loop 0.047;
FWindTuning MusicVolume, CalmVolumeWithMusic, RustleVolumeWithMusic: the calm bed sits about 3.5 dB under its
no-music level of 0.19 / 0.07, owner's note on the first previews, 15 Nov)."""
import sys, os, wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_audio import SR, TAU, N, tarr, note, band, pink, rng_for, hash_str  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "Saved", "MusicOptions")
WIND_DIR = os.path.join(HERE, "..", "..", "Art", "Audio")
TARGET_LUFS = -20.0
TP_MAX_DB = -3.2
MUSIC_VOL, CALM_WITH_MUSIC, RUSTLE_WITH_MUSIC = 0.25, 0.125, 0.047   # in-game volumes for the previews

# ---------- loudness (ITU-R BS.1770 K-weighting, gated), true peak (4x oversampled) ----------
def _biquad_mag(b, a, f):
    z = np.exp(-1j * TAU * f / SR)
    return np.abs((b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z))

def kweight(x):
    f = np.fft.rfftfreq(len(x), 1 / SR)
    m = _biquad_mag([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585], f)
    m *= _biquad_mag([1.0, -2.0, 1.0], [1, -1.99004745483398, 0.99007225036621], f)
    return np.fft.irfft(np.fft.rfft(x, axis=0) * m[:, None], len(x), axis=0)

def lufs(s):
    k = kweight(s); blk, hop = N(0.4), N(0.1)
    ms = np.array([np.sum(np.mean(k[i:i + blk] ** 2, axis=0)) for i in range(0, len(k) - blk, hop)])
    l = -0.691 + 10 * np.log10(ms + 1e-20)
    g = ms[l > -70]; rel = -0.691 + 10 * np.log10(np.mean(g)) - 10
    g = ms[(l > -70) & (l > rel)]
    return -0.691 + 10 * np.log10(np.mean(g))

def true_peak(s, os_=4):
    n = len(s); X = np.fft.rfft(s, axis=0)
    Y = np.zeros((n * os_ // 2 + 1, s.shape[1]), complex); Y[:len(X)] = X
    return 20 * np.log10(np.max(np.abs(np.fft.irfft(Y, n * os_, axis=0))) * os_ + 1e-20)

def master(s):
    """Gain to TARGET_LUFS; if the true peak is then too high, round off only the peaks with a soft knee."""
    for _ in range(6):
        s = s * 10 ** ((TARGET_LUFS - lufs(s)) / 20)
        tp = true_peak(s)
        if tp <= TP_MAX_DB: break
        thr = 10 ** ((TP_MAX_DB - 1.0) / 20); a = np.abs(s)
        over = a > thr
        s = np.where(over, np.sign(s) * thr * (1 + 0.35 * np.log1p((a - thr) / thr)), s)
    return s

# ---------- circular helpers ----------
def wrap_add(buf, x, at_sample):
    """Add x (mono (n,) or stereo (n,2)) into circular buf starting at at_sample, wrapping past the end."""
    n = len(buf); i = int(at_sample) % n; m = len(x)
    while m > 0:
        e = min(n, i + m); k = e - i
        buf[i:e] += x[len(x) - m:len(x) - m + k]
        m -= k; i = 0

def pan(x, p):
    """p in [-1,1], equal power."""
    a = (p + 1) * np.pi / 4
    return np.stack([x * np.cos(a), x * np.sin(a)], 1)

def tukey(n, taper):
    w = np.ones(n); k = min(taper, n // 2)
    r = 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / k)
    w[:k] = r; w[n - k:] = r[::-1]
    return w

def lowpass_st(s, hi, slope=2.0):
    return np.stack([band(s[:, 0], None, hi, slope), band(s[:, 1], None, hi, slope)], 1)

def soft_saw(f, n, r, cutoff=900.0, maxh=24, det=0.0):
    """Saw-like additive stack with a smooth low-pass rolloff (warm, no edge)."""
    t = tarr(n); y = np.zeros(n)
    for k in range(1, maxh + 1):
        fk = f * k * (1 + det)
        if fk > 6000: break
        a = (1.0 / k) / (1 + (fk / cutoff) ** 4)
        if a < 1e-4: continue
        y += a * np.sin(TAU * fk * t + r.random() * TAU)
    return y

def pluck(f, dur, kind="kalimba", vel=1.0, att=0.006):
    """Soft mallet / tine note: few partials, upper ones decay fast, rounded attack."""
    n = N(dur); t = tarr(n)
    if kind == "kalimba":
        parts = [(1.0, 1.0, 1.6), (2.0, 0.10, 0.6), (5.95, 0.07, 0.12), (1.002, 0.35, 1.4)]
    elif kind == "marimba":
        parts = [(1.0, 1.0, 0.9), (3.93, 0.16, 0.18), (9.2, 0.02, 0.05)]
    else:  # music box / soft bell
        parts = [(1.0, 1.0, 1.4), (2.0, 0.25, 0.7), (3.0, 0.08, 0.35), (4.07, 0.05, 0.2), (1.0015, 0.4, 1.3)]
    y = np.zeros(n)
    for r_, a, d in parts:
        fk = f * r_
        if fk > 4500: a *= (4500 / fk) ** 3
        y += a * np.sin(TAU * fk * t) * np.exp(-t / d)
    y *= np.minimum(t / att, 1) ** 2
    y *= np.minimum((dur - t) / 0.05, 1)  # no click at the buffer end
    return vel * y

def drone(n, dur, voices, r, swell_amt=0.4):
    """Snapped-frequency sine drone (integer cycles over the loop), slow integer-cycle swells."""
    t = tarr(n); q = 1.0 / dur; L = np.zeros(n); R = np.zeros(n)
    for vi, (m, a) in enumerate(voices):
        for det, side in ((-0.18, 0), (0.18, 1)):
            for h, ha in enumerate((1, 0.3, 0.1), 1):
                f = round((note(m) * h + det * h) / q) * q
                sw = 1 - swell_amt + swell_amt * (0.5 + 0.5 * np.sin(TAU * t / dur * (1 + (vi + h) % 3) + r.random() * TAU))
                s = np.sin(TAU * f * t + r.random() * TAU) * ha * a * sw
                if side == 0: L += s * 0.8; R += s * 0.35
                else: R += s * 0.8; L += s * 0.35
    return np.stack([L, R], 1)

def chord_pads(n, seg, chords, r, cutoff=850.0, taper_s=4.0, level=1.0):
    """Each chord holds one segment; Tukey windows overlap by their taper so the sum stays constant; circular."""
    buf = np.zeros((n, 2)); tp = N(taper_s); L = N(seg) + tp
    for ci, ch in enumerate(chords):
        w = tukey(L, tp); seg_buf = np.zeros((L, 2))
        for vi, m in enumerate(ch):
            a = 1.0 if vi == 0 else 0.6
            for det, p in ((-0.0016, -0.55), (0.0016, 0.55)):
                v = soft_saw(note(m), L, r, cutoff, det=det)
                seg_buf += pan(v * a, p * (0.5 + 0.5 * (vi % 2)))
        wrap_add(buf, seg_buf * w[:, None] * level, ci * N(seg) - tp // 2)
    return buf

PENTA = [0, 2, 4, 7, 9]  # C D E G A

def penta(lo, hi):
    return [m for m in range(lo, hi + 1) if m % 12 in PENTA]

# ---------- A: golden pads ----------
def opt_a():
    dur = 64.0; n = N(dur); r = rng_for("music_a")
    C3, A2, F2, G2 = 48, 45, 41, 43
    chords = [[C3, 55, 64, 67, 74],      # C add9  (C3 G3 E4 G4 D5)
              [A2 + 12, 52, 60, 64, 67],  # Am7     (A2.. A3 E3 C4 E4 G4)
              [F2 + 12, 48, 57, 64, 67],  # Fmaj7/9 (F3 C3 A3 E4 G4)
              [G2 + 12, 50, 62, 65, 69]]  # G6sus-ish (G3 D3 D4 F4 A4)
    pads = chord_pads(n, 16.0, chords, r, cutoff=800.0, taper_s=5.0)
    bass = np.zeros((n, 2)); roots = [36, 33, 29, 31]
    for i, m in enumerate(roots):
        L = N(16.0) + N(5.0); t = tarr(L)
        v = (np.sin(TAU * note(m) * t) + 0.25 * np.sin(TAU * note(m) * 2 * t)) * tukey(L, N(5.0))
        wrap_add(bass, pan(v, 0.0), i * N(16.0) - N(2.5))
    # a few slow high tones (soft sine, long attack) on chord tones for movement
    mel = np.zeros((n, 2)); line = [(2, 76), (10, 74), (18, 72), (26, 76), (34, 77), (42, 76), (50, 74), (58, 71)]
    for at, m in line:
        L = N(9.0); t = tarr(L)
        e = np.sin(np.pi * t / 9.0) ** 2
        v = (np.sin(TAU * note(m) * t) + 0.12 * np.sin(TAU * note(m) * 2 * t)) * e
        wrap_add(mel, pan(v, r.uniform(-0.4, 0.4)), N(at))
    s = pads * 0.22 + bass * 0.35 + mel * 0.06
    return lowpass_st(s, 2500)

# ---------- B: kalimba drift (sparse pentatonic plucks over a low drone) ----------
def opt_b():
    dur = 72.0; n = N(dur); r = rng_for("music_b"); beat = 60.0 / 60.0
    dr = drone(n, dur, [(36, 1.0), (43, 0.6), (48, 0.35)], r)
    notes = penta(60, 81); buf = np.zeros((n, 2)); idx = 5
    slots = int(dur / (beat / 2))
    for s_ in range(slots):
        p = 0.30 if s_ % 2 == 0 else 0.12
        if r.random() > p: continue
        idx = int(np.clip(idx + r.choice([-2, -1, -1, 1, 1, 2, 0]), 0, len(notes) - 1))
        m = notes[idx]; vel = r.uniform(0.45, 1.0)
        x = pluck(note(m), 3.5, "kalimba", vel)
        jit = r.uniform(-0.015, 0.015)
        wrap_add(buf, pan(x, r.uniform(-0.5, 0.5)), N(s_ * beat / 2 + jit))
        if r.random() < 0.18:  # occasional soft fifth below
            wrap_add(buf, pan(pluck(note(m - 5), 3.5, "kalimba", vel * 0.5), r.uniform(-0.5, 0.5)), N(s_ * beat / 2 + 0.02))
    # light circular echo for space
    echo = np.roll(buf, N(0.75), axis=0)[:, ::-1] * 0.28 + np.roll(buf, N(1.5), axis=0) * 0.12
    s = dr * 0.10 + lowpass_st(buf + echo, 3200) * 0.30
    return s

# ---------- C: music-box arpeggio with a soft bass ----------
def opt_c():
    bpm = 72.0; beat = 60.0 / bpm; bars = 24; dur = bars * 4 * beat; n = N(dur); r = rng_for("music_c")
    prog = [(48, [60, 64, 67, 72]), (45, [57, 60, 64, 69]), (41, [57, 60, 65, 69]), (43, [55, 59, 62, 67]),
            (40, [55, 59, 64, 67]), (45, [57, 60, 64, 69]), (41, [53, 57, 60, 65]), (43, [55, 59, 62, 67])]
    pattern = [0, 1, 2, 3, 2, 1, 2, 1]  # up and back down within the bar (eighths)
    arp = np.zeros((n, 2)); bass = np.zeros((n, 2))
    for b in range(bars):
        root, tones = prog[(b // 3) % len(prog)]
        for e, pi in enumerate(pattern):
            if b % 3 == 2 and e % 2 == 1: continue  # breathe every third bar
            m = tones[pi] + 12
            vel = (0.9 if e == 0 else 0.6 if e % 2 == 0 else 0.42) * r.uniform(0.85, 1.05)
            x = pluck(note(m), 2.6, "box", vel)
            wrap_add(arp, pan(x, -0.35 + 0.7 * pi / 3), N((b * 4 + e * 0.5) * beat + r.uniform(-0.008, 0.008)))
        if b % 3 == 0:
            L = N(3 * 4 * beat + 1.0); t = tarr(L)
            v = (np.sin(TAU * note(root) * t) + 0.3 * np.sin(TAU * note(root) * 2 * t)) * \
                np.minimum(t / 0.08, 1) * np.exp(-t / 5.0) * np.minimum((len(t) / SR - t) / 0.8, 1)
            wrap_add(bass, pan(v, 0.0), N(b * 4 * beat))
        elif b % 3 == 1:
            L = N(2 * 4 * beat); t = tarr(L)
            v = np.sin(TAU * note(root + 7) * t) * np.minimum(t / 0.08, 1) * np.exp(-t / 3.0) * np.minimum((len(t) / SR - t) / 0.8, 1)
            wrap_add(bass, pan(v * 0.5, 0.0), N((b * 4 + 2) * beat))
    echo = np.roll(arp, N(beat * 0.75), axis=0)[:, ::-1] * 0.22
    s = lowpass_st(arp + echo, 3000) * 0.22 + bass * 0.28
    return s

# ---------- D: colony at work (light woody pulse + shaker + pad + marimba bass line) ----------
def opt_d():
    bpm = 66.0; beat = 60.0 / bpm; bars = 20; dur = bars * 4 * beat; n = N(dur); r = rng_for("music_d")
    seg = 4 * 4 * beat  # 4 bars per chord, 5 chords: C F Am G C... -> 20 bars; use C F Am G F
    chords = [[48, 55, 64, 67, 74], [41 + 12, 48, 57, 64, 67], [45 + 12, 52, 60, 64, 67], [43 + 12, 50, 62, 65, 69],
              [41 + 12, 48, 57, 60, 67]]
    roots = [36, 41, 45, 43, 41]
    pads = chord_pads(n, seg, chords, r, cutoff=650.0, taper_s=3.0)
    perc = np.zeros((n, 2)); shk = np.zeros((n, 2)); bassl = np.zeros((n, 2))
    # woody tick: short low-passed noise + 520 Hz damped sine
    def wood(vel, f=520.0):
        L = N(0.25); t = tarr(L); nz = band(r.standard_normal(L), 300, 1800, 2) * np.exp(-t / 0.012)
        y = (np.sin(TAU * f * t) * np.exp(-t / 0.045) + 0.35 * nz) * np.minimum(t / 0.002, 1)
        return y * vel
    def thump(vel):
        L = N(0.5); t = tarr(L); f = 70 + 30 * np.exp(-t / 0.03)
        return np.sin(TAU * np.cumsum(f) / SR) * np.exp(-t / 0.12) * np.minimum(t / 0.004, 1) * vel
    def shaker(vel):
        L = N(0.18); t = tarr(L); e = np.minimum(t / 0.03, 1) * np.exp(-np.maximum(t - 0.03, 0) / 0.035)
        return band(r.standard_normal(L), 1800, 4200, 2) * e * vel
    for b in range(bars):
        for q in range(4):
            at = (b * 4 + q) * beat
            if q in (0, 2): wrap_add(perc, pan(thump(0.9 if q == 0 else 0.6), 0.0), N(at))
            if q in (1, 3): wrap_add(perc, pan(wood(r.uniform(0.35, 0.5), 520 if q == 1 else 440), -0.25), N(at + r.uniform(-0.006, 0.006)))
            if r.random() < 0.35: wrap_add(perc, pan(wood(r.uniform(0.15, 0.25), 660), 0.3), N(at + beat * 0.75))
            for e in range(2):
                wrap_add(shk, pan(shaker(r.uniform(0.5, 0.8) * (1.0 if e == 1 else 0.55)), 0.35), N(at + e * beat / 2))
    # marimba bass line: root on 1, fifth/octave on 3 (and a passing pentatonic note sometimes)
    for b in range(bars):
        root = roots[(b // 4) % len(roots)]
        wrap_add(bassl, pan(pluck(note(root), 2.0, "marimba", 0.9), -0.1), N(b * 4 * beat))
        nxt = root + (7 if b % 2 == 0 else 12)
        wrap_add(bassl, pan(pluck(note(nxt), 1.6, "marimba", 0.55), 0.1), N((b * 4 + 2) * beat))
        if b % 4 == 3:
            wrap_add(bassl, pan(pluck(note(root + 9 if (root + 9) % 12 in PENTA else root + 7), 1.2, "marimba", 0.4), 0.1), N((b * 4 + 3.5) * beat))
    s = pads * 0.12 + lowpass_st(perc, 2500) * 0.20 + shk * 0.035 + lowpass_st(bassl, 2000) * 0.30
    return s

OPTIONS = [("A", "golden-pads", opt_a), ("B", "kalimba-drift", opt_b), ("C", "music-box-arpeggio", opt_c),
           ("D", "colony-pulse", opt_d)]

def write_st(path, s):
    assert np.max(np.abs(s)) <= 1.0, path
    pcm = np.round(s * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())

def read_st(path):
    with wave.open(path) as w:
        ch = w.getnchannels(); x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float) / 32768
    x = x.reshape(-1, ch)
    return np.repeat(x, 2, 1) if ch == 1 else x

def tile(x, n):
    return np.concatenate([x] * (n // len(x) + 1))[:n]

GAME_FILES = {"A": "music_a", "B": "music_b", "C": "music_c"}   # in game: one per day in turn (FWindTuning::MusicSounds)

def main():
    args = sys.argv[2:]
    if "--game" in args:  # make_music.py Art/Audio --game: only the in-game files, music_a/b/c.wav
        os.makedirs(OUT, exist_ok=True)
        for letter, name, fn in OPTIONS:
            if letter in GAME_FILES:
                write_st(os.path.join(OUT, GAME_FILES[letter] + ".wav"), master(fn()))
        print("done ->", os.path.abspath(OUT)); return
    only = args or [o[0] for o in OPTIONS]
    os.makedirs(OUT, exist_ok=True)
    wind = read_st(os.path.join(WIND_DIR, "wind_calm_loop.wav")); leaves = read_st(os.path.join(WIND_DIR, "leaves_rustle_loop.wav"))
    for letter, name, fn in OPTIONS:
        if letter not in only: continue
        s = master(fn())
        write_st(os.path.join(OUT, f"{letter}-{name}.wav"), s)
        q = np.round(s * 32767) / 32767; d = np.abs(np.diff(np.concatenate([q, q[:2]]), axis=0))
        jump = np.max(np.abs(q[0] - q[-1])); slope = np.max(np.abs((q[1] - q[0]) - (q[-1] - q[-2])))
        dd = np.abs(np.diff(np.diff(np.concatenate([q, q[:2]]), axis=0), axis=0))
        n20 = N(20.0)
        mix = tile(wind, n20) * CALM_WITH_MUSIC + tile(leaves, n20) * RUSTLE_WITH_MUSIC + s[:n20] * MUSIC_VOL
        pk = np.max(np.abs(mix)); g = min(1.0, 0.89 / pk)
        write_st(os.path.join(OUT, f"{letter}-preview-with-wind.wav"), mix * g)
        seam = np.concatenate([s[-N(3.0):], s[:N(3.0)]])
        if os.environ.get("MUSIC_SEAM_DIR"):  # optional: 6 s across the loop join, for listening checks
            write_st(os.path.join(os.environ["MUSIC_SEAM_DIR"], f"{letter}-seam-excerpt.wav"), seam)
        print(f"{letter}-{name}: {len(s)/SR:.2f} s, LUFS~{lufs(s):.2f}, TP~{true_peak(s):.2f} dBTP, "
              f"seam jump {jump:.5f} vs median step {np.median(d):.5f} / p99 {np.percentile(d, 99):.5f} "
              f"(ratio to p99 {jump/np.percentile(d,99):.2f}); slope change {slope:.5f} vs p99 {np.percentile(dd,99):.5f}; "
              f"preview peak {20*np.log10(pk):.2f} dBFS, preview gain {g:.3f}")
    print("done ->", os.path.abspath(OUT))

if __name__ == "__main__":
    main()
