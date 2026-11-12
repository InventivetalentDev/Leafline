# Leafline: technical design

What to build and why it is shaped this way. Decisions are firm unless marked **Verify** (an engine behaviour to confirm in UE 5.8 before relying on it) or **Tune** (a number to adjust by play). The game rules are in `design.md`.

## The central idea: one distance field

The tree is defined by a skeleton of branches. From that skeleton we derive a **signed distance function** `D(p)`: the distance from any point to the tree's surface, negative inside. Everything that touches the tree asks `D` instead of using physics collision:

| Consumer | What it asks |
|---|---|
| Ant movement | Project my position back onto the surface; which way is "up" here |
| Camera | March from the ant to my desired position; stop before the tree |
| Wind | Is this point facing the wind, and is the tree upwind of it |
| Trail and replay | Re-project recorded points onto the surface |
| Leaf and nest placement | Where exactly is the surface |

Why this shape:
- Walking on collision meshes gives jittery normals and gets pawns wedged where a branch meets its parent. A smooth field has a continuous gradient everywhere, including at junctions, so both problems cannot occur.
- No collision cooking for a tree that is regenerated at runtime.
- Unattended ants need no navigation: they interpolate a recorded path and project onto the field.

`D` is cheap enough to brute-force for the handful of queries per frame we need (see Performance).

## Units and scale

Unreal units are centimetres. One **ant length (AL) = 20 cm**. This is about 25 times life size, which keeps camera near-planes and float precision comfortable.

| Quantity | Value | Note |
|---|---|---|
| Ant body length | 20 cm | 1 AL |
| Trunk radius at chest height | 750 cm | circumference about 47 m, 235 AL |
| Tree height | about 300 m | a 12 m tree at 25x |
| Walk / sprint / trail-ride speed | 200 / 400 / 600 cm/s | **Tune**. Walk circles the trunk in about 24 s |
| Leaf length | about 200 cm | 10 AL |
| Fragment diameter | 30–100 cm | 1.5–5 AL |
| Lowest leafy branch, day 1 | about 40 m up | first cut inside 90 s |

## Module layout

One runtime module, `AntTreeGame`. Suggested folders under `Source/AntTreeGame/`:

```
Tree/      TreeTypes.h, TreeSkeleton, TreeField, TreeMeshBuilder, TreeActor, LeafSet
Ant/       AntPawn, AntMovement (surface walker), AntVisual, AntCamera
Colony/    Worker roster, TrailNetwork, NestActor
World/     WindSystem, DayDirector, sky and ground setup
Game/      LeaflineGameMode, LeaflineGameInstance, LeaflineSaveGame, Upgrades
UI/        HUD and menu screens
Debug/     console commands
```

All gameplay is C++. Blueprint subclasses exist only to hold tuning values and asset references. Anything an agent has to edit should be text: input actions, mapping contexts and UI are created in C++ rather than as editor assets. Materials are the unavoidable binary assets and are built through the editor MCP.

## Tree

### Skeleton (`TreeSkeleton`)

Pure data, no engine objects, deterministic from a seed (`FRandomStream`).

```
FTreeNode   { FVector Pos; float Radius; }
FTreeBranch { TArray<FTreeNode> Nodes; int32 Parent; float ParentT; int32 Level; FBox Bounds; }
FTreeSkeleton { TArray<FTreeBranch> Branches; FTreeParams Params; int32 Seed; }
```

- Level 0 is the trunk, 1 limbs, 2 boughs, 3 twigs. Leaves grow on levels 2 and 3.
- The trunk has a slight lean and curve and a **root flare** (radius grows toward the ground). Add three to five short root branches that dive into the soil; the nest sits between two of them.
- Children leave the parent at a golden-angle spacing around it with jitter, pitch upward, and take a fraction of the parent's radius at that point.
- Growth avoids existing branches: test each new segment against grown branches, bend away inside a margin, and stop the branch if it would still pass through one.
- `FTreeParams` holds every knob, including `LowestLeafyHeight`, which `DayDirector` raises each day.
- A branch centreline is evaluated as a Catmull-Rom curve through its nodes so the surface has no facets.

### Field (`TreeField`)

```
float   Distance(const FVector& P) const;            // D(p)
FVector Gradient(const FVector& P) const;            // normalised, outward
bool    Project(FVector& P, FVector& OutNormal, float Height) const;
bool    March(const FVector& From, const FVector& To, float Radius, float& OutT) const;
```

- Per branch: the pieces between consecutive mesh rings. Each piece is bounded by the two ring planes exactly as `TreeMeshBuilder` orients its rings (central-difference tangents), and blends centre, radius and axis from one ring to the next by where the point sits between the planes, so the field passes through every ring and neighbours agree on their shared plane: no gap on the outer side of a bend, no overlap on the inner side. Only the branch's two ends are rounded caps. (Earlier versions capped every joint with a sphere, which bulged metres out of the root flare, then used cones bounded by planes perpendicular to their own axes, which left a ridge up to 20 cm tall on the outer side of every slight bend: the day-4 "can't climb past this line" bug.) `Leaf.MeshGap` checks the field against every visible mesh vertex.
- The ground plane carries the nest mound, a half-buried ellipsoid sized by `FTreeParams::NestMoundRadius`/`NestMoundHeight`; `ANestActor::SetMoundSize` scales the visible mound to match.
- Combine all branches **and the ground** with a polynomial smooth minimum. The blend radius `k` gives a small fillet at every junction and at the root flare. Start with `k` = 30% of the child's radius, capped at 1.5 AL. **Tune.**
- `Gradient` by central differences is fine (six extra evaluations).
- `Project` moves a point to the level set `D = Height` with three Newton steps: `P -= Gradient(P) * (D(P) - Height)`.
- `March` is sphere tracing, used by the camera and the wind.
- Acceleration: an AABB per branch, skipped when the query point is farther than the current best distance. Add a uniform grid of segment indices only if profiling asks for it.

Known compromise: the visible mesh is separate tubes that intersect, while the field has fillets. In the crease of a junction the ant rides slightly above the mesh. Keep `k` small enough that this is not visible; moss or bark colour variation at junctions hides the rest.

### Mesh (`TreeMeshBuilder`, `TreeActor`)

- Tubes around each centreline, rings of vertices, closed ends, built into a `UProceduralMeshComponent`. One section for wood. No collision on it.
- The component is transient and created on demand, never saved into the level. The actor's root is a plain scene component.
- Vertex data for the bark shader: UV0 = (arc length around, distance along the branch, both in cm), UV1 = (R cos θ, R sin θ) in cm, vertex colour = (branch level, random per branch, height on tree, unused). `M_TreeBark` evaluates 3D noise at (UV1, 0.15 × along + 5000 × random): a cylinder around each branch, so there is no seam around, features keep their physical size, tips close to a point, and fissures run along the branch. On the steep root flare the stretch is lost (the radius changes faster than 0.15 per cm), so the flare reads as plates. UVs are half precision, a few cm of step high on the trunk, which the long fissures hide. `M_Bark` is the older UV-based version, kept as a fallback.
- **Bark relief (presentation milestone, after the loop works).** Add one analytic ridge function `R(angle, along)` to both the field (`D -= R`) and the mesh vertices, so the fissures in the concept art are real geometry the ant walks over. Keep it gentle: height up to 0.5 AL, wavelength at least 3 AL. Ring density on the trunk must resolve it (about 8 vertices per ridge). Finer detail is shader normals only. Build the smooth version first.
- **Verify**: lighting on a procedural mesh. It has no distance field, so Lumen software tracing ignores it. The project has hardware ray tracing and virtual shadow maps enabled. Check early with a screenshot that the tree casts and receives shadows and is not flat-lit; fall back to screen traces plus a strong sky light if hardware tracing misbehaves.

### Leaves (`LeafSet`)

```
FLeaf { FVector Base; FQuat Rot; float Length; int32 Twig; float Remaining; uint8 Bites; FVector2f BiteUV[3]; float BiteR[3]; }
```

- Leaves attach by their base to twigs. Thousands of them; all are data, a spatial hash finds the ones near an ant.
- Rendered as quads in procedural mesh sections, chunked per limb so one cut updates one small section.
- Shape, veins and serration come from UV maths in a masked, two-sided foliage material. Backlit glow matters for the look.
- Bites: up to three circles per leaf, passed in spare UV channels and vertex colour, discarded in the shader. Updating a bite rewrites that chunk's vertex data.
- Wind sway by world position offset, driven by a material parameter collection.

## Ant

### Movement (`AntMovement`)

A plain `APawn` with a custom movement update. **No `ACharacter`, no physics, no capsule.**

```
State: Pos, Up, Forward, Velocity (tangent to the surface)

Tick(dt):
  n       = Field.Gradient(Pos)
  wish    = camera-relative input, projected onto the plane with normal n
            (if the camera looks along n, use the camera's up as "forward")
  Velocity= approach(Velocity, wish * Speed, Accel * dt), kept tangent to n
  Pos    += Velocity * dt
  Field.Project(Pos, n, RideHeight)
  Up      = slerp(Up, n, UpRate * dt)
  Forward = slerp toward Velocity direction when moving
```

- Substep when `Speed * dt` exceeds half an ant length, so thin twigs are not skipped.
- Thin branches (radius under about 70 cm, fully below 15): with `bSteeringWheel` (default) W/S walk along the branch and A/D round it, clockwise / anticlockwise as seen from the camera. Which end W means (`WheelAway`) is captured from the camera when the stick is first held and kept until it is released, carried across branch joins by continuity. Without it: camera-relative input with the sideways snap.
- States: `Walking`, `Cutting` (rooted), `Gripping` (rooted), `Riding` (following a trail), `Returning` (unattended walk home: from the nearby scent line point nearest the nest, along that line with its loops cut out; `ATrailNetwork::BuildReturnRoute`), `Falling` (simple ballistic drop to the ground, then `WalkingHome` along the ground to the nest).
- Sprint multiplies speed and drains stamina. Carried mass scales speed down.
- The same component drives the player's ant and unattended ants; only the source of "wish" differs.

### Camera (`AntCamera`)

A separate actor that follows the controlled ant. It is never attached.

- Keeps its own reference up `CamUp`, which eases toward the ant's `Up` with a time constant of about 0.6 s. **Tune.**
- Yaw and pitch orbit around `CamUp`. When `CamUp` changes, keep the view direction as steady as possible and re-orthogonalise.
- The desired position is behind and above the ant at a boom length; the actual position follows with a critically damped spring.
- Wood does not push the camera in (`bAvoidThickOcclusion` restores the old boom shortening). The camera passes through branches; `M_TreeBark` dithers away the bark within `CutawayRadius` of the line from the camera to the ant, narrowing to nothing at the ant, and anything within 35 cm of the camera. The camera publishes its position and look-at point to `MPC_View` (`CamPos`, `AntFocus`, `CutawayOn`, `CutawayRadius`) every frame. Shadows ignore the cutaway (`ShadowReplace`). `M_Leaf` (masked since 12 Nov) does the same for leaves in a Custom HLSL node, at 1.5 times the radius and without the narrowing, but it keeps leaves within 15 to 45 cm of the ant (the leaf being cut is usually there); leaves within 25 to 45 cm of the camera fade too. The camera never goes below `GroundClearance` above the ground.
- Shifting ants is a deliberate one-second glide to the new ant, not a cut.
- Shallow depth of field focused on the ant sells the macro scale. Keep it mild enough that the route ahead stays readable.

### Visual (`AntVisual`)

- Placeholder until the Blender mesh arrives: three engine spheres.
- Final: static mesh from `docs/briefs/ant-mesh.md`. Legs animate in the material from a gait phase and walk speed supplied by C++ through a dynamic material instance.
- A named socket or fixed offset at the mandibles holds the carried fragment, which is a small procedural leaf piece scaled by its size and held overhead.

## Trails (`TrailNetwork`)

- Each worker owns `TArray<FTrailPoint> Path` (`Pos`, `Normal`), sampled every 0.5 AL of travel from the moment it leaves the nest, whether it walked or rode.
- **Returning** is that array played backwards at the ant's laden speed, each point re-projected onto the field.
- The network keeps every path for the day. For drawing, a new path only adds geometry where it is farther than a threshold from existing trail; where it overlaps, it raises that trail's strength.
- **Riding**: find the nearest trail point within reach, then advance along that path away from the nest. At the end of a path, or on any movement input, return to `Walking`. Where paths fork, prefer the one whose direction best matches the camera's forward.
- Rendering: a thin ribbon lifted slightly off the surface along the stored normals, unlit translucent cyan, with a slow flow in the shader. Brightness follows strength. Restrained, as in the concept art.

## Cutting and carrying

- When the controlled ant is within reach of a leaf base, show the prompt.
- Holding the cut button enters `Cutting`. Fragment size grows at a rate set by the Mandibles upgrade, up to the lesser of the size cap and the leaf's remaining area.
- The predicted risk colour comes from the wind formula below, using today's full gust strength and an exposure of 1.
- On release: write the bite into the leaf, spawn the fragment on the ant, set carried mass.
- **Send home** switches the ant to `Returning` and asks the roster for the next worker.
- Reaching the nest while carrying deposits the mass into today's haul.

## Wind (`WindSystem`)

- One prevailing direction per day (horizontal, random per day), with a few degrees of wander.
- Phases: `Calm`, `Warning`, `Rising`, `Holding`, `Falling`. Durations and peak strength scale with the day number.
- Publishes `Strength` (0–1) and `Direction` to a material parameter collection for leaf sway, streaks and the audio mix.
- Exposure at a point: `Facing = saturate(dot(Normal, -WindDir) * 0.5 + 0.5)`, multiplied by `Open`, which is 0.25 if `Field.March` upwind hits the tree within a few metres and 1 otherwise.
- Blow-off test, evaluated a few times per second during `Rising` and `Holding`:

```
Force = Strength * PeakForce(day) * Exposure * (BodyArea + FragmentArea)
Hold  = BaseGrip * GripUpgrade * (Gripping ? GripBonus : 1)
blown off when Force > Hold
```

- Streaks are math-only: ribbons or a Niagara system stretched along the wind direction. They appear in `Warning` so the player has time to react.

## Colony, days and saving

- `LeaflineGameInstance` holds the persistent colony: leaf mass to spend, total hauled, upgrade levels, day number, base seed. `LeaflineSaveGame` writes it to one slot.
- `DayDirector` runs the day as a state machine:

```
Title -> DayStart (grow tree from hash(seed, day), set wind, reset roster)
      -> Shift (player controls worker i; others may be Returning)
      -> AllHome -> Recap -> Nest (upgrade screen) -> DayStart
```

- The roster holds one entry per worker: `Waiting`, `Out`, `Returning`, `Home`, `BlownOff`. "Next worker" spawns at the nest. When none are waiting, the camera follows a returning ant and the cycle key switches between them until all are in.
- Difficulty per day: `LowestLeafyHeight` rises, gust peak and frequency rise.
- Spawn objects the pawn depends on in `InitGame`, since the pawn exists before `BeginPlay`.
- Milestones and the end card read `TotalHauled`.

## Input

Enhanced Input, with `UInputAction` and `UInputMappingContext` objects created in C++ (`NewObject`, then `MapKey`) so there are no input assets. Bindings are in `design.md`.

## UI

- In-game HUD: `AHUD` canvas drawing. Stamina bar, carried load, worker pips, colony meter, context prompt, risk colour while cutting.
- Screens (title, recap, upgrades, end card, pause): start as canvas screens driven by keys and gamepad buttons, which is the fastest route to a complete game. Move them to Slate widgets in the presentation milestone if time allows.
- Engine default font. English only.

## World and lighting

- Sky atmosphere, directional sun, real-time sky light, exponential height fog with volumetric fog for light shafts, virtual shadow maps.
- The sun's elevation is set by the shift number.
- Ground: a large procedural plane with a soil and leaf-litter shader, included in the field so the ant can walk on it. A soft boundary turns the ant back a short distance from the trunk.
- Optional: a ring of distant plain trunks fading into fog to suggest a grove.
- The project has Substrate, Lumen, hardware ray tracing and SM6 enabled from the template. **Verify** early that a packaged build runs on a second machine or at least outside the editor; if hardware ray tracing causes trouble, turn it off rather than debug it.

## Audio

- Sources are synthesised by script (`docs/briefs/audio.md`) and loaded from WAV data at runtime (the editor MCP has no audio import).
- A wind loop whose volume and filter follow `Strength`; footsteps triggered from the gait phase; cut crunch; fragment tear; deposit chime; shift whoosh; an ambient pad; UI ticks.
- Plain audio components and attenuation are enough. No middleware.

### Runtime WAV playback (written by Codex)

`Art/Audio/*.wav` are copied to `Content/Leafline/AudioRaw/` with the extension `.wavraw`, so the editor never auto-imports them, and staged into the pak with `+DirectoriesToAlwaysStageAsUFS=(Path="Leafline/AudioRaw")` in `DefaultGame.ini`. `ULeaflineAudioSubsystem` (a world subsystem) loads and caches 48 kHz 16-bit PCM, mono or stereo, and plays 2D sounds through `USoundWaveProcedural`: `PlayOneShot(Name, Volume, Pitch)` and `StartLoop(Name, Volume)` returning a `ULeaflineAudioLoop` with `SetVolume`, `SetPitch` and `Stop`. Loops refill on underflow without a gap. Test with `Leaf.Sound deposit 1 1.2`. After regenerating the audio, copy the files again.

## Debug tools

Console commands pay for themselves, because verification is done by driving Play In Editor through MCP and reading the log:

| Command | Purpose |
|---|---|
| `Leaf.AutoMove <x>` / `Leaf.AutoStrafe <x>` / `Leaf.AutoTurn <deg/s> [wander]` | Constant input for unattended movement tests; `wander` flips the turn direction at random. While active, a line every 5 s logs distance, max projection error, stalls and branch levels visited |
| `Leaf.Teleport <branch> <t>` / `Leaf.Teleport L<level> [t]` | Put the ant on a given branch, or a random branch of a level (0 trunk, 1 limb, 2 bough, 3 twig, -1 root) |
| `Leaf.FieldBench [n]` | Time n field evaluations near the surface |
| `Leaf.SurfaceGap [maxZ]` | Project points around the trunk and roots (below maxZ, default 1500 cm) onto the walking field and log how far they float above the visible bark: mean, worst and where |
| `Leaf.MeshGap` | Evaluate the walking field at every visible bark mesh vertex: mean, worst and how many float more than 1 or 3 cm (the honest check; `SurfaceGap` compares the field with its own frustum model) |
| `Leaf.GapAt <x> <y> <z>` | At one point: the field distance, and each nearby branch's distance to its visible bark |
| `Leaf.GoLeaf` / `Leaf.Cut [size]` / `Leaf.SendHome [delay]` | Drive the loop from the console: go beside a random cuttable leaf, cut it at once, send the ant home |
| `Leaf.Hold <cut\|grip\|follow> <seconds>` | Hold a button for a while, as if pressed (hold-to-cut, grip, riding) |
| `Leaf.Aim <heading\|up> <pitch>` | Aim the camera toward a world heading (0 = +X) or up the trunk, at a pitch (repeatable screenshots) |
| `Leaf.Delay <seconds> <command...>` | Run a console command later, also while paused (unattended screenshot runs) |
| `Leaf.PauseMenu <0\|1> [row]` | Pause and show the pause menu (0) or the options page (1) with a row selected (screenshots) |
| `Leaf.LightInfo` | Log the sun, sky light, atmosphere, fog, clouds and post-process settings found in the level |
| `Leaf.QuitAfter <seconds>` | Exit a standalone game after a delay, also while paused (unattended runs; ignored in the editor) |
| `Leaf.Tune <cam\|move\|input> [Field] [Value]` | List, read or set a tuning value while playing (shown on screen and logged), e.g. `Leaf.Tune cam SurfaceFollow 0` |
| `Leaf.NewDay [seed]` | Regrow |
| `Leaf.Gust [0-1]` | Trigger a gust (warning, rise, hold, fall) with that peak |
| `Leaf.FaceWind exposed\|sheltered` | Put the ant on the windward or lee side of the trunk (wind tests) |
| `Leaf.Give <mass>` / `Leaf.Buy <index>` | Economy |
| `Leaf.DebugField 0\|1` | Draw normals, projection error, exposure |
| `Leaf.Carry <size>` | Hand the ant a fragment |

Log gameplay events under one category (`LogLeafline`) so pickups, deposits, blow-offs and state changes can be checked from the log.

## Performance budget

- Field: a few thousand curve segments. Each ant needs about 20 evaluations per frame, the camera about 30 per frame. With per-branch bounds this is well under a millisecond for ten ants.
- Tree mesh: up to about 1–2 million triangles with bark relief, built once per day. A build under two seconds is acceptable behind the day-start screen. Generate on a worker thread if it hitches.
- Leaves: 20–40 thousand quads, masked. Watch overdraw.
- Target 60 fps at 1080p on a mid-range GPU.
- Measured 2026-11-12 (`Tools/perf_packaged.sh`, packaged Development build, 1920x1080 windowed, the owner's NVIDIA GPU with ray tracing tier 1.1). Every run sat at the display's 60 Hz, so the GPU time is the real headroom:

  | Run | fps (avg / 1% low) | Game thread | GPU |
  |---|---|---|---|
  | Trunk, D3D12 SM6, ray tracing | 60 / 53 | 1.6 ms | 4.2 ms |
  | Canopy (twigs and leaves), ray tracing | 60 / 52 | 1.7 ms | 4.0 ms |
  | Trunk, `-noraytracing` | 60 / 53 | 1.6 ms | 4.2 ms |
  | Canopy, `-noraytracing` | 60 / 55 | 1.5 ms | 4.2 ms |
  | Trunk, `-dx11` (SM5) | 60 / 54 | 1.4 ms | not comparable |

  After look pass 1 (8645 leaves instead of 5691, 12600 grove crown leaves, stronger fog scattering), same runs: trunk 4.2 ms, canopy 4.6 ms GPU with ray tracing; 4.4 and 4.4 ms without. Still 60 fps, 1% lows 50 to 53. With the leaf material masked for its cutaway: trunk 4.6, canopy 5.0 ms with ray tracing; 4.3 and 4.9 ms without.

  Without hardware ray tracing the game starts and runs the same ("Ray tracing is disabled. Reason: not supported"); the GPU cost does not change, so nothing depends on it. DX11 also starts and runs. Reference shots (`Tools/ref_shots.sh`, last taken after look pass 1 and the material changes) with and without ray tracing look nearly the same, so the look does not depend on it. Whether the picture changes without ray tracing (Lumen falls back to software tracing, and the procedural tree has no mesh distance field) has not been compared yet. A mid-range GPU is perhaps three to five times slower than this one, so 4.2 ms leaves room for 60 fps there, unchecked.

## Risks, in order

1. **Camera feel.** Only the owner can judge it. Get it into their hands on day one.
2. **Lighting on the procedural mesh.** Check with a screenshot in the first hour of tree work.
3. **Field and mesh disagreeing** at junctions or after bark relief. `Leaf.DebugField` exists for this.
4. **Packaging.** Do a packaged build as soon as the loop runs; do not leave it for Sunday.
5. **Disk space.** Drive D: had 57 GB free on 2026-11-12. Cooked builds and the derived data cache are large.
