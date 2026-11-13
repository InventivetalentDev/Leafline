# Look pass: reference shots, part 1 values, cutaway diagnosis

Parallel agent for the main session, Fri 13 Nov 2026, 15:45 to 17:10. Covers parts 0 and 1 of `docs/briefs/look-pass.md`, and the diagnosis and framing steps of part 2. All shots are 1280×720, seed 517192, day 1 of the save slot `RefShot`, and they live in `Saved/RefShots/`. Unless marked otherwise they use the old bark (`Leaf.Bark 0`, so before and after stay comparable) and hardware ray tracing (`-rt`).

The owner's save `Saved/SaveGames/Leafline.sav` had md5 `cd04b6aa38ea4451fd94963c64a9bc11` (mtime 12:31:52) before the first run. It still had that md5 after every set.

## Recommended values

Part 1, lighting (tuning only):

```
Leaf.Tune cam WoodClearance 60
Leaf.Tune look SkyLightIntensity 3
Leaf.Tune look SkyLightColor (R=0.75,G=0.85,B=1.15,A=1)
Leaf.Tune cam ExposureBias 0.2
```

In `-ExecCmds` and `LOOK=`, replace the commas inside the colour with semicolons, like this:

```
LOOK="Leaf.Tune cam WoodClearance 60,Leaf.Tune look SkyLightIntensity 3,Leaf.Tune look SkyLightColor (R=0.75;G=0.85;B=1.15;A=1),Leaf.Tune cam ExposureBias 0.2"
```

Keep the fog values: no fog change measurably helped (see below). Keep depth of field unchanged (I didn't touch it). Keep `ThickWoodRadius` at 120 (lowering it made no difference in any tested pose).

Framing (part 2; this needs its own verdict from the owner playing):

```
Leaf.Tune cam BoomLength 170
Leaf.Tune cam DefaultPitch -25
```

I tested `DefaultPitch` -20 and -28. The value -25 is between them and was not shot by itself.

`WoodClearance 60` belongs to the camera, but it is the main lever in part 1. See "Why WoodClearance" before you adopt it, because the camera feels different near thick wood.

## Part 0: views and `Tools/ref_shots.sh`

Changes to the script (I made no other edits):

- Every run now passes `-LeafSlot=RefShot`. The script stores the md5 of `Leafline.sav` at start and stops with exit code 3 if the file changes.
- New view `junction`: `Leaf.Teleport 21 0.08` (limb 21, radius 92 cm where it leaves the trunk), then `Leaf.Aim trunk -80`. At the brief's +45° (`t1-junction-rt.png`, `t2h81-…`, `t2h261-…`) there is no wedge on this seed. At −80° (looking down onto the junction) the black wedge shows clearly, so −80 is the default. You can still set `JUNCTION_PITCH=45`.
- New view `sunward`: `Leaf.FaceWind sheltered`, then `Leaf.Aim 170 30`. Shot 1 is shift 1: its first worker pip is lit. In shift 1 the sun stands at pitch −22° and yaw −10.3° (`Leaf.LightInfo`), so its azimuth is about 170°, at 4600 K. The day-1 wind on this seed blows toward yaw 245, so the lee side of the trunk is about 75° from the sun's azimuth. From there the camera looks along the trunk's tangent toward the sun. Because pitch +30 puts the camera inside the trunk, the thick-wood rule moves it onto the bark, as in shot 1. The amber wash reproduces (`look0-sunward-rt.png`).
- New view `bough` (sweeps only, not in the default set): `Leaf.Teleport 55 0.37` puts the ant on a bough of 30 cm radius (found with `Leaf.Orbit 1 30`).
- `PITCHES="-80 -45 -10 25 60"` sweeps the pitch for each view taken. Files are named `<label>-<view>-p<pitch>-<mode>.png`.
- `SHIFT=2|3` sends one or two workers home with a fragment first, so the sun stands at noon or evening. Shift 2 works (`l1noon-trunk-rt.png`). One `SHIFT=3` run produced no screenshot, and I didn't retry it.
- `--probe "<commands>" [seconds]` runs the game once without a screenshot and prints the `LogLeafline` lines. I used it to find limb radii, limb headings (from the camera yaw that `Leaf.LightInfo` logs after a teleport), and the trunk spots that face the sun.
- `docs/tools.md` still needs these options; I didn't edit it.

The existing `trunk` view (`Leaf.Teleport 0 0.3`, `Leaf.Aim up 25`) already reproduces the look-up wedge of shots 2 and 3 almost exactly: the ant on a band of bark that crosses the frame, a storm-dark sky, and a black wedge below the ant (`look0-trunk-rt.png`). Use it alongside `junction`.

### `look0` set (old bark, both modes)

`Saved/RefShots/look0-{trunk,canopy,base,junction,sunward}-{rt,nort}.png`.

Mean absolute difference between ray tracing on and off, per channel out of 255: trunk 4, canopy 3, base 1, sunward 1, junction 14. The junction difference is larger, but I didn't look into it.

## Part 1: lighting and haze

### `sunward`: the bark under the ant is cut away, not hazed

In `look0-sunward` the lower half of the frame, where the bark the ant stands on should be, shows sky and ground instead. The thick-wood rule puts the camera only `WoodClearance` (12 cm) above the bark, and the cutaway (55 cm round the camera-to-ant line) then removes a trench of bark in front of the camera. Through that trench you see past the trunk.

The evidence:

- With `Leaf.Tune cam CutawayRadius 1` the lower half is solid brown bark (`dgCut1-sunward-rt.png`; region round the ant rgb(79,51,27), sat 0.66).
- With all fog off (`FogDensity 0`, `bVolumetricFog False`, in `l1nofog-sunward-rt.png`) the region doesn't change.

At first I took the lower half for a grazing-angle reflection; the `CutawayRadius 1` shot rules that out. The fog rows of the table below therefore measured mostly the hole, and I didn't repeat them on visible bark.

I measured mean RGB, saturation and the luminance standard deviation in the box x 380–900, y 470–700:

| Shot | Change | Bark round the ant | Far ground (top right) | Round the sun |
|---|---|---|---|---|
| look0 | none | rgb(112,84,50), sat 0.55 | rgb(49,34,12) | rgb(241,228,204), contrast 14 |
| l1a | FogStartDistance 1000 | identical | identical | identical |
| l1b | SunVolumetricScattering 1 | rgb(108,82,48) | rgb(57,41,15) | contrast 17 |
| l1c | VolumetricFogScattering 0.3 | rgb(117,88,53) | rgb(60,42,17) | contrast 22 |
| l1d | FogSunGlowExponent 24 | rgb(111,84,50) | rgb(50,35,13) | contrast 15 |
| l1nofog | no fog at all | rgb(110,85,49) | rgb(62,45,18) | contrast 21 |
| l1wc40 | WoodClearance 40 | rgb(98,68,37), sat 0.62, fissures visible | rgb(59,41,16) | contrast 12 |
| l1wc80 | WoodClearance 80 | rgb(87,54,27), sat 0.68 | rgb(73,50,22) | contrast 11 |
| l1final02 | recommended values | rgb(114,81,52), bark with fissures and the ant's shadow | rgb(85,68,38), sat 0.55 (hazed) | rgb(195,184,158), contrast 55 (clouds visible) |

With the recommended values (`l1final-sunward`, `l1final02-sunward`), the ant stands on brown bark with readable fissures and its own shadow. The far ground below now separates by haze, as the brief asks, and the sun glare is smaller.

The rule behind this: `WoodClearance` must be at least `CutawayRadius` (55), or the cutaway digs into the bark the camera sits on. The look-up assist needs the same care: its `LookUpClearance` is 10, so with the assist on the trench would come back.

### Why WoodClearance

`WoodClearance` is used only by the rule that keeps the camera out of wood at least `ThickWoodRadius` (120 cm) thick, so it applies to the trunk, roots and the lower limbs. At 60 the camera never comes closer than 60 cm to that wood: it gets pushed out once it is within 60 cm, not only once it is inside. The setting fixes two things:

- the cut-away bark under the ant in `sunward` (above);
- the look-up wedge in the `trunk` view (`l1wc60-trunk-rt.png`, `l1final-trunk-rt.png`). The wedge is gone, and the ant is larger, on readable bark. With the camera 12 cm above the bark, the cutaway (55 cm round the camera-to-ant line) dug a trench about 40 cm deep into the trunk. With the camera 60 cm above, the trench no longer reaches the bark.

There is a cost. In the `trunk` view the sky's share of the upper frame falls from 48.7% to about 21.7%, because the camera now looks along the bark from higher up. Looking up from the trunk shows less canopy than before, while the brief wants looking up to get better. The look-up assist (part 2, step 4) is the place to win that back.

The owner should feel it on the trunk before it ships, since this is a camera change.

### Shadowed bark fill

The sky light changes shadowed bark only a little; Lumen GI carries most of the fill. I measured the darkest quarter of bark-coloured pixels:

| Shot | Canopy luminance, blue/red | Junction luminance, blue/red |
|---|---|---|
| look0 | 42.6, 0.54 | 41.3, 0.39 |
| SkyLightIntensity 2 | 40.9, 0.55 | 49.2, 0.42 |
| 2 + colour (0.75, 0.85, 1.15) | 44.8, 0.58 | 48.4, 0.50 |
| 4 + colour (0.7, 0.82, 1.2) | 46.6, 0.63 | 55.3, 0.55 |

I chose 3 with (0.75, 0.85, 1.15): a visible cool fill on the shadowed trunk that doesn't flatten the sunlit side. The black wedge itself stays black at any sky light setting, because nothing reaches it (see part 2). The warm sun and the leaf shadows on the trunk are unchanged in every shot.

### The storm-dark sky in shots 2 and 3

The dark sky is neither a designed storm nor an exposure fault alone. It comes from the first shift's low sun, and auto exposure metering on sunlit bark makes it worse. Intent covers the low warm morning sun, not the dark sky. The evidence:

- No code darkens the sky. `DayDirector::UpdateSun` steps only the sun's pitch, yaw and temperature through the shifts. Nothing in `WindSystem` or `DayDirector` touches clouds, atmosphere or exposure for a gust or a shift. Shot 2's "gust coming" is a coincidence.
- In shift 1 the sun is 22° up at azimuth 170°. The look-up views look high into the sky on the far side, the darkest part of a morning sky, at cloud sides that face away from the sun.
- The low sun darkens the sky by itself. At a fixed EV 1.5 (`ExposureMinEV` = `ExposureMaxEV` = 1.5), the `trunk` view's sky (box x 20–200, y 120–300) has luminance 41.6 at noon (`l1noonev15-trunk-rt.png`, shift 2) and 18.6 in the morning (`l1ev15-trunk-rt.png`). With auto exposure the values are 82.7 at noon (`l1noon-trunk-rt.png`) and 61.4 in the morning (`look0-trunk-rt.png`). The auto pair alone doesn't prove anything, because at noon the bark band is in shadow and auto exposure opens up; the fixed-EV pair does.
- Exposure scales sky and bark together. Fixed EV 1.5 and 3 (`l1ev15-trunk`, `l1ev3-trunk`, through `ExposureMinEV` = `ExposureMaxEV`) are both darker than auto. So auto exposure settles below EV 1.5, and the sky stays darker than the bark at every EV. In the frames, sky luminance divided by sunlit bark luminance (75th percentile) is 0.45 in `look0-trunk` and 0.47 to 0.54 in the owner's shots 2 and 3, against 0.66 in the views that look elsewhere at the same time (`sw0-junction-p60`, `t1-junction`).
- The more bark in frame, the darker the sky: with `WoodClearance 60` the trunk view is mostly bark, and its sky luminance drops from 70 to 48 at the same bias. `ExposureBias 0.2` brings it back to 55 with sunlit bark at 164 of 255, still unclipped. That is why I recommend 0.2 instead of the current −0.4.

Tuning cannot fully lift the morning sky without overexposing the bark, because the sky-to-sun ratio is fixed by the scene. If the owner wants a lighter morning sky, the next steps are scene changes: a brighter atmosphere (Rayleigh scattering, which has no tuning field), or a little less sun relative to the sky.

## Part 2: what the black wedge is

**It is bark buried inside another branch, seen through the cutaway hole.** That bark gets no direct light, because the shadow pass sees the whole, uncut tree (`ShadowReplace`). It also gets no sky light or GI, because it is enclosed. So it renders black. It is not the limb's own inner wall seen from behind: `M_TreeBark.uasset` has no `TwoSided` property, so its back faces are culled.

The evidence, at the `junction` pose (−80°) and in the `trunk` view:

| Shot | What changed | Result |
|---|---|---|
| `look0-junction-rt`, `sw0-junction-p-80-rt` | none | black wedge with hard edges on the limb next to the ant |
| `dgUnlit-junction-p-80-rt` | `viewmode unlit` | the wedge area shows bark (fissures, moss), continuous with the surrounding bark: it is real front-facing bark, only unlit |
| `dgCut1-junction-p-80-rt` | `Leaf.Tune cam CutawayRadius 1` | wedge gone, the limb is solid and lit: you see it only through the cutaway |
| `dgTW40-junction-p-80-rt` | `ThickWoodRadius 40` | no change |
| `dgUnlit-trunk-rt` | `viewmode unlit`, `trunk` view | the black area is a stretch of the trunk or limb column behind the band of bark the ant stands on, inside the band's wood; its curved lower edge is where that column comes out of the band, and below it the same column is lit |
| `dgCut1-trunk-rt` | `CutawayRadius 1`, `trunk` view | wedge gone |

**`ThickWoodRadius` does not settle it.** At +60°, the end of the sweep with the hard-edged view and the strip of bark between two holes, I tested `ThickWoodRadius` 40 and 1. The shots are identical to the baseline (`dgTW40-junction-p60-rt`, `dgTW1-junction-p60-rt`, `sw0-junction-p60-rt`). The camera is not inside wood there: it is under the 92 cm limb, and the cutaway opens the limb from below. At −80° the camera is above everything. Note that `ThickWoodRadius 0` doesn't mean "always": the `> 0` test in `AntCamera.cpp` turns the rule off entirely, which is why I tested 1 instead of 0.

At `t2h81-junction-rt` (limb 21, heading 81, pitch 45) the limb doesn't show at all, and the ant floats among leaves without a wedge. I expected the camera to be inside the limb there. `ThickWoodRadius 40` at that pose (`dgTW40h81-junction-rt.png`) gives the same picture, so the camera is probably not in wood there either: at 45° it ends up about 155 cm to the side of the ant and 155 cm below it. I found no pose where `ThickWoodRadius` changes the picture, so the recommendation to keep 120 rests on these four tests: −80 and +60 on the trunk heading, 45 on heading 81, and 1 at +60. I couldn't explain why the limb is missing from the heading-81 shot.

**Where it is fixed:**

- `M_TreeBark2` with vertex-alpha masking of buried bark: the main session reports that the −80° junction wedge is gone with it.
- `WoodClearance 60` removes the look-up wedge of the `trunk` view even with the old bark (above).

**Pitch sweeps (old bark, ray tracing):**

- Junction, `sw0-junction-p{-80,-45,-10,25,60}-rt.png`:
  - −80: the black wedge;
  - −45, −10 and 25: clean;
  - +60: hard-edged holes under the limb, the ant floating against the sky, and a thin strip of bark left between the holes.
- Bough, 30 cm, `sw0-bough-p{-80,-45,-10,25,60}-rt.png`: no black area and no inside view at any pitch. At −10 and 25 a leaf covers part of the ant.

These sweeps should be repeated with `M_TreeBark2` for acceptance test 2.

## Framing

Canopy pose (twig 400, heading 170), pitch −20, with only the boom changed:

| Shot | Boom | Pitch | Ant width (screen) |
|---|---|---|---|
| `fr220-canopy-p-20-rt.png` | 220 | −20 | ~4.4 % |
| `fr170-canopy-p-20-rt.png` | 170 | −20 | ~5.6 % |
| `fr150-canopy-p-20-rt.png` | 150 | −20 | ~6.3 % |
| `fr170-canopy-p-28-rt.png` | 170 | −28 | ~5.6 % |

I estimated the widths by eye; they agree with the boom ratio. At 150, the leaves near the camera crowd the frame and the route ahead shows less. At 170, the ant reads clearly and the branch ahead stays in view. −28 looks down a little more than −20, and both read well. Hence `BoomLength 170` and `DefaultPitch -25`.

The bough view (`fr*-bough-rt.png`) is not useful for framing: big leaves cover it.

## Before and after (old bark, ray tracing)

| View | Before | After (recommended part 1 values) |
|---|---|---|
| trunk | `look0-trunk-rt.png` | `l1final02-trunk-rt.png` |
| sunward | `look0-sunward-rt.png` | `l1final02-sunward-rt.png` |
| canopy | `look0-canopy-rt.png` | `l1final02-canopy-rt.png` (leaf signals unchanged: the pale rich leaf and the deep greens look the same, and no leaf pixel reaches G 245) |
| base | `look0-base-rt.png` | `l1final02-base-rt.png` (seen only as `l1final-base`, at bias 0: almost unchanged, the nest shadow a little cooler) |
| junction | `look0-junction-rt.png` | `l1final02-junction-rt.png` (wedge unchanged: tuning can't fix it, `M_TreeBark2` does) |

`l1final02-*` is the recommended set as written above (`ExposureBias 0.2`), all five views, with ray tracing only. `l1final-*` is the same with `ExposureBias 0`, in both modes. I looked closely only at canopy, trunk and sunward of the 0.2 set.

## Caveats

- **The build changed during this work.** `look0` and the tests up to `l1ev3` show the old HUD. From `l1noon` on, the shots show the new HUD, and the later ones may also include the build in which depth of field focuses on the ant's real distance. The lighting comparisons in this report pair shots of the same build, except `look0` against `l1final`.
- **Shared output.** The main session runs `ref_shots.sh` too. The log `Saved/Logs/RefShot.log` is shared, so a log read right after a run can already belong to the other session's next run. Screenshots are told apart by label.
- **Lost screenshots.** Both sessions touch the same `Saved/RefShots/.marker`, and a run decides which screenshot is its own by comparing against that file. So a concurrent run can make yours report "no screenshot"; that is the likely reason for the one missing `SHIFT=3` shot. The script's `timeout 240` also counts time spent waiting for the lock, so a long queue can lose a run as well. The script header lists `SHIFT=3` as an option, but it is unverified.
- **I broke two of the main session's runs.** I edited `ref_shots.sh` in place while those runs were executing, and two of them broke mid-edit. I will not edit it in place again.

## After (M_TreeBark2 and the new defaults)

Taken 17:14 to 17:27 on the build of 17:14. The new bark is on (no `Leaf.Bark 0`). The adopted defaults are `WoodClearance` 60, `SkyLightIntensity` 3, `SkyLightColor` (0.75, 0.85, 1.15), `ExposureBias` 0.2, `BoomLength` 170 and `DefaultPitch` −25. There is no `LOOK` except where noted. The sets:

- `look1-*`: the five views, both modes;
- `sw1-*`: the sweeps, ray tracing only;
- `sig1-*`: the signal shots, ray tracing only, plus the variants `sig1b`, `sig1c` and `sig1d`.

### The five views

| View | Before | After | What I see |
|---|---|---|---|
| trunk | `look0-trunk-rt.png` | `look1-trunk-rt.png` | No wedge, and the ant is larger. The bark has no black lines: fissures are soft, darker grooves; the plates run long along the trunk with varying tones; the moss lies in patches, not rings. Looking up shows little sky: 19.0% of the upper frame against 48.7% before. Sky luminance is 64 against 70, with sunlit bark the same (157 against 156). |
| canopy | `look0-canopy-rt.png` | `look1-canopy-rt.png` | Leaves as before. The pale rich leaf under the ant (cream) and the deep greens read clearly. A white cut-highlight outline at the bottom right runs on past the leaf's visible edge, where that leaf has faded near the camera. The same ghost outline appears in `sig1c` at the lower left, but I didn't check whether it is new. |
| base | `look0-base-rt.png` | `look1-base-rt.png` | Much the same. The trunk base shows the new plates. Haze toward the low sun is as strong as before, and the near ground keeps its colour. |
| junction | `look0-junction-rt.png` | `look1-junction-rt.png` | No black wedge: where it was, you now see the bark of the farther limb. New: the wood near the camera blurs into grey streaks (depth of field); shadowed bark is slate grey-green, not brown; and the right-hand surface shows big light/dark triangles that read like folds or facets. |
| sunward | `look0-sunward-rt.png` | `look1-sunward-rt.png` | The ant stands on brown bark with plates and its own shadow, clearly apart from the haze (acceptance test 3). The far ground separates by haze: rgb(49,34,12) before, rgb(95,77,44) after. Sun glare is down: sun-area luminance 241 to 194, detail contrast 14 to 55. |

Mean absolute difference between ray tracing on and off, per channel out of 255: trunk 5, canopy 5, base 2, sunward 3, junction 10. That is as close as before, and the junction is again the largest.

### Acceptance test 2: pitch sweeps

The files are `sw1-{junction,bough}-p{-80,-45,-10,25,60}-rt.png`.

**Junction.** No pitch shows a black area or the dark inner walls of a limb.

- −80 and −45: clean, but with the grey streaks and fold-like triangles described above.
- −10 and +25: the cutaway shows as a visible hole in the trunk wall next to the ant, with leaves and sky seen through the bark. At +25 the hole's upper edge is still fairly hard.
- +60: as before (`sw0-junction-p60`), the camera is under the limb. Large grey planes of the underside end in a hard curved edge, and the ant stands on that edge against the sky. The edge is softer (dithered) on the left. The thin strip of bark between two holes is gone.

**Bough.** No black area and no view into a limb's interior at any pitch. The branch between the camera and the ant renders dark slate grey-green and heavily blurred.

### Acceptance test 5: signals

- **Wind map, `sig1-trunk-rt.png`** (`Leaf.Exposure 1`, `trunk` view): green and amber spots on the bark round the ant, readable against the new bark. The HUD's "Here: partly exposed" is amber, the same colour as the spots. Red (exposed) is not in frame at this pose.
- **Leaves, `sig1c-canopy-rt.png`** (`Leaf.Strip 133.8`, `canopy` view): stripped leaves show as brown stalks, clearly different from the green leaves beside them.
- **Leaves, the rich one:** the two leaves left in `sig1c` are both light green. The pale cream rich leaf shows next to deep plain greens in `sig1d-canopy-rt.png` (`Leaf.Strip 133`) and in `look1-canopy-rt.png`, but those have no stalks in frame. `sig1-canopy` (134.5) and `sig1b` (136) strip the whole twig.
- **Missing:** a single shot with rich, plain and brown together. A strip height between 133 and 133.8 (for example 133.4) should give it, once the lock is free.

### Still wrong, or for the owner to judge

1. **Shadowed new bark reads slate grey-green rather than brown**: at the junction, on the bough, and most strongly on wood near the camera. Sunlit bark is a good tan. Likely causes are the cool sky light (×3, blue tint) on the new base colour, or the moss. I haven't tested which. The next test: `LOOK="Leaf.Tune look SkyLightColor (R=1;G=1;B=1;A=1)"` on `junction` and `bough`, then `SkyLightIntensity 2`.
2. **Blur near the camera.** Depth of field now focuses on the ant's real distance, and the boom is 170. Wood within about half a metre of the camera smears into streaks at the junction and on the bough. The route ahead stays readable in `trunk`, `canopy` and `sunward`.
3. **Fold-like light/dark triangles** on bark seen at a low angle at the junction (`look1-junction`, `sw1-junction-p-80` and `p-45`). This could be the plate normals under grazing light, or mesh facets; stills can't tell which.
4. **Cutaway holes are still visible** at the junction (−10, +25, +60). They are no longer black and are soft on some sides, but hard at the top at +25 and on the right at +60.
5. **The ghost leaf outline** (canopy).
6. **What stills can't judge:** shimmer, and the bark's fine grain in motion; those need the package.

Nothing I saw contradicts the other targets: no black fissure outlines, no haze on near bark in `sunward`, and the leaf and wind-map colours read as listed in the brief.

### Missing shots

- The three-signal leaf shot (rich, plain and brown together), as above.
- `SHIFT=3` (evening), never verified.

Everything else was taken: `look1` 10 of 10, `sw1` 10 of 10, and both `sig1` shots, plus the strip variants.
