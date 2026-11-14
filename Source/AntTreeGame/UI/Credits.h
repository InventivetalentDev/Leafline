#pragma once

#include "CoreMinimal.h"

/**
 * The credits text, read from Content/Leafline/UIRaw/credits.txt (staged as UFS with the folder, like buildinfo.txt) so
 * names can change without code. Format, one line per row: a line starting with '#' is a heading (accent colour), a
 * blank line is a gap, a line starting with ';' is a note for the file's editor and is not shown, anything else is a
 * row of text (long rows wrap). UTF-8, with or without BOM, LF or CRLF.
 */
namespace LeafCredits
{
	enum class ERowKind : uint8
	{
		Text,
		Heading,
		Gap,
	};

	struct FRow
	{
		ERowKind Kind = ERowKind::Text;
		FString Text;
	};

	/** The rows as last read (Reload on first use). */
	const TArray<FRow>& Get();

	/** Read the file again (the credits page calls this each time it opens). A missing file gives a short built-in text. */
	void Reload();
}
