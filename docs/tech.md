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
- Vertex data for the bark shader: UV0 = (arc length around, distance along the branch, both in cm), UV1 = (R cos θ, R sin θ) in cm, UV2 = the distance along split as (Hi, Lo), Hi = floor(along / 256) × 256, so both halves are exact in half floats and their sum interpolates linearly (UV0.y alone has 8 to 16 cm steps high on the trunk). UV3 = (bark radius, buried distance). The bark radius is the branch radius without the trunk flare and without branch collars (`FTreeBranch::BarkRadius`): `M_TreeBark2` builds its ridge cylinder from it, because a cylinder of the true radius sweeps radially through the ridge cells wherever the radius changes fast along the branch, and the flare read as paving stones. The buried distance is the signed distance in cm to the overlapping branch (a child's vertices against its parent; a parent's against a child's base, only when that child's open base ring lies wholly inside the parent), unclamped in a half float, plus 0.2 × the radius of the branch it is buried in, 10000 where nothing overlaps; the bark hides UV3.y below -`BuriedDepth` (15 cm, `MPC_View`, `Leaf.Tune cam BuriedDepth`), so about 235 cm by the trunk and about 33 cm inside a 90 cm limb. Seen through the cutaway, that buried bark was the black wedge at junctions (look pass, 13 Nov). Two earlier versions opened holes in the owner's playtest: an 8-bit vertex-alpha encoding clamped to ±25 cm and then ±200 cm (trunk rings are up to 300 cm apart, and a clamped end value pulls the linearly interpolated cut outward on both surfaces), and masking the parent inside children whose base ring pokes out of the parent (big limbs, roots: their open end showed). Even unclamped, the distance interpolated across the 200 to 300 cm triangles of big branches dips far below the true one (the flare is bell-shaped and branches bend, so the solids are not convex): on the owner's tree a root lost visible bark up to about 200 cm "inside" the trunk until the depth reached 200 (a sweep by a Claude agent), hence the depth that scales with the radius. Vertex colour = (branch level, random per branch, height on tree, the old 8-bit buried value, no longer read). The overlap pass costs about 100 ms per tree build (logged with the mesh build).
- Branch collars (`FTreeParams::bBranchCollars`, default on, `Leaf.Collars 0|1` regrows today's tree): every limb, bough and root swells where it leaves its parent, by `CollarAmount` (0.45, boughs 0.8 of it) of its base radius, ramping in over the second half of its buried length (so the base ring stays inside the parent) and decaying over `CollarLength` (1.2) base radii. Applied to the skeleton radii after growth, drawing no random numbers, so the walking surface and the mesh agree and the tree is the same tree. Twigs get none: they carry leaves from 15% of their length, and leaf count and order must not change (one-tree saves store bites by leaf index); checked equal with collars on and off.
- `M_TreeBark2` (look pass, 13 Nov, the default; `Leaf.Bark 0` switches back to `M_TreeBark`, loaded by path so a missing asset falls back). Three Custom HLSL nodes whose code lives in `Tools/materials/bark2_*.hlsl` (the scripts there rebuild or update the material over unreal-mcp; never edit the code only in the editor). Bark space q = (direction round the branch × bark radius (UV3.x), along × 0.085): ridges are Voronoi cells of q, about 14 cm across and twelve times longer along the branch, warped so furrows wander; furrows are their soft borders, darker wood (0.42 of the ridge colour) with no outline, widened with the pixel footprint so they never alias. Relief is the analytic gradient of the same height (normal only). Each ridge has its own tone; mottling, a slow tone change across the tree, fine grain streaks up close (gone by about 2.5 m), roughness from 0.74 (ridges) to 0.95 (furrows), AO in the furrows. Moss in patches in true proportions, a little more in furrows, on top of limbs and low down. Twigs are smoother and warmer. Colour and moss noise use (UV1, along) unstretched. A layer of small ridged-noise cracks was tried and dropped: it drew looping outlines.
- `M_TreeBark` (the fallback) evaluates 3D noise at (UV1, 0.15 × along + 5000 × random): a cylinder around each branch, so there is no seam around, features keep their physical size, tips close to a point, and fissures run along the branch. On the steep root flare the stretch is lost (the radius changes faster than 0.15 per cm), so the flare reads as plates. UVs are half precision, a few cm of step high on the trunk, which the long fissures hide. `M_Bark` is the older UV-based version, kept as a fallback.
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
- **Controls: screen direction everywhere** (`FAntMoveTuning::ControlStyle` 0, the default since 13 Nov, owner's design). The stick (or W/A/S/D) is a direction on screen. When it is first pushed, `MakeScreenBasis` turns "up" and "right" on screen into two surface directions (`HeldFwd`, `HeldRight`); while the stick is held they are carried along the bark with the velocity (rotated by the change of surface normal each substep), so a held stick keeps going straight over the surface and the camera is free meanwhile (it no longer steers). The camera is read again after the stick has been centred for `PauseSeconds` (0.15 s), or when the stick direction jumps by more than `NewInputAngleDeg` (40 degrees) from one frame to the next; gradual stick turns steer within the held directions. **On a branch** (any level, the trunk too) the stick is read against how the whole branch lies on screen, not the patch of bark under the ant: the branch axis is projected to the screen with perspective at the ant (the camera position comes with `SetMoveInput`; away from the camera moves a point toward the screen centre), plus `AxisDepthWeight` (0.25) of "away counts as up" for a branch pointing straight at the camera. Stick along that screen direction walks along the branch; across it turns the branch's side facing the camera that way (round the branch); diagonals go diagonally. On the ground: screen-up (`CamUp + ScreenDepthWeight * CamForward`) and screen-right projected onto the surface. Going round a branch is limited to `MaxLapsPerSecond` (1) laps a second, walking along it is not, so a diagonal on a twig is a gentle spiral. `bSnapOnBranches` (off by default, owner's choice) snaps the held direction to along or around on thin wood (`SnapFullRadius` 25 cm, fading by `SnapFadeRadius` 60 cm). The rounded tip keeps the old latch (stop at the apex, leave when the stick reverses).
- **Old controls** (`ControlStyle` 1, `Leaf.Tune move ControlStyle 1`): camera-relative on the trunk and the ground, and on wood under `WheelFullRadius` (fading by `WheelFadeRadius`) the steering wheel: W/S along the branch, A/D round it; which end W means (`WheelAway`) is picked from the camera at press (or from the ant's facing when the branch runs across the screen) and kept until release. Below about 70 cm a gentle stick (under 0.2) used the even older thin-branch rules. Playtest (13 Nov): A/D circling on a branch that runs across the screen, W flipping after circling a branch, and the switch of meaning at junctions were the reasons for the change.
- **Input view** (`Leaf.InputViz`, F3 while `bInputVizKey` is True in `DefaultGame.ini`): short paths over the bark for where W, A, S and D would go from here (`PreviewWish`, the same rules without changing any state), the live stick (white), the held direction before the twig snap (thin cyan), the actual movement (green), and the rule in charge, the branch radius, the snapshot state and the stick on the left of the screen. Always in the log (player's ant, dev builds): `Control trace: onto branch ...` for every junction crossing with the stick held (radius and rule before and after, how far forward moved on screen), and `Control trace: moving N deg off the stick's screen direction ...` when the ant moves more than 60 degrees away from the stick's screen direction for a third of a second (circling a twig behind it shows here by design).
- States: `Walking`, `Cutting` (rooted), `Gripping` (rooted), `Riding` (following a trail), `Returning` (unattended walk home: from the nearby scent line point nearest the nest, along that line with its loops cut out; `ATrailNetwork::BuildReturnRoute`), `Falling` (simple ballistic drop to the ground, then `WalkingHome` along the ground to the nest).
- Sprint multiplies speed and drains stamina. Carried mass scales speed down.
- The same component drives the player's ant and unattended ants; only the source of "wish" differs.

### Camera (`AntCamera`)

A separate actor that follows the controlled ant. It is never attached.

- Keeps its own reference up `CamUp`, which eases toward the ant's `Up` with a time constant of about 0.6 s. **Tune.**
- Yaw and pitch orbit around `CamUp`. When `CamUp` changes, keep the view direction as steady as possible and re-orthogonalise.
- The desired position is behind and above the ant at a boom length; the actual position follows with a critically damped spring.
- Wood does not push the camera in (`bAvoidThickOcclusion` restores the old boom shortening). The camera passes through branches; the bark dithers away within `CutawayRadius` of the line from the camera to the ant, narrowing to nothing at the ant, and anything near the camera. The camera publishes its position and look-at point to `MPC_View` (`CamPos`, `AntFocus`, `CutawayOn`, `CutawayRadius`, and for `M_TreeBark2` also `CutawayBand` (rim width as a fraction of the radius), `CutawayTaper` (radius × (1 − t)^taper), `NearFadeStart`/`NearFadeEnd` (cm from the camera), `BuriedDepth`) every frame, so the cutaway tunes live with `Leaf.Tune cam`. Faces are one-sided, so from inside a limb the limb itself is not drawn; what showed through was bark buried in the neighbouring branch, now hidden (see the mesh section).
- Look-up assist (`bLookUpAssist`, off until the owner has tried it): when pitching up would put the camera under the ant's own surface, the camera comes in along the view line to `LookUpClearance` above that surface (no nearer than `LookUpMinBoom`) and looks along the pitched view, turned back toward the ant only as far as needed to keep it near the bottom of the frame. Depth of field focuses on the ant's real distance. `ExposureMinEV`/`ExposureMaxEV` fix the auto exposure range when set (both -100 leave the level's settings). Shadows ignore the cutaway (`ShadowReplace`). `M_Leaf` (masked since 12 Nov) does the same for leaves in a Custom HLSL node, at 1.5 times the radius and without the narrowing, but it keeps leaves within 15 to 45 cm of the ant (the leaf being cut is usually there); leaves within 25 to 45 cm of the camera fade too. The camera never goes below `GroundClearance` above the ground, and never sits inside wood thicker than `ThickWoodRadius` (it is projected out to `WoodClearance` above the bark and looks at the ant from there).
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
- **Yesterday's trails** (one tree per stage): at day end `ExportToday` hands the day's paths to the save. The next day `LoadYesterday` adds them as paths flagged `bYesterday`: drawn at `YesterdayGlow` (0.3), 0.4 cm lower than today's ribbon, never brightened by lines, ignored by today's merge test (a fresh trail over a faint one draws at full strength), left out of `DrawnLength`, and ridden and followed home like any other path. They are dropped when that day ends (the save then holds the newer day's routes) and on a move.
- Rendering: a thin ribbon lifted slightly off the surface along the stored normals, unlit translucent cyan. Brightness (vertex alpha, which `M_Trail` multiplies into opacity) shows each line's patch: `ATrailNetwork::SetLineGlow` gives every drawn stretch within `MergeDistance` of a line's route the brightest glow of the lines using it (full patch 1.0, stripped 0.3, no line 0.25), and only the chunks that changed are rebuilt. Faint stretches are a little thinner.

## The line (`LineSystem`)

Spec: `docs/briefs/the-line.md`. `ALineSystem` (spawned by the game mode, initialised with the tree, trails, nest, wind and director) owns lines, the recruit pool and the fixed-step clock.

- **Line**: made by `AddLine` when a scout's piece is on its way home (`ADayDirector::SendHome`, or a scout carrying it into the nest). The route is `BuildReturnRoute` from the scout's cut (`AAntPawn::GetLastCut`, recorded at the cut, not where send-home was pressed), nest first. The patch is `ATreeActor::BuildPatch` around the cut leaf (same twig, then neighbouring twigs of the bough, nearest first along the bark; `FLineTuning::PatchMaxLeaves` 4, `PatchMaxPieces` 10 including the scout's piece), and each patch leaf gets an approach route over the branch graph (`ATreeActor::BuildLeafApproach`). Leaves without an approach are left out.
- **Recruits** are `AAntPawn`s with `MakeRecruit`: never possessed, no trail recording, stepped by the line system (`SetSimDriven`, `SimStep`), spawned per trip and gone into the nest at the end (`DepositAndVanish`). States: `Nest`, `Queued`, `Out` (riding the line plus the approach at `OutSpeed`), `Cutting` (`StartAutoCut`, the template size at the ant's cut rate), `Laden` / `Empty` (route backwards at the laden speed × `HomeSpeedScale`), `Fallen` (blown off: fall, walk home over the ground). Per-leaf reservations keep two recruits from going for the last piece. A recruit not at its leaf `ReachTimeout` s after the end of the line cuts from where it stands (logged as a fallback). A rescue after `RescueAfter` s guarantees every recruit gets home.
- **Fixed steps**: `ALineSystem::Tick` accumulates real time × `SpeedFactor` and runs steps of `FLineTuning::Step` (1/120 s): the wind clock (`AWindSystem::Advance`; the wind's own tick only does sound, materials and streaks), every recruit and, during the harvest, returning scouts handed over with `AddDrivenAnt`. A frame budget (`FrameBudgetMs`) bounds the skip; over budget the backlog is dropped, so results never depend on frame rate or on watching versus skipping. Transforms and leg animation are pushed once per frame (`PushVisuals`).
- **Values**: `FTreeLeaf::Richness` (1× low to about 3× high and far out) multiplies a piece's value (`AAntPawn::GetCarriedValue`); the carried weight that slows the ant stays size-based. Leaf vertex alpha carries `(Richness / RichnessMul - 1) / 2` for `M_Leaf` (the shape-based value, so a stage's richness multiplier does not saturate the look).
- **Read-outs**: `GetLines` (per line: pieces home and lost, patch remaining, fullness, recruits on it), `NumIdle`, `GetPatchPreview` (cached a second), `GetRecentDeliveries` (for the "+N"; the chime pitch rises with `Chain`).

## Cutting and carrying

- When the controlled ant is within reach of a leaf base, show the prompt.
- Holding the cut button enters `Cutting`. Fragment size grows at a rate set by the Mandibles upgrade, up to the lesser of the size cap and the leaf's remaining area.
- The predicted risk colour comes from the wind formula below, using today's full gust strength and an exposure of 1.
- On release: write the bite into the leaf, spawn the fragment on the ant, set carried mass.
- **Send home** switches the ant to `Returning` and asks the roster for the next worker.
- Reaching the nest while carrying deposits the mass into today's haul.

## Wind (`WindSystem`)

- One prevailing direction per day (horizontal, random per day), with a few degrees of wander.
- Phases: `Calm`, `Warning`, `Rising`, `Holding`, `Falling`. Durations and peak strength scale with the day number; with one tree per stage, `StartDay(Day, Seed, ForceScale, RampDay)` takes peak force `PeakForceDay1 × stage GustMul × (1 + GustPerDayOnTree × (days on this tree − 1))` and ramps the gust cycle by days on the tree, seeded from hash(tree seed, day) so the direction changes daily.
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
Title -> DayStart (one tree off: grow tree from hash(seed, day); on: keep the stage's tree, or grow it on a move
                   or after loading; set wind, reset roster and recruit pool) [-> Move card on a new tree]
      -> Shift (player controls scout i; others may be Returning; recruits work lines at 1x)
      -> Harvest (no scout waiting or out, recruits still have work: 5x, skippable)
      -> AllHome -> Recap -> Nest (upgrade screen) -> DayStart
```

- `ADayDirector::UpdateDayFlow` runs every frame in `Shift` and `Harvest`: with no scout waiting or out (and the send-home or fall moment over), it enters the harvest if `ALineSystem::HasWork()`, else ends the day once no scout is still returning. In the harvest the day ends when no recruit has work and no scout is returning. `HasWork` is true while any recruit is queued or out, or one in the nest has trips left and a live line to go to.

- The roster holds one entry per worker: `Waiting`, `Out`, `Returning`, `Home`, `BlownOff`. "Next worker" spawns at the nest. When none are waiting, the camera follows a returning ant and the cycle key switches between them until all are in.
- Difficulty per day (one tree off): `LowestLeafyHeight` rises, gust peak and frequency rise. With one tree per stage, see below.
- Spawn objects the pawn depends on in `InitGame`, since the pawn exists before `BeginPlay`.
- Milestones and the end card read `TotalHauled`, against the line set or the "by hand" set (`ULeaflineGameInstance::AreRecruitsEnabled`).
- **One tree per colony stage** (`FDayTuning::bOneTree`, default on; `Leaf.OneTree`; spec `docs/briefs/one-tree.md`). The save holds the stage, its tree seed, the stage's first day, the tree's leaf count, the bitten leaves (`FSavedLeaf`) and the last day's routes (`FSavedTrail`, with their day). `StartDay` with the flag on: seed 0 means no tree yet (a fresh colony or an older save), so it picks the stage from the milestones and `SeedForStage(stage)`; the tree is grown only when its seed, stage or the game instance's tree generation (bumped by `SetTree` with a new stage or seed, `ResetSave` and every load) differ from the grown one, otherwise it is kept as it is, bites and all (a kept day start takes about 2 ms, a grown one about 400 ms). Shape comes from the stage (`Stage % 4`: the old giant, a spreading giant, a tall spire, a leaning elder), and the stage's `FStageTuning` sets trunk height, lowest leafy height, richness, gusts and the sparse low zone. Yesterday's routes are loaded when they are from the day before. `EndDay` puts the tree's bites and today's routes into the colony together with the day's haul, in one `SaveNow`; a purchase or the end card saves later and carries that snapshot. If the colony's milestones (under the one-tree sets) now call for a later stage, `EndDay` also moves it one stage (`SetTree`: new seed, from the next day, bites and routes cleared); at most one move a day. The next `StartDay` from the nest shows `EDayPhase::Move`, the card "The colony moves to …", with the camera on the whole new tree; at launch on such a day the card follows the title. Quitting mid-day loses that day's bites and haul alike: neither was written.
- Save version 3 adds the tree fields. Versions 1 and 2 never stored their version (it equalled the class default), so `Version` now defaults to 0 and a new colony sets 3 explicitly; a file that loads as 0 is migrated: days, upgrades and haul kept, tree seed 0 (a fresh tree for the stage its milestones give).
- Save version 2 adds `BestDayHaul`; version-1 saves load with their progress (levels clamped to the new range). Unreal's save serialisation skips properties equal to their defaults, so a version-1 file never stored its `Version` and simply loads as current; checked on a copy of the owner's day-7 save. `-LeafSlot=<name>` selects another save slot for automated runs (`Tools/line_test.sh` uses `LineTest`).

## Input

Enhanced Input, with `UInputAction` and `UInputMappingContext` objects created in C++ (`NewObject`, then `MapKey`) so there are no input assets. Bindings are in `design.md`.

## UI

- In-game HUD: `AHUD` canvas drawing. Stamina bar, carried load, worker pips, colony meter, context prompt, risk colour while cutting.
- Screens (title, recap, upgrades, end card, pause): start as canvas screens driven by keys and gamepad buttons, which is the fastest route to a complete game. Move them to Slate widgets in the presentation milestone if time allows.
- Engine default font. English only.

## World and lighting

- Sky atmosphere, directional sun, real-time sky light, exponential height fog with volumetric fog for light shafts, virtual shadow maps.
- The sun's elevation is set by the shift number.
- `FLookTuning` on the day director (`Leaf.Tune look`) holds sun, sky, fog and haze values. Look pass (13 Nov, `docs/reviews/look-shots.md`): sky light 3 with a cool tint (0.75, 0.85, 1.15) so shadowed bark gets a cool fill (Lumen GI still carries most of it), and the camera's exposure bias 0.2 (was -0.4). The dark sky in the first shift's look-up views is the low morning sun (22°) plus auto exposure metering on sunlit bark, not a designed storm; tuning cannot lift it without overexposing the bark. The amber wash on near bark was not haze but the cutaway digging a trench into the bark under a camera 12 cm above it; the fix is `WoodClearance` 60 (at least `CutawayRadius`). Fog values unchanged: no fog change measurably helped.
- Ground: a large procedural plane with a soil and leaf-litter shader, included in the field so the ant can walk on it. A soft boundary turns the ant back a short distance from the trunk.
- Optional: a ring of distant plain trunks fading into fog to suggest a grove.
- The project has Substrate, Lumen, hardware ray tracing and SM6 enabled from the template. **Verify** early that a packaged build runs on a second machine or at least outside the editor; if hardware ray tracing causes trouble, turn it off rather than debug it.

## Audio

- Sources are synthesised by script (`docs/briefs/audio.md`) and loaded from WAV data at runtime (the editor MCP has no audio import).
- A wind loop whose volume and filter follow `Strength`; footsteps triggered from the gait phase; cut crunch; fragment tear; deposit chime; shift whoosh; an ambient pad; UI ticks.
- Plain audio components and attenuation are enough. No middleware.

### Runtime WAV playback (written by Codex)

`Art/Audio/*.wav` are copied to `Content/Leafline/AudioRaw/` with the extension `.wavraw`, so the editor never auto-imports them, and staged into the pak with `+DirectoriesToAlwaysStageAsUFS=(Path="Leafline/AudioRaw")` in `DefaultGame.ini`. `ULeaflineAudioSubsystem` (a world subsystem) loads and caches 48 kHz 16-bit PCM, mono or stereo, and plays 2D sounds through `USoundWaveProcedural`: `PlayOneShot(Name, Volume, Pitch)` and `StartLoop(Name, Volume)` returning a `ULeaflineAudioLoop` with `SetVolume`, `SetPitch` and `Stop`. Loops refill on underflow without a gap. Test with `Leaf.Sound deposit 1 1.2`. After regenerating the audio, copy the files again.

**Button prompt icons** work the same way: 26 PNGs from Xelu's CC0 prompt pack (`docs/sourced-assets.md`), cropped to each key's shape, 64 px tall, in `Content/Leafline/UIRaw/*.pngraw`, staged with `+DirectoriesToAlwaysStageAsUFS=(Path="Leafline/UIRaw")`. `LeafPrompts::Get` (`UI/PromptIcons.*`) loads them with `FImageUtils::ImportBufferAsTexture2D` and caches them. HUD text can hold tokens (`{Cut}`, `{Grip}`, `{Send}`, `{Follow}`, `{Cycle}`, `{Sprint}`, `{Pause}`, `{Confirm}`, `{Back}`, `{Move}`, `{Look}`, `{Choose}`, `{Change}`, and `{orclick}`, which reads " or click" on keyboard and mouse only): `DrawTextCentered` / `DrawTextAt` draw them as icons for the device used last (`ALeaflinePlayerController::IsUsingGamepad`, from `InputKey`), light keyboard keys or Xbox Series buttons for every pad (owner's choice; no PlayStation glyphs). A missing icon falls back to the key's name.

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
| `Leaf.Hold <cut\|grip\|follow\|cycle> <seconds>` | Hold a button for a while, as if pressed (hold-to-cut, grip, riding, the Tab tap and hold) |
| `Leaf.Aim <heading\|up\|trunk> <pitch>` | Aim the camera toward a world heading (0 = +X), up the trunk or toward it, at a pitch (repeatable screenshots) |
| `Leaf.Delay <seconds> <command...>` | Run a console command later, also while paused (unattended screenshot runs) |
| `Leaf.PauseMenu <0\|1> [row]` | Pause and show the pause menu (0) or the options page (1) with a row selected (screenshots) |
| `Leaf.LightInfo` | Log the sun, sky light, atmosphere, fog, clouds and post-process settings found in the level |
| `Leaf.QuitAfter <seconds>` | Exit a standalone game after a delay, also while paused (unattended runs; ignored in the editor) |
| `Leaf.Tune <cam\|move\|input\|look\|day\|line\|wind> [Field] [Value]` | List, read or set a tuning value while playing (shown on screen and logged), e.g. `Leaf.Tune cam SurfaceFollow 0` |
| `Leaf.NewDay [seed]` | Regrow |
| `Leaf.Gust [0-1]` | Trigger a gust (warning, rise, hold, fall) with that peak |
| `Leaf.FaceWind exposed\|sheltered` | Put the ant on the windward or lee side of the trunk (wind tests) |
| `Leaf.Give <mass>` / `Leaf.Buy <index>` | Economy |
| `Leaf.DebugField 0\|1` | Draw normals, projection error, exposure |
| `Leaf.Carry <size>` | Hand the ant a fragment |
| `Leaf.Recruits 0\|1` | The line's off switch: recruits, harvest, Brood row, milestone set (the pool follows from the next day) |
| `Leaf.Pool <n>` | Force the recruit pool from the next day; -1 returns to the Brood upgrade |
| `Leaf.Patch [leaf] [size]` | Log the patch around the leaf in reach (or a given leaf): leaves, pieces at that size, richness |
| `Leaf.Harvest` / `Leaf.SkipHarvest` | End the shifts now and go to the harvest; skip a running harvest |
| `Leaf.PatchSurvey [samples]` | Patch sizes over random leaves at templates 0.3 / 0.6 / 1.0, and approach failures by reason |
| `Leaf.StartDay <day> [seed]` | Start that day now (after `Leaf.ResetSave`, for repeatable tests) |
| `Leaf.AtSim <seconds> <command...>` | Run a console command at that sim time of the day, between fixed steps (repeatable tests: the same day plays the same) |
| `Leaf.Cycle` | The cycle button (Tab): next worker in a shift, next view in the harvest (in play a tap of Tab switches workers on release; holding it shows the wind map) |
| `Leaf.InputViz [0\|1]` | The input view (also F3 in dev builds): where W/A/S/D and the stick would take the ant, the actual movement and the rule in charge; no argument toggles |
| `Leaf.PadIcons [-1\|0\|1]` | Prompt icons follow the device in use (-1, default), or always show keys and mouse (0) or pad buttons (1); for screenshots without a pad |
| `Leaf.Goto <x> <y> <z> [heading] [pitch]` | Put the ant on the surface nearest that point and aim the camera. The HUD's faint bottom-left line (build, tree, seed, day) ends with this command for the view on screen, so a screenshot is enough to reproduce it (with the same save or seed) |
| `Leaf.BuildLine [0\|1]` | Show or hide that line (`bShowBuildLine` under `[/Script/AntTreeGame.LeaflineHUD]` in `DefaultGame.ini`; set False before the submission build) |
| `Leaf.Collars [0\|1]` | Branch collars on (default) or off; grows today's tree again, fresh. No argument toggles |
| `Leaf.Bark [0\|1]` | The look-pass bark `M_TreeBark2` (1, default) or the older `M_TreeBark` (0); no argument toggles |
| `Leaf.Smoke [0\|1]` | Scent trails as a thin haze (`M_TrailSmoke` plus an upright ribbon, the default) or the plain flat ribbon (`M_Trail`); no argument toggles |
| `Leaf.Exposure [0\|1]` | Show or hide the wind map (exposure spots on the bark round the ant, the HUD's bands and colours); no argument toggles |
| `Leaf.Orbit [seconds] [radius cm] [n]` | Hold screen-right and log the turn round the branch every 0.5 s and a summary (laps, half-seconds with under 15 cm of way round); with a radius, first go to the limb or bough point of about that radius on the n-th best match (the tangent-point test) |
| `Leaf.LineInfo` | Log every line (home, lost, patch left, reserved, recruits on it) and the pool |
| `Leaf.TestLines <n> [size] [exposed\|sheltered\|any] [seed]` | Scouts lay lines at once to leaves in the lowest leafy band, each with a real walk over the bark from the nest (tests) |
| `Leaf.AutoPlay <days> [lines] [watch\|skip] [buy] [reload] [report] [quit]` | Unattended days with scripted scouts and random lines (with one tree on, among the lowest leaves left); logs whether every day ends. `reload` reads the save and regrows the tree before each day (the launch path); `report` logs `Leaf.TreeReport` each morning; days without recruits fast-forward the scouts' walk home |
| `Leaf.OneTree 0\|1` | One tree per colony stage on or off, from the next day |
| `Leaf.Move` | Move the colony to the next tree now (today restarts on it, with the move card) |
| `Leaf.Strip <metres>` | Use up every leaf below that height (testing the climb) |
| `Leaf.TreeReport [label]` | Live leaves and pieces left per 10 m band, on the six lowest leafy boughs and the two lowest limbs |
| `Leaf.Stage <sparse\|blend\|drop\|leafy\|height\|rich\|gust> <value>` | Change today's stage tuning and grow its tree again, fresh (tuning sweeps) |
| `Leaf.ReloadColony` | Read the save again and restart the day from it, growing the tree as at launch |

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

  After look pass 1 (8645 leaves instead of 5691, 12600 grove crown leaves, stronger fog scattering), same runs: trunk 4.2 ms, canopy 4.6 ms GPU with ray tracing; 4.4 and 4.4 ms without. Still 60 fps, 1% lows 50 to 53. With the leaf material masked for its cutaway: trunk 4.6, canopy 5.0 ms with ray tracing; 4.3 and 4.9 ms without. After the look pass (13 Nov: `M_TreeBark2`, boom 170, the new sky fill), Epic: trunk 4.2, canopy 4.1 ms with ray tracing; 4.3 and 4.0 ms without; Low (`scalability 0`): 1.5 and 1.3 ms with, 1.6 and 1.2 ms without. Still 60 fps, 1% lows 52 to 54. The new bark costs nothing measurable (the closer camera changes the views slightly). `EXTRA_CMDS` in `perf_packaged.sh` adds commands to every run.

  Without hardware ray tracing the game starts and runs the same ("Ray tracing is disabled. Reason: not supported"); the GPU cost does not change, so nothing depends on it. DX11 also starts and runs. Reference shots (`Tools/ref_shots.sh`, last taken after look pass 1 and the material changes) with and without ray tracing look nearly the same, so the look does not depend on it. Whether the picture changes without ray tracing (Lumen falls back to software tracing, and the procedural tree has no mesh distance field) has not been compared yet. A mid-range GPU is perhaps three to five times slower than this one, so 4.2 ms leaves room for 60 fps there, unchecked.

## Risks, in order

1. **Camera feel.** Only the owner can judge it. Get it into their hands on day one.
2. **Lighting on the procedural mesh.** Check with a screenshot in the first hour of tree work.
3. **Field and mesh disagreeing** at junctions or after bark relief. `Leaf.DebugField` exists for this.
4. **Packaging.** Do a packaged build as soon as the loop runs; do not leave it for Sunday.
5. **Disk space.** Drive D: had 57 GB free on 2026-11-12. Cooked builds and the derived data cache are large.
