# Brief: synthesised audio

A self-contained job for a parallel agent. It does not touch the Unreal editor.

## Goal

Every sound in the game, synthesised by a script, in keeping with the project's math-only rule. Both jams score audio, so "present and pleasant" is the bar, and "not obviously a sine beep" matters.

## Rules

- Write the generator during this jam. Do not reuse audio or code from any other project.
- No samples and no AI audio models. If synthesis cannot reach an acceptable result for a sound, say so in the report; the owner may add CC0 samples from freesound.org later.
- Listen with `earshot` (CLI on the PATH, also an MCP server in `.mcp.json`). Run `earshot --help` first. Use `describe`, `match`, `ab` and `measure`, and quote readings in your report. Do not claim how something sounds without a reading. Its `forge` command can search synthesis recipes against a description; use it if it helps, but the committed generator must be able to reproduce every file.
- You deliver files. The main session imports them into Unreal.

## Deliverables

| File | Content |
|---|---|
| `Tools/audio/make_audio.py` | Generator. Python 3 with numpy (run with `uv run --with numpy`). One function per sound, a fixed random seed, writes every WAV below. |
| `Art/Audio/*.wav` | 48 kHz, 16-bit, mono unless stated |
| `Art/Audio/README.md` | For each file: purpose, length, whether it loops, peak and loudness as measured, and the earshot reading |

## Sounds

| File | What it is | Notes |
|---|---|---|
| `wind_calm_loop.wav` | Light air through leaves | Stereo, 8–12 s, seamless loop |
| `wind_gust_loop.wav` | Strong wind, some low rumble and whistling | Stereo, 8–12 s, seamless loop. The game crossfades between calm and gust by wind strength |
| `gust_warning.wav` | A rising swell that says "wind is coming" | 2–3 s, one-shot |
| `leaves_rustle_loop.wav` | Canopy rustle | Stereo loop, layered under wind |
| `step_01.wav` … `step_06.wav` | Tiny dry ticks of an ant's feet on bark | 20–40 ms each, quiet, varied |
| `cut_loop.wav` | Mandibles sawing through a leaf: fast crisp rasping | 1–2 s loop |
| `cut_done.wav` | The fragment tearing free | Short snap with a papery tail |
| `pickup.wav` | Soft confirmation as the fragment is lifted | |
| `send_home.wav` | A light, warm cue for sending a worker home | |
| `shift_whoosh.wav` | The camera gliding to the next ant | About 1 s |
| `deposit.wav` | A pleasant chime as a fragment reaches the nest | Pitch-friendly, so the game can raise it with fragment size |
| `blown_off.wav` | A sudden gusty whip and fall | Not comedic |
| `grip.wav` | A small tense creak while holding on | Loopable |
| `stamina_low.wav` | Subtle cue when sprint runs out | |
| `ui_move.wav`, `ui_confirm.wav`, `ui_deny.wav` | Menu ticks | Very short |
| `upgrade.wav` | A satisfying rising figure | |
| `milestone.wav` | A fuller version of the chime for colony milestones | |
| `day_end.wav` | A calm closing phrase | 3–4 s |
| `ambient_pad_loop.wav` | A soft, slow, warm pad to sit under everything | Stereo, 40–60 s seamless loop, unobtrusive |

## Quality bar

- Loops are seamless: no click at the join. Check with `measure` and by concatenating a loop with itself.
- No clipping. Leave headroom; peaks at or under -3 dBFS.
- One-shots start at a zero crossing and end in silence.
- Relative levels are sensible: steps far below the chime, wind gust the loudest bed.
- Wind should be filtered noise with slow random modulation, not white noise at a fixed level.
- The chime, the upgrade figure, the milestone and the pad share one key, so they sit together.

## Report

List each file with its earshot reading, name the two or three you are least happy with, and say what you would try next for them.
