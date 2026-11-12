# Codex review: M4 screens, runtime audio, pause, movement (2026-11-12)

Codex reviewed `5333dec..70f5910` (M4 screens, the runtime audio subsystem, the pause fix, the movement changes) read-only, with static inspection, numerical and state-machine reproductions, and checks against the installed UE 5.8 source. Engine execution and packaging were not run. It found no double-spend path and no packaging blocker; WAV headers and UFS staging checked out. Findings copied and condensed by the main session, with what was done.

| # | Severity | Finding | Resolution |
|---|---|---|---|
| 1 | Medium | The world runs behind the title screen. If every worker is blown off and walks home before the player confirms, play starts in Shift with nobody possessed and nothing left to end the day. | Confirming the title possesses the next worker if nobody is possessed, then checks whether everyone is already home (which ends the day normally). |
| 2 | Medium | The twig-tip fade applied in both directions, so an ant resting close to the apex could not leave it: full reverse input faded below the dead zone. | Removing the fade for "away" made the ant pace at tips (near the apex the toward/away judgement flips by itself), and judging from the raw input paced at the cap edge. Final: the fade stays in both directions, but once the ant settles at the apex it latches the stick direction; it holds until the stick turns round, then leaves at full push. Verified in PIE on a 4 cm twig: forward holds with no movement at all (the old 1 cm jitter is gone), reverse leaves at once, wandering runs show no projection failures. |
| 3 | Medium | The cutting loop belongs to the world's audio subsystem and outlived its pawn (for example `Leaf.NewDay` mid-cut), stacking loops. | `AAntPawn::EndPlay` stops the cutting and grip loops. |
| 4 | Medium | `CreateSound2D` marks every voice as a UI sound, so cutting and grip loops kept playing through pause. | Only sounds named `ui_*` stay UI sounds; gameplay voices pause with the game. |
| 5 | Medium | The pending end card lived only in the director. Quitting at the winning recap lost it for that save forever. | The save records `bEndCardSeen`; a colony past the last milestone that has not seen it gets it at the next recap. |

Built 2026-11-12. #2 verified in PIE as above. #1, #3, #4 and #5 compile and were checked by reading the code only; they were not exercised in PIE.
