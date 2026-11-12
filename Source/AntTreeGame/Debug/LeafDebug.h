#pragma once

#include "CoreMinimal.h"

/** Debug state set by the Leaf.* console commands. Plain globals so Live Coding never has to add class members. */
namespace LeafDebug
{
	/** Constant forward input (0..1) applied on top of the player's input. */
	extern float AutoMove;
	/** Constant camera yaw rate in degrees per second. */
	extern float AutoTurn;
	/** When true, AutoTurn flips direction at random intervals so the walk covers more of the tree. */
	extern bool bAutoWander;
	/** Draw normals and log projection error. */
	extern bool bDebugField;
}
