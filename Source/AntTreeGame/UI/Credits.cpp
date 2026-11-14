#include "UI/Credits.h"
#include "AntTreeGame.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace LeafCredits
{
	namespace
	{
		TArray<FRow> Rows;
		bool bLoaded = false;

		void AddLine(const FString& Raw)
		{
			const FString Line = Raw.TrimEnd();
			if (Line.StartsWith(TEXT(";"))) return;
			if (Line.TrimStart().IsEmpty())
			{
				// One gap per blank line, but none at the very top.
				if (Rows.Num() > 0) Rows.Add({ ERowKind::Gap, FString() });
				return;
			}
			if (Line.StartsWith(TEXT("#")))
			{
				Rows.Add({ ERowKind::Heading, Line.Mid(1).TrimStart() });
				return;
			}
			Rows.Add({ ERowKind::Text, Line.TrimStart() });
		}
	}

	void Reload()
	{
		bLoaded = true;
		Rows.Reset();
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Leafline/UIRaw/credits.txt"));
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *Path))
		{
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines, false);
			for (const FString& L : Lines) AddLine(L);
		}
		else
		{
			UE_LOG(LogLeafline, Warning, TEXT("Credits: %s could not be read; showing the built-in short credits"), *Path);
			AddLine(TEXT("# Leafline"));
			AddLine(TEXT("A game by inventivetalent"));
			AddLine(TEXT(""));
			AddLine(TEXT("Made with Unreal Engine 5.8"));
		}
		while (Rows.Num() > 0 && Rows.Last().Kind == ERowKind::Gap) Rows.Pop();
		int32 Headings = 0;
		for (const FRow& R : Rows) Headings += R.Kind == ERowKind::Heading ? 1 : 0;
		UE_LOG(LogLeafline, Log, TEXT("Credits: %d rows (%d headings) read"), Rows.Num(), Headings);
	}

	const TArray<FRow>& Get()
	{
		if (!bLoaded) Reload();
		return Rows;
	}
}
