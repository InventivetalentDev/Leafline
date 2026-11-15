# Leafline audio

All files are synthesised by `Tools/audio/make_audio.py` (numpy only, fixed per-sound seeds):
`uv run --with numpy python Tools/audio/make_audio.py`. 48 kHz, 16-bit; stereo for wind, rustle and pad, mono otherwise.
Tonal material shares one key (C major). Loops are built with circular spectral filtering and integer-cycle partials, so the join is seamless by construction (seam sample step checked, no outlier). One-shots start and end at 0.
Readings are earshot `measure` (peak dBFS, integrated LUFS) and `match` (z >= 3 strong, 2-3 weak, < 2 no match). "not matched" means no semantic reading was taken.

| File | Purpose | Length | Type | Peak dBFS | LUFS | earshot reading |
|---|---|---|---|---|---|---|
| `wind_calm_loop.wav` | Light air through leaves, stereo | 10000 ms | loop | -9.0 | -24.7 | wind bed; match 'gentle breeze through leaves': strong, z +3.2 |
| `wind_gust_loop.wav` | Strong wind, rumble + body + broad 350 Hz-1.5 kHz howl, no whistle (owner, 15 Nov), stereo | 10000 ms | loop | -2.5 | -15.5 | loudest bed; describe: 'a gust of wind rushing past' z +3.4 (strong), no siren or kettle tags (the old sine-whistle version read 'a police siren' z +3.1, 'a kettle whistling' z +2.5); ab vs old: 'a whistling wind' z +2.4 (old +3.0) |
| `gust_warning.wav` | Rising swell before a gust | 2600 ms | one-shot | -8.0 | -18.8 | not matched (no reading taken) |
| `leaves_rustle_loop.wav` | Canopy rustle, stereo | 10000 ms | loop | -12.0 | -34.2 | WEAK: match 'rustling leaves' z +2.5 (was +1.8; plausible); nearest 'a small animal running through grass' z +2.5 |
| `cut_loop.wav` | Mandible sawing on leaf | 1500 ms | loop | -10.0 | -21.6 | match 'rasping sawing through a leaf' z +2.6 (was +1.0; plausible, rank #5/322); nearest 'a snare roll' z +3.3 |
| `cut_done.wav` | Fragment tears free | 600 ms | one-shot | -9.0 | -30.3 | WEAK: match 'paper tearing' z +1.6 (was -0.3; still no match); nearest 'crisp hi-hats' z +4.2 |
| `pickup.wav` | Soft confirmation on lift | 500 ms | one-shot | -13.0 | -25.7 | not matched |
| `send_home.wav` | Warm 3-note cue G4-E5-C6 | 900 ms | one-shot | -11.0 | -20.8 | not matched |
| `shift_whoosh.wav` | Camera glide to next ant | 1000 ms | one-shot | -10.0 | -19.5 | match 'whoosh': strong, z +3.3 |
| `deposit.wav` | Chime, C6 (pitch-friendly) | 1400 ms | one-shot | -6.0 | -20.9 | match 'pleasant bell chime': weak z +2.3; nearest 'a success chime' z +4.0 |
| `blown_off.wav` | Gust whip and fall | 1600 ms | one-shot | -6.0 | -16.4 | match 'sudden whooshing wind gust': strong, z +4.4 |
| `grip.wav` | Tense creak while holding | 1200 ms | loop | -17.0 | -27.7 | WEAK: match 'creaking wood' z +1.4 (was +0.6; still no match); nearest 'a balloon squeak' z +3.4 |
| `stamina_low.wav` | Soft falling E4-C4 cue | 700 ms | one-shot | -17.0 | -26.4 | not matched |
| `ui_move.wav` | Menu tick | 40 ms | one-shot | -18.0 | n/a (short) | not matched |
| `ui_confirm.wav` | Menu confirm C6-G6 | 220 ms | one-shot | -13.0 | n/a (short) | not matched |
| `ui_deny.wav` | Menu deny E4-C4 | 200 ms | one-shot | -15.0 | n/a (short) | not matched |
| `upgrade.wav` | Rising C-major arpeggio + chord | 2000 ms | one-shot | -8.0 | -17.7 | not matched |
| `milestone.wav` | Fuller chime, C-major chord | 3400 ms | one-shot | -7.0 | -20.6 | not matched |
| `day_end.wav` | Calm descending phrase G-E-D-C over chord | 4000 ms | one-shot | -10.0 | -21.3 | not matched |
| `ambient_pad_loop.wav` | Cadd9 pad, stereo, 48 s | 48000 ms | loop | -14.0 | -25.8 | match 'soft warm ambient pad': strong, z +3.3 (rank #2/322) |
| `step_01.wav` | Ant foot tick on bark | 30 ms | one-shot | -27.1 | n/a (short) | step_01 match 'tiny dry tick, footstep on bark': weak z +2.4; nearest 'a soft key release click' z +2.9 |
| `step_02.wav` | Ant foot tick on bark | 30 ms | one-shot | -26.3 | n/a (short) | same recipe, varied seed |
| `step_03.wav` | Ant foot tick on bark | 30 ms | one-shot | -27.0 | n/a (short) | same recipe, varied seed |
| `step_04.wav` | Ant foot tick on bark | 30 ms | one-shot | -25.1 | n/a (short) | same recipe, varied seed |
| `step_05.wav` | Ant foot tick on bark | 30 ms | one-shot | -27.8 | n/a (short) | same recipe, varied seed |
| `step_06.wav` | Ant foot tick on bark | 30 ms | one-shot | -26.3 | n/a (short) | same recipe, varied seed |
| `bird_call.wav` | Small songbird: four quick falling FM whistles, 3-5 kHz, for the bird's dive warning | 800 ms | one-shot | -11.0 | -16.1 | describe: 'a bird chirping' z +2.6 (plausible), 'birds singing in the morning' z +2.5; still 'an 8-bit sound effect' z +2.2 (first version: bird +2.1, 8-bit +3.6); realness: code-synthesized. In 2.5-5 kHz it is +19.6 dB over the calm wind's RMS (+35.5 over the gust's) |
| `bird_pass.wav` | Feathery wing rush with a ~12 Hz flutter, easing downward | 600 ms | one-shot | -10.0 | -20 (peak raised from -12 after measuring -22.1) | describe: 'a fast whoosh' z +3.2, 'cloth whipping through the air' z +3.1, 'a bird flapping its wings' z +2.0; compare: cosine 0.66 to shift_whoosh, 0.65 to blown_off (an outlier among them) |
