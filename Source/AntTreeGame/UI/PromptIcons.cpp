#include "UI/PromptIcons.h"
#include "AntTreeGame.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace LeafPrompts
{
	UTexture2D* Get(const FString& Name)
	{
		static TMap<FString, UTexture2D*> Cache;
		if (UTexture2D** Found = Cache.Find(Name)) return *Found;
		UTexture2D* Tex = nullptr;
		TArray<uint8> Bytes;
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Leafline/UIRaw"), Name + TEXT(".pngraw"));
		if (FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			Tex = FImageUtils::ImportBufferAsTexture2D(Bytes);
		}
		if (Tex)
		{
			Tex->Filter = TF_Bilinear;
			Tex->UpdateResource();
			Tex->AddToRoot(); // shared by every HUD for the whole session
		}
		else
		{
			UE_LOG(LogLeafline, Warning, TEXT("Prompt icon %s could not be loaded from %s; the prompt shows text instead"), *Name, *Path);
		}
		Cache.Add(Name, Tex);
		return Tex;
	}

	bool Expand(const FString& Token, bool bPad, TArray<FString>& OutIcons, FString& OutFallback)
	{
		struct FEntry
		{
			const TCHAR* Token;
			TArray<const TCHAR*> Keys;
			TArray<const TCHAR*> Pad;
			const TCHAR* KeysText;
			const TCHAR* PadText;
		};
		// The bindings are in ALeaflinePlayerController::CreateInput. Menus confirm with cut or grip and go back with
		// send home; pads show Xbox-style buttons whatever the pad (owner, 13 Nov).
		static const TArray<FEntry> Entries = {
			{ TEXT("Cut"), { TEXT("Key_E"), TEXT("Key_Mouse_Left") }, { TEXT("Pad_X") }, TEXT("E / LMB"), TEXT("X") },
			{ TEXT("Grip"), { TEXT("Key_Space") }, { TEXT("Pad_A") }, TEXT("Space"), TEXT("A") },
			{ TEXT("Send"), { TEXT("Key_Q") }, { TEXT("Pad_Y") }, TEXT("Q"), TEXT("Y") },
			{ TEXT("Follow"), { TEXT("Key_F") }, { TEXT("Pad_RB") }, TEXT("F"), TEXT("RB") },
			{ TEXT("Cycle"), { TEXT("Key_Tab") }, { TEXT("Pad_LB") }, TEXT("Tab"), TEXT("LB") },
			{ TEXT("Sprint"), { TEXT("Key_Shift") }, { TEXT("Pad_LT") }, TEXT("Shift"), TEXT("LT") },
			{ TEXT("Pause"), { TEXT("Key_Esc") }, { TEXT("Pad_Menu") }, TEXT("Esc"), TEXT("Start") },
			{ TEXT("Confirm"), { TEXT("Key_E"), TEXT("Key_Space") }, { TEXT("Pad_A") }, TEXT("E / Space"), TEXT("A") },
			{ TEXT("Back"), { TEXT("Key_Q") }, { TEXT("Pad_Y") }, TEXT("Q"), TEXT("Y") },
			{ TEXT("Move"), { TEXT("Key_W"), TEXT("Key_A"), TEXT("Key_S"), TEXT("Key_D") }, { TEXT("Pad_Left_Stick") }, TEXT("WASD"), TEXT("left stick") },
			{ TEXT("Look"), { TEXT("Key_Mouse_Simple") }, { TEXT("Pad_Right_Stick") }, TEXT("mouse"), TEXT("right stick") },
			{ TEXT("Choose"), { TEXT("Key_W"), TEXT("Key_S") }, { TEXT("Pad_Left_Stick") }, TEXT("W / S"), TEXT("stick") },
			{ TEXT("Change"), { TEXT("Key_A"), TEXT("Key_D") }, { TEXT("Pad_Left_Stick") }, TEXT("A / D"), TEXT("stick") },
		};
		for (const FEntry& E : Entries)
		{
			if (!Token.Equals(E.Token, ESearchCase::IgnoreCase)) continue;
			OutIcons.Reset();
			for (const TCHAR* N : (bPad ? E.Pad : E.Keys)) OutIcons.Add(N);
			OutFallback = bPad ? E.PadText : E.KeysText;
			return true;
		}
		return false;
	}
}
