"""Leafline audio generator. Pure synthesis (numpy only), fixed seed.
Run: uv run --with numpy python Tools/audio/make_audio.py [outdir]
Key of all tonal material: C major (chime, upgrade, milestone, day_end, pad)."""
import sys, wave, os
import numpy as np

SR = 48000
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "Art", "Audio")
TAU = 2 * np.pi

def rng_for(name):
    return np.random.default_rng(abs(hash_str(name)) % (2**32))

def hash_str(s):
    h = 2166136261
    for c in s:
        h = ((h ^ ord(c)) * 16777619) & 0xFFFFFFFF
    return h

def N(sec): return int(round(sec * SR))
def tarr(n): return np.arange(n) / SR
def db(x): return 10 ** (x / 20)
def note(m): return 440.0 * 2 ** ((m - 69) / 12)   # midi -> Hz
C4, D4, E4, G4, A4, B4 = 60, 62, 64, 67, 69, 71

# ---------- circular (loop-safe) spectral tools ----------
def band(x, lo, hi, slope=4.0):
    """Smooth FFT band-pass (lo or hi may be None). Circular => loops stay seamless."""
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR); m = np.ones_like(f)
    if lo: m *= 1 / (1 + (lo / np.maximum(f, 1e-3)) ** (2 * slope))
    if hi: m *= 1 / (1 + (f / hi) ** (2 * slope))
    return np.fft.irfft(X * m, len(x))

def noise(n, r): return r.standard_normal(n)

def pink(n, r):
    X = np.fft.rfft(noise(n, r)); f = np.fft.rfftfreq(n, 1 / SR); f[0] = f[1]
    return np.fft.irfft(X / np.sqrt(f), n)

def slow(n, r, cutoff, lo=0.0, hi=1.0):
    """Slow random control signal in [lo,hi], circular."""
    x = band(noise(n, r), None, cutoff, 2)
    x = (x - x.min()) / (x.max() - x.min() + 1e-12)
    return lo + (hi - lo) * x

def norm(x, peak_db):
    return x * (db(peak_db) / (np.max(np.abs(x)) + 1e-12))

def fade(x, a=0.002, b=0.01):
    x = x.copy(); na, nb = N(a), N(b)
    x[:na] *= np.linspace(0, 1, na) if x.ndim == 1 else np.linspace(0, 1, na)[:, None]
    x[-nb:] *= np.linspace(1, 0, nb) if x.ndim == 1 else np.linspace(1, 0, nb)[:, None]
    return x

def env_ad(n, att, dec, curve=3.0):
    t = tarr(n); a = np.minimum(t / max(att, 1e-4), 1)
    return a * np.exp(-curve * np.maximum(t - att, 0) / dec)

def write(name, x, mono=True):
    x = np.asarray(x)
    assert np.max(np.abs(x)) <= 1.0, name
    pcm = (x * 32767).astype("<i2")
    ch = 1 if pcm.ndim == 1 else pcm.shape[1]
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(ch); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())

def tone(freq, n, harm=(1.0,), ph=0.0):
    t = tarr(n); y = np.zeros(n)
    for k, a in enumerate(harm, 1):
        y += a * np.sin(TAU * freq * k * t + ph * k)
    return y

def bell(freq, dur, amp=1.0, bright=1.0):
    """Soft chime: harmonic-ish partials with slight inharmonic shimmer, each decaying at its own rate."""
    n = N(dur); t = tarr(n); y = np.zeros(n)
    parts = [(1.0, 1.0, 1.0), (2.0, 0.45 * bright, 0.7), (3.0, 0.22 * bright, 0.5),
             (4.17, 0.12 * bright, 0.32), (5.43, 0.06 * bright, 0.2), (1.003, 0.5, 1.0)]
    for r, a, d in parts:
        y += a * np.sin(TAU * freq * r * t) * np.exp(-t * 5.0 / (dur * d))
    y *= np.minimum(t / 0.003, 1)
    return amp * y

def place(buf, x, at):
    i = N(at); e = min(len(buf), i + len(x)); buf[i:e] += x[:e - i]

def stereo_from(l, r): return np.stack([l, r], 1)

def decor(x, r, width=0.35):
    """Cheap stereo: mix with a short-delayed, differently-filtered copy."""
    d = N(0.011 + 0.006 * r.random())
    return stereo_from(x + width * np.roll(x, d) * 0.0 + 0.0, x) if False else stereo_from(x, np.roll(x, d))

# ---------- winds ----------
GUST_HOWL = 0.6     # level of the gust's broad howl band (with the gust's swell envelope, up to 0.16)
GUST_HOWL_BAND = (350, 1500)
GUST_BODY = 2.0     # was 1.2 with the whistle; more body keeps the gust about as loud without it
GUST_PEAK_DB = -2.5  # the gust loop's peak after normalising (was -3.5: the tonal whistle carried loudness)

def wind_bed(sec, name, gust):
    r = rng_for(name); n = N(sec); chans = []
    for c in range(2):
        base = pink(n, r)
        if not gust:
            body = band(base, 250, 2800, 2)
            m = slow(n, r, 0.35, 0.25, 1.0) ** 1.5
            sh = band(noise(n, r), 3000, 9000, 2) * 0.08 * slow(n, r, 1.5, 0, 1) ** 2
            y = body * m * 1.0 + sh
            y *= 0.6 + 0.4 * slow(n, r, 0.12, 0, 1)
        else:
            rumble = band(base, 30, 220, 3) * 2.2
            body = band(base, 180, 1800, 2) * GUST_BODY
            m = slow(n, r, 0.45, 0.35, 1.0) ** 1.3
            # Howl, no whistle (owner, 15 Nov: "a bit less whistly"): broad noise 350 Hz - 1.5 kHz swelling with the gust
            # instead of the old sine sweeping 900-1600 Hz plus a 1-2.5 kHz band. The random draws keep their order (the
            # unused sweep draw stays), so the rumble and body move exactly as before.
            slow(n, r, 0.25, 0, 1)
            amp = slow(n, r, 0.5, 0, 1) ** 3 * 0.16
            whist = band(noise(n, r), *GUST_HOWL_BAND, 2) * GUST_HOWL * amp
            y = (rumble + body * m) * (0.55 + 0.45 * slow(n, r, 0.2, 0, 1)) + whist
        chans.append(y)
    s = stereo_from(*chans)
    return s

def wind_calm(): return norm(wind_bed(10, "calm", False), -9.0)
def wind_gust(): return norm(wind_bed(10, "gust", True), GUST_PEAK_DB)

def smoothenv(n, r, cutoff, shape=1.0):
    """Rectified, low-passed noise envelope in [0,1] (circular)."""
    x = np.abs(band(noise(n, r), None, cutoff, 2)); return (x / x.max()) ** shape

def leaves_rustle():
    r = rng_for("rv0"); n = N(10); chans = []
    for c in range(2):
        e = smoothenv(n, r, 25, 1.5) * slow(n, r, 0.5, 0.2, 1)
        y = band(noise(n, r), 2000, 9000, 2) * e
        y += band(noise(n, r), 4000, 12000, 2) * 0.3 * smoothenv(n, r, 80, 3)
        chans.append(y)
    return norm(stereo_from(*chans), -12.0)

def gust_warning():
    r = rng_for("warn"); n = N(2.6); t = tarr(n); y = np.zeros(n)
    base = noise(n, r)
    centres = [150, 300, 600, 1200, 2400]
    prog = t / t[-1]; cen = 120 * (2.0 ** (prog * 4.4))          # rises 120 Hz -> ~2.5 kHz
    for c in centres:
        b = band(base, c * 0.6, c * 1.5, 2)
        w = np.exp(-0.5 * (np.log2(cen / c) / 0.9) ** 2); y += b * w
    sw = (prog ** 2.2) * (1 - np.exp(-(1 - prog) * 20))
    y *= sw * (0.8 + 0.2 * slow(n, r, 6, 0, 1))
    y += band(noise(n, r), 25, 90, 3) * 0.6 * sw
    y = fade(y, 0.02, 0.12)
    return norm(y, -8.0)

# ---------- feet / cutting ----------
def step(i):
    r = rng_for(f"step{i}"); n = N(0.03)
    t = tarr(n); f0 = 2200 + 1800 * r.random()
    y = band(noise(n, r), 1500, 7000, 2) * np.exp(-t / 0.0035)
    y += 0.5 * np.sin(TAU * f0 * t) * np.exp(-t / 0.002)
    y += 0.25 * band(noise(n, r), 300, 1200, 2) * np.exp(-t / 0.006)
    y *= np.minimum(t / 0.0005, 1)
    y = fade(y, 0.0004, 0.006)
    return norm(y, -26.0 + 2.5 * (r.random() - 0.5) * 2)

def cut_loop():
    r = rng_for("cv0"); n = N(1.5); t = tarr(n)
    ph = (6.0 * t) % 1.0                           # 9 slow saw strokes in 1.5 s -> seamless
    st = np.sin(np.pi * ph)
    y = band(noise(n, r), 600, 5000, 2) * st * (0.5 + 0.5 * smoothenv(n, r, 300, 1))
    return norm(y, -10.0)

def cut_done():
    r = rng_for("dv0"); n = N(0.6); t = tarr(n)
    y = band(noise(n, r), 1500, 9000, 2) * np.exp(-t / 0.08) * (0.3 + 0.7 * smoothenv(n, r, 400, 1)) * np.minimum(t / 0.006, 1)
    cr = np.zeros(n)
    for p in (r.random(40) ** 2 * N(0.3)).astype(int):
        ln = N(0.003); k = band(noise(ln, r), 2000, None, 2) * np.exp(-np.arange(ln) / (SR * 0.0007))
        cr[p:p + ln] += k[: len(cr[p:p + ln])] * (0.3 + r.random())
    return norm(fade(y + 0.6 * cr, 0.002, 0.05), -9.0)

# ---------- tonal one-shots ----------
def pickup():
    n = N(0.5); y = np.zeros(n)
    place(y, bell(note(G4 + 12), 0.4, 0.6, 0.4), 0.0)
    place(y, bell(note(C4 + 24), 0.4, 0.8, 0.4), 0.07)
    y += 0.05 * band(noise(n, rng_for("pick")), 1500, 5000, 2) * env_ad(n, 0.002, 0.04)
    return norm(fade(y, 0.001, 0.08), -13.0)

def warm_note(m, dur, amp=1.0):
    n = N(dur); t = tarr(n); f = note(m)
    y = (np.sin(TAU * f * t) + 0.35 * np.sin(TAU * 2 * f * t) + 0.12 * np.sin(TAU * 3 * f * t)
         + 0.2 * np.sin(TAU * f * 1.004 * t))
    return amp * y * (np.minimum(t / 0.02, 1)) * np.exp(-t * 4.0 / dur)

def send_home():
    n = N(0.9); y = np.zeros(n)
    place(y, warm_note(G4 + 12 - 12 + 12 - 12, 0.6, 0.7), 0.0)   # G4
    place(y, warm_note(E4 + 12, 0.7, 0.85), 0.12)                 # E5
    place(y, warm_note(C4 + 24, 0.7, 0.6), 0.26)                  # C6 (soft top)
    return norm(fade(y, 0.002, 0.08), -11.0)

def shift_whoosh():
    r = rng_for("whoosh"); n = N(1.0); t = tarr(n); p = t / t[-1]
    base = noise(n, r); y = np.zeros(n)
    cen = 350 * 2.0 ** (2.6 * np.sin(np.pi * p * 0.5 + 0.0) )         # up and plateau
    for c in [200, 450, 900, 1800, 3600]:
        y += band(base, c * 0.6, c * 1.5, 2) * np.exp(-0.5 * (np.log2(cen / c) / 0.8) ** 2)
    sw = np.sin(np.pi * p) ** 1.8
    y *= sw
    return norm(fade(y, 0.01, 0.1), -10.0)

def deposit():
    n = N(1.4); y = np.zeros(n)
    place(y, bell(note(C4 + 24), 1.4, 1.0), 0.0)                  # C6
    place(y, bell(note(G4 + 24), 1.0, 0.35, 0.6), 0.0)            # G6 shimmer
    return norm(fade(y, 0.0005, 0.2), -6.0)

def blown_off():
    r = rng_for("blown"); n = N(1.6); t = tarr(n); base = noise(n, r); y = np.zeros(n)
    # whip: bright burst rising fast, then falling with a doppler-like drop
    cen = np.where(t < 0.25, 400 * 2 ** (3.3 * t / 0.25), 3500 * 2 ** (-3.6 * (t - 0.25) / 1.35))
    for c in [150, 300, 600, 1200, 2400, 4800]:
        y += band(base, c * 0.6, c * 1.5, 2) * np.exp(-0.5 * (np.log2(cen / c) / 0.8) ** 2)
    env = np.where(t < 0.25, (t / 0.25) ** 1.5, np.exp(-(t - 0.25) / 0.5))
    y *= env
    y += band(noise(n, r), 40, 200, 3) * 0.5 * np.exp(-t / 0.5) * np.minimum(t / 0.1, 1)
    return norm(fade(y, 0.004, 0.2), -6.0)

def grip():
    r = rng_for("gv2"); n = N(1.2); t = tarr(n)
    f = 800 * (1 + 0.15 * slow(n, r, 3, -1, 1))
    ph = np.cumsum(TAU * f / SR); ph -= np.linspace(0, ph[-1] - TAU * round(ph[-1] / TAU), n)
    sq = sum(np.sin(k * ph) / k ** 1.3 for k in range(1, 7))
    am = (0.5 + 0.5 * np.sin(TAU * 18 * t)) ** 3 * slow(n, r, 4, 0.3, 1)
    y = band(sq, 400, 4000, 2) * am + 0.15 * band(noise(n, r), 1000, 4000, 2) * am
    return norm(y, -17.0)

def stamina_low():
    n = N(0.7); y = np.zeros(n)
    place(y, warm_note(E4 - 12 + 12, 0.35, 0.8), 0.0)         # E4
    place(y, warm_note(C4, 0.5, 0.8), 0.22)                   # C4
    y += 0.03 * band(noise(n, rng_for("stam")), 400, 2500, 2) * env_ad(n, 0.1, 0.4)
    return norm(fade(y, 0.004, 0.1), -17.0)

def ui_move():
    n = N(0.04); t = tarr(n)
    return norm(fade(np.sin(TAU * 1568 * t) * np.exp(-t / 0.008), 0.0005, 0.01), -18.0)

def ui_confirm():
    n = N(0.22); y = np.zeros(n)
    place(y, bell(note(C4 + 24), 0.15, 0.7, 0.3), 0); place(y, bell(note(G4 + 24), 0.18, 0.8, 0.3), 0.07)
    return norm(fade(y, 0.0005, 0.05), -13.0)

def ui_deny():
    n = N(0.2); y = np.zeros(n); t = tarr(n)
    for at, m in ((0, E4), (0.08, C4)):
        place(y, warm_note(m, 0.12, 0.8) , at)
    return norm(fade(y, 0.001, 0.05), -15.0)

def upgrade():
    n = N(2.0); y = np.zeros(n)
    for i, (at, m) in enumerate([(0, C4 + 12), (0.1, E4 + 12), (0.2, G4 + 12), (0.3, C4 + 24), (0.4, E4 + 24)]):
        place(y, bell(note(m), 0.9, 0.6), at)
    for m in (C4 + 24, E4 + 24, G4 + 24):
        place(y, bell(note(m), 1.5, 0.45, 0.5), 0.55)
    return norm(fade(y, 0.0005, 0.3), -8.0)

def milestone():
    n = N(3.4); y = np.zeros(n)
    for at, m in [(0, C4 + 12), (0.14, G4 + 12), (0.28, C4 + 24), (0.42, E4 + 24), (0.56, G4 + 24)]:
        place(y, bell(note(m), 1.2, 0.55), at)
    for m, v in ((C4 + 24, 0.55), (E4 + 24, 0.45), (G4 + 24, 0.45), (C4 + 36, 0.3), (C4 + 12, 0.5)):
        place(y, bell(note(m), 2.8, v, 0.6), 0.75)
    # light shimmer tail (a diffuse echo of the chord)
    for k, d in enumerate((0.19, 0.31, 0.47)):
        y += 0.0; y[N(d):] += 0.18 * y[: len(y) - N(d)] * (0.6 ** k)
    return norm(fade(y, 0.0005, 0.5), -7.0)

def day_end():
    n = N(4.0); y = np.zeros(n)
    for at, m in [(0.0, G4 + 12), (0.7, E4 + 12), (1.4, D4 + 12), (2.1, C4 + 12)]:
        place(y, bell(note(m), 1.6, 0.5, 0.35), at)
    t = tarr(n)
    for m in (C4 - 12, G4 - 12, E4, G4):
        y += 0.18 * np.sin(TAU * note(m) * t) * np.minimum(t / 0.8, 1) * np.minimum((4.0 - t) / 1.2, 1)
    return norm(fade(y, 0.002, 0.4), -10.0)

# ---------- pad (48 s loop: every partial is an integer number of cycles) ----------
def pad():
    dur = 48; n = N(dur); t = tarr(n); r = rng_for("pad"); q = 1.0 / dur
    def snap(f): return round(f / q) * q
    voices = [(C4 - 24, 1.0), (G4 - 24, 0.7), (E4 - 12, 0.5), (G4 - 12, 0.45), (D4, 0.3), (C4, 0.25)]   # Cadd9 colour
    L = np.zeros(n); R = np.zeros(n)
    for vi, (m, a) in enumerate(voices):
        f0 = snap(note(m))
        sw = 0.5 + 0.5 * np.sin(TAU * (t / dur * (1 + vi % 3)) + r.random() * TAU)       # slow swell, integer cycles
        for det, side in ((-0.25, 0), (0.25, 1)):
            for h, ha in enumerate((1, 0.35, 0.15, 0.07), 1):
                f = snap(f0 * h) + det * h * 0.6 // q * q if False else snap(f0 * h) + det * h
                f = round(f / q) * q
                s = np.sin(TAU * f * t + r.random() * TAU) * ha * a * (0.4 + 0.6 * sw)
                if side == 0: L += s * 0.8; R += s * 0.35
                else: R += s * 0.8; L += s * 0.35
    s = stereo_from(L, R)
    return norm(s, -14.0)

# ---------- bird (call and wing rush) ----------
def bird_call():
    """Four quick falling whistles of a small songbird (3-5 kHz, above the gust bed's < 2.5 kHz).
    Each note: exponential downward sweep, 45 Hz FM vibrato (+-700 Hz), pitch jitter between notes,
    a weak 2nd harmonic, a second 'syrinx' voice 18 Hz away, breath noise, soft attack. Variant E of the
    14 Nov second pass (lowest '8-bit' and highest 'bird chirping' readings of five)."""
    r = rng_for("bird_call_E"); n = N(0.8); y = np.zeros(n)
    notes = ((.03, 4800, 3500, .09, .7), (.17, 4600, 3300, .09, .9), (.31, 4300, 3000, .11, 1.0), (.48, 4000, 2800, .14, .8))
    for i, (at, f0, f1, dur, a) in enumerate(notes):
        j = 1 + 0.06 * (r.random() - 0.5) * 2
        m = N(dur); t = tarr(m); p = t / dur
        f = (f1 + (f0 - f1) * np.exp(-2.0 * p)) * j + 700 * np.sin(TAU * 45 * (1 + .15 * i) * t + r.random() * 6) * np.sin(np.pi * p)
        ph = np.cumsum(TAU * f / SR)
        s = np.sin(ph) + 0.05 * np.sin(2 * ph + .5)
        s += 0.45 * np.sin(np.cumsum(TAU * (f + 18) / SR))
        s += 0.2 * band(noise(m, r), 2500, 7000, 2)
        e = np.minimum(t / 0.006, 1) * np.minimum((dur - t) / 0.03, 1) * (0.7 + 0.3 * np.sin(np.pi * p))
        place(y, s * e * a, at)
    return norm(fade(y, 0.006, 0.05), -11.0)

def bird_pass():
    """Soft wing rush ~0.55 s: feathery band noise with a wing-beat flutter, passing by (pitch eases down)."""
    r = rng_for("bird_pass"); n = N(0.6); t = tarr(n); p = t / t[-1]; base = noise(n, r); y = np.zeros(n)
    cen = 2600 * 2.0 ** (-0.9 * p)                                  # eases down as it passes
    for c in [500, 900, 1600, 2800, 5000]:
        y += band(base, c * 0.6, c * 1.5, 2) * np.exp(-0.5 * (np.log2(cen / c) / 1.1) ** 2)
    flut = 0.72 + 0.28 * np.sin(TAU * (11 + 3 * p) * t + 0.5)      # wing beats
    y *= flut * np.sin(np.pi * p) ** 2.2
    y += 0.25 * band(noise(n, r), 150, 450, 2) * np.sin(np.pi * p) ** 2   # soft air body
    return norm(fade(y, 0.02, 0.12), -10.0)

# ---------- main ----------
def main():
    os.makedirs(OUT, exist_ok=True)
    write("wind_calm_loop", wind_calm()); write("wind_gust_loop", wind_gust())
    write("gust_warning", gust_warning()); write("leaves_rustle_loop", leaves_rustle())
    for i in range(1, 7): write(f"step_{i:02d}", step(i))
    for fn in (cut_loop, cut_done, pickup, send_home, shift_whoosh, deposit, blown_off, grip, stamina_low,
               ui_move, ui_confirm, ui_deny, upgrade, milestone, day_end):
        write(fn.__name__, fn())
    write("ambient_pad_loop", pad())
    write("bird_call", bird_call()); write("bird_pass", bird_pass())
    print("done ->", os.path.abspath(OUT))

if __name__ == "__main__":
    main()
