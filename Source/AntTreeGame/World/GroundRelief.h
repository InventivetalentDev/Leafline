#pragma once

#include "CoreMinimal.h"

/**
 * Relief of the visible ground beyond the ants' reach (floor pass, 14 Nov, docs/briefs/floor-from-above.md step 4).
 * The walking field's ground stays a plane; only what is drawn far out rises and falls: the ground rings
 * (ATreeActor::BuildGround), the grove trunks' feet (BuildGrove) and the forest floor's scatter (AForestFloor). One height
 * function for all of them (and the litter and twigs), zero inside the start radius, a smooth ramp, then broad swells
 * (80 m and wider, each faded out where the rings get too coarse for it) that grow to a few metres, and wider and
 * higher ones far out so the horizon is not a ruled line.
 * Leaf.Relief 0|1 switches it (off by default: it did not show through the fog).
 */
namespace LeafRelief
{
	/** Default start (cm from the trunk axis): 90 m, 10 m beyond the farthest of 4,027 logged landings of a blown-off ant
	 *  (80.0 m; the walking boundary turns ants back from 70 m, fully at 78 m). The litter, twigs and grass sit on it. */
	constexpr float DefaultStartRadius = 9000.f;
	/** The ramp from the start to full relief (cm). */
	constexpr float RampLength = 13000.f;

	ANTTREEGAME_API bool IsOn();
	ANTTREEGAME_API void SetOn(bool bOn);
	/** Where the relief starts (cm from the trunk axis) and factors on the heights of the near bands (80 and 240 m) and the
	 *  far bands (900 m and 3 km): Leaf.Relief 1 <start m> <near> <far>. */
	ANTTREEGAME_API void SetShape(float StartRadius, float NearScale, float FarScale, float RimHeight, float OuterRadius);
	ANTTREEGAME_API float GetStartRadius();
	ANTTREEGAME_API float GetNearScale();
	ANTTREEGAME_API float GetFarScale();
	/** A rise toward the edge of the visible ground (cm at the edge, uneven), so its outline covers the sky's own horizon. */
	ANTTREEGAME_API float GetRimHeight();
	/** How far the ground rings reach while the relief is on (cm; 0 keeps FTreeParams::GroundRadius). */
	ANTTREEGAME_API float GetOuterRadius();
	/** Changes whenever the relief is switched or its frame moves: things built on it compare it to rebuild. */
	ANTTREEGAME_API int32 Stamp();
	/** The trunk axis (world X, Y), the tree seed (offsets the pattern) and the crown's reach (cm from the axis: the
	 *  farthest branch sample). Set when the ground is built. */
	ANTTREEGAME_API void SetFrame(const FVector2D& Centre, int32 Seed, float CrownReach = 0.f);
	/** Where the relief really starts: the start radius, or the crown's reach plus a blown-off ant's drift (16.6 m at most
	 *  in still air: at most 5.8 m/s sideways, drag 0.35/s) plus 5.4 m, whichever is farther. */
	ANTTREEGAME_API float GetEffectiveStart();
	/** Height (cm) of the visible ground above the ground plane at world X, Y; 0 when off or inside the start radius. */
	ANTTREEGAME_API float Height(double X, double Y);
	/** Upward unit normal of the visible ground at world X, Y. */
	ANTTREEGAME_API FVector Normal(double X, double Y);
	/** Steepness (rise per run) at world X, Y. */
	ANTTREEGAME_API float Slope(double X, double Y);
}
