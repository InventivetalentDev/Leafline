#pragma once

#include "CoreMinimal.h"

/**
 * Relief of the visible ground beyond the ants' reach (floor pass, 14 Nov, docs/briefs/floor-from-above.md step 4).
 * The walking field's ground stays a plane; only what is drawn far out rises and falls: the ground rings
 * (ATreeActor::BuildGround), the grove trunks' feet (BuildGrove) and the far grass tufts (AForestFloor). One height
 * function for all three, zero inside StartRadius of the trunk axis, a smooth ramp to FullRadius, then broad swells
 * (80 m and wider, each faded out where the rings get too coarse for it) that grow to a few metres, and wider and
 * higher ones far out so the horizon is not a ruled line.
 * Leaf.Relief 0|1 switches it (off by default: it did not show through the fog).
 */
namespace LeafRelief
{
	/** 160 m: beyond the litter (140 m), the tail tufts (154 m) and every logged landing of a blown-off ant (80 m). */
	constexpr float StartRadius = 16000.f;
	/** Full relief from here (300 m). */
	constexpr float FullRadius = 30000.f;

	ANTTREEGAME_API bool IsOn();
	ANTTREEGAME_API void SetOn(bool bOn);
	/** Changes whenever the relief is switched or its frame moves: things built on it compare it to rebuild. */
	ANTTREEGAME_API int32 Stamp();
	/** The trunk axis (world X, Y) and the tree seed, which offsets the pattern. Set when the ground is built. */
	ANTTREEGAME_API void SetFrame(const FVector2D& Centre, int32 Seed);
	/** Height (cm) of the visible ground above the ground plane at world X, Y; 0 when off or inside StartRadius. */
	ANTTREEGAME_API float Height(double X, double Y);
	/** Upward unit normal of the visible ground at world X, Y. */
	ANTTREEGAME_API FVector Normal(double X, double Y);
	/** Steepness (rise per run) at world X, Y. */
	ANTTREEGAME_API float Slope(double X, double Y);
}
