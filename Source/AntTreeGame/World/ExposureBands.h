#pragma once

#include "CoreMinimal.h"

/**
 * The exposure bands shared by the HUD ("Here: sheltered / partly exposed / exposed"), the wind map's colours and the
 * bird (which only sees ants in the exposed band). Exposure is AWindSystem::GetExposure, 0..1.
 */
namespace LeafExposure
{
	/** Below this: sheltered (green). */
	constexpr float ShelteredBelow = 0.25f;
	/** Below this: partly exposed (amber); from here up: exposed (red), in the bird's view. */
	constexpr float PartlyBelow = 0.6f;

	inline bool IsExposed(float Exposure) { return Exposure >= PartlyBelow; }
	/** 0 sheltered, 1 partly exposed, 2 exposed. */
	inline int32 Band(float Exposure) { return Exposure < ShelteredBelow ? 0 : (Exposure < PartlyBelow ? 1 : 2); }
}
