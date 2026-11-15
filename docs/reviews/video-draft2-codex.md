1. **The five biggest improvements**

   1. **Show delegation.** Add five readable seconds of recruits following a scent trail. This is the strongest missing evidence for innovation and the theme.
   2. **Repair the action order:** cut → wind → send home → next worker. Currently the ant starts home before the wind insert, while the explanatory caption arrives afterward.
   3. **Remove clip 4’s camera lurch.** Cut its opening use entirely; the climb already establishes that view.
   4. **Replace the recap with an earned result.** “Trail laid: 0 m” contradicts the climb. The subsequent 11.4 → 211.4 jump comes from `Leaf.Give 200`, making progression look unearned.
   5. **Get to decisions earlier.** The first cut starts at 19.5 seconds. Reduce the scenic introduction, shorten the menu hold, and spend that time showing workers and consequences.

2. **Motion**

   I inspected the whole draft at **15 frames/second**, covering every cut and camera move; checked selected source passages at **30 frames/second**; and inspected full-resolution HUD frames. [Extracted sheets](/mnt/d/UnrealProjects/AntTreeGame/Saved/VideoCap/codex-review/motion/draft_06.jpg).

   Times below are **draft times**, except where explicitly labelled source.

   | Clip / time | Finding and edit |
   |---|---|
   | 1, 0.00–0.50 | Intentional black fade delays finding the ant. Shorten to approximately 0.15 s. |
   | 1 → 2, 2.80 | Abrupt change from looking down the trunk to looking into the crown. It works as an intentional scale reveal, although it cannot read as continuous movement. Keep a hard cut. |
   | 2, 2.80–3.10 | A foreground leaf still grazes the top edge at the incoming cut. Start **spire source 7.6**, rather than 7.3. |
   | 2 → 3, 8.00 | Clean photographic cut; the warmer, closer ant is a useful change of scale. |
   | 3 → 4, 12.00 | The moving orbit stops against another trunk composition. Individually clean, editorially repetitive. |
   | 4, approximately 13.73–14.23 | **Definite lurch:** camera plunges toward the bark, with heavy blur and rapid reframing. Source approximately **1.73–2.23**. This is the crane entrance inside the selected window. |
   | 4 → 5, 14.50 | Another abrupt change in ant size and camera distance on nearly the same trunk axis. Removing clip 4 solves both problems. |
   | 5 → 6, 19.50 | No obvious camera fault. The sudden HUD and foliage load makes the viewer search again for the ant. |
   | 6, 23.53–23.93 | The finished fragment swings broadside, then almost edge-on. It briefly reads as a leaf popping into view; the dense source sequence shows a rotation/lift, rather than a stall. |
   | 6 → 7, 25.20 | **No picture discontinuity:** these are contiguous portions of the same take. Only the caption disappears. |
   | 7 → 8, 26.30 | Major continuity problem: the returning ant is replaced by a stationary loaded ant elsewhere. Put wind **before** the send-home action. |
   | 8, 26.30–31.30 | Rapid leaf oscillation, conspicuous blur/ghosting and changing streak patterns. Particularly distracting around **28.2–28.6, 29.4–29.6 and 30.7–30.9**. No obvious camera reset, but the leaf can look unstable. |
   | 8 → 9, 31.30–32.10 | Re-entry lands during the handoff camera’s descent. Bark, ground and framing move rapidly before the nest settles. Use **cut source 9.0 onward** for the new worker. |
   | 9 → 10, 34.10–34.30 | Near-matching nest views jump position, the ant disappears, then the recap appears. Start recap at **source 0.7**, after the panel is present. |
   | 10, approximately 40.20 | Growth/currency jumps without a visible gameplay cause. This is the scripted grant, not an exposure or rendering fault. |
   | 10 → 11, 42.70 | Abrupt change into a much darker, less colourful image. Deliberate end-card treatment, but it feels more sombre than the preceding film. |

   The ant orbit, climb and end orbit progress evenly in the sampled frames. I found no obvious sustained freeze or repeated-frame run; frame sampling cannot certify playback smoothness.

   **Omitted spire footage:** source **4.8–5.5** contains a fast, blurred branch passage; **5.7–7.3** contains foreground foliage, including near-total occlusion around **6.9**. Keep this passage excluded.

3. **The hook**

   **The scale is promising; the opening is not yet a strong gameplay hook.** For almost three seconds, a new viewer sees patterned bark and a very small ant. The crown finally supplies context.

   Keep the owner’s spire opening: **1.8 seconds on the ant, then the crown/title**. Follow with the sunlit ant briefly, then the scent-trail climb. The revised list reaches active climbing at **9.0 s** and cutting at **15.0 s**.

   Open on the spire, but make “tiny creature, enormous climb” apparent before the viewer has to work to identify the subject.

4. **Pace, clip by clip**

   All in/out points here are **seconds within the source take**.

   | Draft clip | Decision | Proposed source selection |
   |---|---|---|
   | 1 — spire bark | Trim | `spire-take2 0.2–2.0` |
   | 2 — spire crown/title | Trim | `spire-take2 7.6–11.8` |
   | 3 — ant orbit | Trim | `ant-take3 1.0–4.0` |
   | 4 — tree opening | Cut here; repurpose crown later | `tree-take2 9.0–12.8`, after upgrades |
   | 5 — climb | Extend slightly | `climb-take3 0.5–6.5`; the extra source footage remains stable |
   | 6 — cut | Trim | `cut-take4 0.8–5.7` |
   | 7 — return begins | Extend and move after wind | `cut-take4 5.7–7.4` |
   | 8 — wind | Extend to show relief | `wind-take3 1.8–8.8`; includes gripping onset and the return toward calm |
   | 9 — next worker | Trim incoming flight | `cut-take4 9.0–10.8` |
   | 10 — recap/nest | Split and trim | `recap-take1 0.7–4.2`, then `7.0–10.0` |
   | 11 — end card | Trim | `end-take2 0.5–6.0` |

   **48.7 seconds is a suitable overall length. Its allocation is the problem.** The existing-footage proposal below is **46.2 seconds**. With a delegation replacement, approximately 46–50 seconds should be enough.

5. **Story, theme and judging**

   A new judge will understand **an ant climbs, selects a cut, carries a leaf, faces wind, and returns control to the nest**. They are less likely to understand that the route becomes an instruction for other ants.

   - **Set the vision:** the climb shows a trail, but “scent trail” needs naming.
   - **Decide what goes in:** the growing cut arc and risk colours communicate this well.
   - **Delegate:** presently missing. Sending one ant home demonstrates automation only partially; recruits must visibly use the route.
   - **Progression:** the nest proves upgrades exist, but the purchase is subtle. Neither depleted low branches nor an earned move is demonstrated.

   The draft’s strongest judging evidence is **visual scale and the cut-size idea**. Fun, colony ambition and route-based level decisions are underrepresented.

   Keep the dropped ride, harvest, move and bird shots out. Add a **new stationary colony view**, rather than restoring those shots. Use the existing Brood purchase to explain recruitment, then ideally show its workers. The late `tree-take2` crown can suggest the next objective, but does not prove progression or a colony move.

6. **Picture problems**

   | Clip / draft time | Problem |
   |---|---|
   | 1, 0.50–2.80 | Ant is difficult to identify against the bark pattern; little silhouette or environmental context. |
   | 2, approximately 4.0–5.5 | White title crosses bright foliage, weakening contrast. |
   | 3, 8.0–12.0 | Attractive light, but the largely stationary ant makes this feel like a model showcase. Three seconds is sufficient. |
   | 4, 13.73–14.23 | Severe motion blur during the camera entrance. |
   | 5, 14.5–19.5 | Ant and pale trail compete with strong bark lines. The route reads better than the character. |
   | 6, 19.5–25.2 | Foliage crowds the cutting area; the ant sits below the visual centre while the ring attracts attention above it. |
   | 6, approximately 23.6 | The fragment’s blurred lift makes the actual bite and attachment harder to understand. |
   | 8, throughout | Bright fragment dominates the ant. Streaks can resemble rain; gripping is much less obvious than leaf flapping. |
   | 9, approximately 32.0–33.7 | Long burned caption meets the centred gameplay prompt, creating overlapping reading zones. |
   | 10, 34.3–39.7 | “Trail laid: 0 m” reads as a contradiction or bug after the trail shot. Largest piece **3.8** also differs from the earlier **4.4** cut. These are separate demonstrations presented like one trip. |
   | 10, 39.7–42.7 | Brood purchase changes a small pip and price while “Start day 2” stays highlighted. A judge may miss the purchase entirely. |
   | 11, 42.7–48.7 | Heavy dimming suppresses the warm colour and nearly loses the ant. Consider reducing the end-card black overlay from 45% to roughly 30%. |

   At native **1080p**, the main cut number and risk message are readable. Smaller HUD explanations and upgrade descriptions require deliberate reading; they should support the film, rather than carry its essential explanation.

7. **Captions, title and end card**

   Use these replacements at the revised timeline positions:

   | Film time | Text |
   |---|---|
   | 1.8–6.0 | `@title LEAFLINE` |
   | 9.0–15.0 | `Lay a scent trail.` |
   | 15.0–19.9 | `Choose your cut.` |
   | 19.9–26.9 | `Bigger cuts catch the wind.` |
   | 26.9–28.6 | `Send it home.` |
   | 28.6–30.4 | `Next worker.` |
   | 30.4–33.9 | `A day's haul.` |
   | 33.9–36.9 | `Recruit more workers.` |
   | 36.9–40.7 | `Climb for richer leaves.` |
   | 40.7–46.2 | Existing end-card wording |

   That is **eight short explanatory captions plus two title treatments**. If delegation is refilmed, replace the late canopy caption with **“Your colony follows your trail.”**

   Keep lower-left placement and approximately 49 px text at 1080p. Shortening the handoff wording prevents the current prompt collision. Keep centred text for title/end only.

   The end wording already works: **“Direct the colony. Choose every cut.”** There is no need to explain the jam theme explicitly. The current title implementation delays its appearance by one second; in this proposal it becomes fully visible around **3.6 s**.

8. **Sound**

   Using the revised timeline:

   - **0–9 s:** restrained wind and the game’s ambient pad. Let the crown reveal broaden the ambience; close ant sounds should remain modest while the ant is standing.
   - **9–15 s:** bring footsteps forward, making the climb feel tactile.
   - **15–19.9 s:** cutting texture builds with the arc. Land the release crunch around **18.73 s**.
   - **19.9–26.9 s:** warning → rising gust → strongest wind → relief. Approximate source-derived beats: grip **20.12**, wind rising **20.60**, full gust **21.82**, falling **24.82**, calm **26.83**. Keep the gust the loudest passage.
   - **26.9–30.4 s:** return/selection cues punctuate the handoff. Bridge the omitted camera travel with ambience.
   - **30.4–36.9 s:** use the recap’s own tally sounds and purchase cue, approximately **34.9 s**. Let them register above the bed.
   - **36.9–46.2 s:** open the ambience for the canopy, then settle into the card and fade.

   For new colony footage, let several small footstep rhythms establish multiple workers. Use deposit chimes only where actual deliveries justify them.

   No extra music is needed. Target approximately **−14 LUFS integrated**, with true peak below **−1 dBTP**. Match sound to each take’s recorded setup: `wind-take3` used **piece 0.6, aim 40 −25**, which differs from the script’s current defaults.

9. **Revised cut list — existing takes only, 46.20 s**

   This skips the visible currency grant, but the recap remains a separate staged demonstration with inconsistent results. Replace that footage before submission if possible.

   Saved as [reviewed_cut.txt](/mnt/d/UnrealProjects/AntTreeGame/Saved/VideoCap/codex-review/reviewed_cut.txt); all source ranges were checked against take durations.

   ```text
   # <take> <in> <out> | <caption>
   # Existing takes only. Total: 46.20 s.
   spire-take2    0.2   2.0   |
   spire-take2    7.6  11.8   | @title LEAFLINE
   ant-take3      1.0   4.0   |
   climb-take3    0.5   6.5   | Lay a scent trail.
   cut-take4      0.8   5.7   | Choose your cut.
   wind-take3     1.8   8.8   | Bigger cuts catch the wind.
   cut-take4      5.7   7.4   | Send it home.
   cut-take4      9.0  10.8   | Next worker.
   recap-take1    0.7   4.2   | A day's haul.
   recap-take1    7.0  10.0   | Recruit more workers.
   tree-take2     9.0  12.8   | Climb for richer leaves.
   end-take2      0.5   6.0   | @end LEAFLINE / Direct the colony. Choose every cut. / inventivetalent
   ```

   Export from the **1080p60 takes**, rather than upscaling the preview.

10. **Reshoots: only two footage gaps justify them**

   These are proposed `shot_def` changes, **not executed or verified**.

   **A. Add a stationary delegation shot.** Use trail following only to position the camera before filming:

   ```bash
   colony)
       POOL=6; CALM=1; HUD=0
       STEPS=(
           "2|Leaf.TestLines 1 0.55 sheltered 517192"
           "9|Leaf.Hold follow 3"
           "13|Leaf.Tune cam BoomLength 240"
           "13.1|Leaf.Tune cam DepthOfFieldFstop 0"
           "13.2|Leaf.Aim up 25"
       )
       DUMP_AT=16; DUMP_LEN=10 ;;
   ```

   Film **launch seconds 16–26**, selecting five to seven seconds with several recruits visibly passing along the same route. The positioning and traffic timing need a rehearsal: adjust the follow duration or filming window if workers remain behind the camera. Accept the shot only when the trail and multiple ants read immediately. Replace the late canopy shot plus a little scenic time.

   **B. Refilm the recap from completed lines.** Replace the three `Leaf.Carry` deliveries and remove `Leaf.Give 200`:

   ```text
   HUD=1; POOL=6; CALM=1

   2.0 | Leaf.TestLines 3 0.55 sheltered 517192
   5.0 | Leaf.Harvest
   5.1 | Leaf.SkipHarvest
   ```

   Probe to find the actual recap onset, **R**. Then schedule:

   ```text
   R+3.8 | Leaf.Hold grip 0.2
   R+5.3 | Leaf.Buy 4

   DUMP_AT = R+0.2
   DUMP_LEN = 8.0
   ```

   That filming window is **R+0.2 through R+8.2**: tally, readable result, nest, purchase, aftermath. Check that completed deliveries cover the purchase and that trail distance is nonzero. An absolute launch time cannot be established without running the probe.

   The spire, ant, climb and handoff problems can be handled with existing takes; they do not need another capture.