# itch page text: Epic AI MegaJam (frozen at submission)

<!-- Owner: fill the bracketed placeholders. -->

**Title:** Leafline
**Short description:** You're a leafcutter ant on a giant tree. Cut leaves, lay scent trails and send the colony after you.
**Tags (suggested):** Singleplayer, 3D, Procedural Generation, Ants, Unreal Engine
**Platform:** Windows
**Theme colours (owner, 15 Nov, "Evening bark"):** BG `#16110D`, BG 2 `#211A14`, Text `#EFE6D2`, Link `#9BD36A`
**Download:** `inventivetalent_Leafline.zip` (0.47 GB). Unzip it and run `Leafline/AntTreeGame.exe`.
**Video:** [VIDEO URL]

---

## About

You're a leafcutter ant on an enormous tree. Climb, lay a scent trail and cut a piece from a leaf. A bigger piece is worth more, but it slows you down and catches the wind like a sail. Send your ant home, take over the next one, and let your recruits follow your trails and harvest along them.

The colony keeps its tree from day to day. Cut leaves don't grow back, and leaves higher up are worth more, so you climb higher and into stronger wind until the colony moves to a new, taller tree.

Made for the Epic AI MegaJam 2026 (theme: Director's Cut): you direct the colony with your trails and choose every cut.

## How to play

1. Climb from the nest to a leafy branch. Your ant leaves a scent trail as it walks.
2. Hold Cut at a leaf to choose the size of the piece, then let go.
3. Send the ant home. It follows the trail by itself while you take over the next worker.
4. When a gust comes, hold Grip or get into shelter. An ant that's blown off loses its piece and walks home.
5. At the end of the day, spend leaf mass on upgrades in the nest. Two recruits work your trails from day 1, and Brood adds more.

Hold Tab (LB on a pad) for the wind map: green bark is sheltered, red is exposed.

From day 2, a bird rides the wind. It swoops at ants carrying pieces on exposed bark. It never touches them; its wing gust knocks them off. Grip or get into shelter when it dives.

Drop your piece in the open (G, or B on a pad) to draw the bird away for a while.

## Controls

| Action | Keyboard and mouse | Gamepad (Xbox names) |
|---|---|---|
| Move / look | WASD / mouse | left / right stick |
| Cut (hold) | E or left mouse button | X |
| Send home | Q | Y |
| Grip (hold) | Space | A |
| Ride a trail (hold) | F | RB |
| Wind map (hold) / switch ant (tap) | Tab | LB |
| Sprint (hold) | Left Shift | LT |
| Drop your piece for the bird | G | B |
| Pause | Esc | Start |

The pause menu lists all controls and has options for sensitivity, volume and graphics.

## How it's made

The tree, leaves, forest floor, sky, scent trails and wind are all generated with code and shaders: no textures, no hand-made tree models. Made with Unreal Engine 5.8 by one person directing AI agents (Claude and Codex), which wrote most of the code and made the key art and the HUD icons. The UI panel art is hand-painted by Cookiehero.

The sounds and the three music tracks (one per day: warm pads, kalimba, a music box) are synthesised by Python scripts too.

Button prompt icons: ["Xelu's FREE Controller Prompts"](https://thoseawesomeguys.com/prompts/) by Nicolae (Xelu) Berbece, updated by Paul Paun, CC0 1.0.

## Credits and links

- Made by inventivetalent
- UI panel art, the frames and backgrounds of the menu screens and the HUD boxes: [Cookiehero](https://cookiehero.itch.io/). The rest of the interface (layout, text, icons, meters) was made with code and AI tools.
- Playtesting and design feedback: [iCeeker](https://iceeker.itch.io/)
- Dev log: [docs/devlog.md on GitHub](https://github.com/InventivetalentDev/Leafline/blob/megajam-submission/docs/devlog.md)
- Source code, as submitted: [github.com/InventivetalentDev/Leafline (tag megajam-submission)](https://github.com/InventivetalentDev/Leafline/tree/megajam-submission)
