#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Button prompt icons: Xelu's free controller and key prompts (CC0, docs/sourced-assets.md), light keyboard keys and
 * Xbox Series buttons for every pad. Loaded at runtime from Content/Leafline/UIRaw/<Name>.pngraw (staged as UFS like
 * the audio; the extension keeps the editor from importing them as assets).
 */
namespace LeafPrompts
{
	/** Icon by file name without extension ("Key_E", "Pad_A"); null if the file is missing. Cached for the session. */
	UTexture2D* Get(const FString& Name);

	/**
	 * The icons for a prompt token ("Cut", "Grip", ...) on keyboard and mouse or on a pad, and the text to show if an
	 * icon cannot be loaded. Returns false for an unknown token.
	 */
	bool Expand(const FString& Token, bool bPad, TArray<FString>& OutIcons, FString& OutFallback);
}
