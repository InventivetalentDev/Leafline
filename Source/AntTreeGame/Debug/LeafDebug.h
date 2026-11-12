#pragma once

#include "CoreMinimal.h"

/** Debug state set by the Leaf.* console commands. Plain globals so Live Coding never has to add class members. */
namespace LeafDebug
{
	/** Constant forward input (0..1) applied on top of the player's input. */
	extern float AutoMove;
	/** Constant sideways input (-1..1, positive = screen right) applied on top of the player's input. */
	extern float AutoStrafe;
	/** Constant camera yaw rate in degrees per second. */
	extern float AutoTurn;
	/** When true, AutoTurn flips direction at random intervals so the walk covers more of the tree. */
	extern bool bAutoWander;
	/** Draw normals and log projection error. */
	extern bool bDebugField;
	/** Seconds left for buttons held by Leaf.Hold (cut, grip, follow). */
	extern float HoldCut;
	extern float HoldGrip;
	extern float HoldFollow;
}
