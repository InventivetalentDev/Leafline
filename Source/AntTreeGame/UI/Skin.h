#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Optional images for the HUD's panels and menu buttons (docs/ui-skin.md), drawn as nine slices by ALeaflineHUD. Every
 * kind is optional: a kind without a readable image and valid margins keeps today's flat panel, and with no skin files
 * at all nothing changes. Read from the first folder holding a skin.txt: the folder given to Leaf.ReloadSkin, else
 * Saved/Skin (for trying art in a build), else Content/Leafline/UIRaw (shipped, staged as UFS). Leaf.Skin 0 turns it off.
 */
namespace LeafSkin
{
	enum class EKind : uint8
	{
		Card,           // the big screens: title, recap, nest, move and end cards, pause, options, credits, controls
		Group,          // HUD groups: day and workers, wind, colony counter, line rows, stamina, harvest, banners
		Tag,            // one-line read-outs: prompts, hints, cut labels, the "+N" pops
		ButtonSelected, // the selected or hovered menu row
		ButtonIdle,     // the other menu rows (today they have no background)
		GroupCenter,    // optional: Group panels centred near the screen centre (colony counter, banners); else Group
		Count
	};

	struct FImage
	{
		UTexture2D* Texture = nullptr;
		FIntPoint Size = FIntPoint::ZeroValue;
		// Slice margins in image pixels, and image pixels per 1080p screen pixel (2: painted at twice the 1080p size).
		int32 Left = 0, Top = 0, Right = 0, Bottom = 0;
		float PixelsPerDesign = 2.f;
		// skin.txt "mirror_right <image name>": panels of this kind centred right of the screen centre are drawn flipped
		// left to right (a one-sided ornament then points the other way on that side).
		bool bMirrorRight = false;
		// skin.txt "outset <image name> <px>" or "outset <image name> <left> <top> <right> <bottom>": the image is drawn that
		// many 1080p pixels further outside the panel (beyond the panel's padding), so a frame clears the text. Left and
		// right swap with the image when it is mirrored.
		float OutsetL = 0.f, OutsetT = 0.f, OutsetR = 0.f, OutsetB = 0.f;
		// skin.txt "opacity <image name> <0..1>": the image is drawn that opaque (the flat panels were 0.72), so the world
		// shows through; text on top is unaffected.
		float Opacity = 1.f;
	};

	/** The image for a kind, or null for a flat panel (no image for it, it failed to load, or Leaf.Skin 0). */
	const FImage* Get(EKind Kind);

	/** Read the skin again; an empty folder means the default search. Logs one line saying what each kind uses. */
	void Reload(const FString& Folder = FString());
}
