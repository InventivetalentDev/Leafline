# Brief: wind exposure overlay ("wind map")

For one agent (Opus). Owner's playtest note: "the wind risk is really hard to judge from just the UI, maybe some kind of
overlay vision while holding Tab that colors in green-red how exposed an area is?"

## What it is

While the player holds Tab (the main session does the input: tap Tab still switches workers, holding shows the map),
the bark around the controlled ant is covered in small coloured spots, like paint dots, showing how exposed each spot is
to today's wind: green sheltered, amber partly exposed, red exposed. Leaves get no spots (ants walk on bark only).

**It must never disagree with the HUD read-out "Here: sheltered / partly exposed / exposed".** So the colour of a spot is
`AWindSystem::GetExposure(P, N)` at that spot, with the HUD's bands and colours (`UI/LeaflineHUD.cpp` around line 289):
- exposure < 0.25: sheltered, `FLinearColor(0.6, 0.95, 0.5)`
- exposure < 0.6: partly exposed, `FLinearColor(1.0, 0.8, 0.35)`
- otherwise exposed, `FLinearColor(0.85, 0.35, 0.25)`
`GetExposure` is facing (normal against the wind direction) times a shelter test (a march through the tree's distance
field upwind, `FTreeField::March`), so it cannot be done in a shader; it is sampled on the CPU near the ant.

## Files (yours only)

- New: `Source/AntTreeGame/World/ExposureOverlay.h`, `Source/AntTreeGame/World/ExposureOverlay.cpp`.
- New console command `Leaf.Exposure [0|1]` (toggle with no argument) registered in `ExposureOverlay.cpp`, as a named
  function plus `FAutoConsoleCommandWithWorldAndArgs`, like `Debug/TreeDebugCommands.cpp`.
- Read anything else; edit nothing else. The main session wires the input, the HUD legend and the material.

## Design

`AExposureOverlay : AActor`, with a `UProceduralMeshComponent` (module already in `Build.cs`; see `Colony/TrailNetwork.*`
for the pattern), no collision, no shadows.
- `static AExposureOverlay* Get(UWorld* World)`: finds the one in the world or spawns it.
- `void SetShown(bool bShow)`, `bool IsShown() const`. Hidden: no work at all, mesh hidden.
- A tuning struct `FExposureOverlayTuning` (`USTRUCT(BlueprintType)`, `UPROPERTY(EditAnywhere, BlueprintReadWrite)`
  members) as a `Tuning` member: `Radius` (700 cm around the ant), `Spacing` (30 cm between spots), `SpotRadius` (6 cm),
  `Lift` (1.5 cm off the bark), `SamplesPerFrame` (exposure evaluations per tick, start at 40, tune for under 1.5 ms),
  `RecentreDistance` (150 cm: when the ant has moved this far, sample the new area; keep spots that are still inside).
- Centre: the player's pawn if it is an `AAntPawn` (`GetAntMovement()->GetSurfacePosition()`); nothing to do without one.
- Spot positions: for each branch of `ATreeActor::Get(this)->GetSkeleton().Branches` whose `Bounds` reach the sphere
  (trunk, limbs, boughs, twigs and roots; level -1 is roots), walk its samples (`Samples`, `SampleRadius`,
  `SampleDist`) every `Spacing` cm of arc length; at each, a ring of points round the branch spaced about `Spacing` apart
  (at least 6, at most 64). Put each on the walkable surface with `GetField().Project(P, N, 0.0)` (see `TreeField.h`;
  drop points that do not converge). Drop points outside `Radius`, and points closer than `0.6 * Spacing` to a spot
  already kept (a spatial hash), which removes doubles where branches blend into each other and the trunk.
  Twigs thinner than about 3 cm radius may be skipped if they only add clutter.
- Exposure: evaluate nearest spots first, `SamplesPerFrame` per tick, so the map fills outward from the ant in well under
  a second. Then keep refreshing the oldest spots round-robin within the same budget, because the wind direction wanders
  slowly during the day (`AWindSystem::GetDirection`). Spots with no value yet are not drawn.
- Mesh: one section; each spot a small disc (a 6 or 8 triangle fan) in the tangent plane at `P + N * Lift`, vertex colour
  the band colour with alpha 0.85. Rebuild the section at most a few times a second, not every tick. Expect up to a few
  thousand spots.
- Material: `UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> SpotMaterial`, null by default. If null at
  BeginPlay, load `/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial` as a fallback (it shows vertex
  colour). The main session will make an unlit `M_Exposure` later and set it.
- Fade in and out over about 0.15 s if it is easy (scale the alpha), otherwise just show and hide.
- Log (`LogLeafline`): when shown, once the first fill completes:
  `Exposure map: N spots within R m (S sheltered, P partly, E exposed), positions in X ms, filled in Y s`; when hidden,
  one short line. Also log a warning if one tick's exposure work takes more than 3 ms.

## Rules

- Do not build, launch Unreal or run `Scripts/*.bat`; the main session builds. Write code that compiles with UE 5.8.
- Do not commit. Report what you wrote, the public API, and anything you were unsure of.
- Keep to `CLAUDE.md` (clean room, math only, one log category `LogLeafline`).
- Do not use `TActorIterator` per tick; cache the tree and wind pointers (they live for the whole session; the tree is
  regrown in place on a new tree, so re-sample when `ATreeActor` reports a different leaf count or on `SetShown(true)`).
