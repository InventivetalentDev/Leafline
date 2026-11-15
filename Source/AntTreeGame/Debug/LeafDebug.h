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
	extern float HoldCycle;
	/** Prompt icons: -1 follow the device in use, 0 always keyboard and mouse, 1 always pad (Leaf.PadIcons). */
	extern int32 ForcePadIcons;
	/** Input view: where each direction would take the ant, the rule in charge, and the control trace in the log (Leaf.InputViz, F3). */
	extern bool bInputViz;
	/** What the input view shows this frame, filled by the player's ant and drawn by the HUD. */
	struct FInputVizLabel
	{
		FVector Pos = FVector::ZeroVector;
		FString Text;
		FColor Color = FColor::White;
	};
	extern TArray<FString> InputVizLines;
	extern FColor InputVizColor;
	extern TArray<FInputVizLabel> InputVizLabels;
	/** Leaf.ScreenDirLog: log this many frames of W/A/S/D screen angles (both screen-direction mappings) from the player's ant. */
	extern int32 ScreenDirLogs;
}
