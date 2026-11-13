# Sourced assets

Everything in the game is made for it (code, procedural geometry and shaders, the Blender-script ant, synthesised
sounds) except what is listed here. Only CC0 material is allowed (`CLAUDE.md`).

| Asset | Where in the repo | Source | Licence | Notes |
|---|---|---|---|---|
| Button prompt icons: 15 light keyboard and mouse keys, 11 Xbox Series buttons | `Content/Leafline/UIRaw/Key_*.pngraw`, `Pad_*.pngraw` | "Xelu's FREE Controller Prompts" by Nicolae (Xelu) Berbece, updated by Paul Paun (2021), https://thoseawesomeguys.com/prompts/ (downloaded 13 Nov 2026) | CC0 1.0 (public domain), stated on the page and in the pack's `Readme.txt` | Chosen by the owner (13 Nov). Resized from 100 to 64 px and saved without metadata; renamed `.pngraw` so the editor does not import them. No PlayStation icons: every pad shows the Xbox-style buttons. Loaded at runtime by `UI/PromptIcons.cpp`. Crediting is optional; the pages credit the pack anyway. |
