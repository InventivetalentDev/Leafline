#include "UI/Skin.h"
#include "AntTreeGame.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace LeafSkin
{
	namespace
	{
		constexpr int32 NumKinds = int32(EKind::Count);
		const TCHAR* const Names[NumKinds] = { TEXT("Panel_Card"), TEXT("Panel_Group"), TEXT("Panel_Tag"), TEXT("Button_Selected"), TEXT("Button_Idle"), TEXT("Panel_GroupCenter") };
		FImage Images[NumKinds];
		bool bValid[NumKinds] = {};
		bool bLoaded = false;
		bool bEnabled = true;
		FString OverrideFolder; // from Leaf.ReloadSkin <folder>

		void Clear()
		{
			for (int32 i = 0; i < NumKinds; ++i)
			{
				if (Images[i].Texture) Images[i].Texture->RemoveFromRoot();
				Images[i] = FImage();
				bValid[i] = false;
			}
		}

		bool IsPng(const TArray<uint8>& Bytes)
		{
			static const uint8 Sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
			return Bytes.Num() > 8 && FMemory::Memcmp(Bytes.GetData(), Sig, 8) == 0;
		}

		/** Name.pngraw (as in the repo) or Name.png (as painted). Empty reason = loaded. */
		UTexture2D* LoadImage(const FString& Folder, const FString& Name, FString& OutReason)
		{
			for (const TCHAR* Ext : { TEXT(".pngraw"), TEXT(".png") })
			{
				TArray<uint8> Bytes;
				if (!FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Folder, Name + Ext), FILEREAD_Silent)) continue;
				UTexture2D* Tex = IsPng(Bytes) ? FImageUtils::ImportBufferAsTexture2D(Bytes) : nullptr;
				if (!Tex) OutReason = FString::Printf(TEXT("%s%s is not a readable PNG"), *Name, Ext);
				return Tex;
			}
			OutReason = TEXT("no image");
			return nullptr;
		}
	}

	void Reload(const FString& Folder)
	{
		bLoaded = true;
		Clear();
		OverrideFolder = Folder.IsEmpty() ? FString() : (FPaths::IsRelative(Folder) ? FPaths::Combine(FPaths::ProjectDir(), Folder) : Folder);
		TArray<FString> Search;
		if (!OverrideFolder.IsEmpty()) Search.Add(OverrideFolder);
		else
		{
			Search.Add(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Skin")));
			Search.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Leafline/UIRaw")));
		}
		FString Dir, Text;
		for (const FString& S : Search)
		{
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(S, TEXT("skin.txt"))))
			{
				Dir = S;
				break;
			}
		}
		if (Dir.IsEmpty())
		{
			UE_LOG(LogLeafline, Log, TEXT("Skin: none (no skin.txt in %s): flat panels"), *FString::Join(Search, TEXT(" or ")));
			return;
		}

		// skin.txt: "scale <image pixels per 1080p pixel>", "<image name> <left> <top> <right> <bottom>" (image pixels) and,
		// optionally, "mirror_right <image name>" and "outset <image name> <1080p pixels>".
		float PixelsPerDesign = 2.f;
		int32 Margins[NumKinds][4];
		bool bMirror[NumKinds] = {};
		float Outset[NumKinds] = {};
		FString Reason[NumKinds];
		for (int32 i = 0; i < NumKinds; ++i) Reason[i] = TEXT("no margins line in skin.txt");
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Raw : Lines)
		{
			const FString Line = Raw.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT(";"))) continue;
			TArray<FString> T;
			Line.ParseIntoArrayWS(T);
			if (T[0].Equals(TEXT("scale"), ESearchCase::IgnoreCase))
			{
				const float V = T.Num() == 2 && FCString::IsNumeric(*T[1]) ? FCString::Atof(*T[1]) : 0.f;
				if (V >= 0.25f && V <= 8.f) PixelsPerDesign = V;
				continue;
			}
			if (T[0].Equals(TEXT("mirror_right"), ESearchCase::IgnoreCase))
			{
				for (int32 i = 0; i < NumKinds; ++i)
				{
					if (T.Num() == 2 && T[1].Equals(Names[i], ESearchCase::IgnoreCase)) bMirror[i] = true;
				}
				continue;
			}
			if (T[0].Equals(TEXT("outset"), ESearchCase::IgnoreCase))
			{
				const float V = T.Num() == 3 && FCString::IsNumeric(*T[2]) ? FCString::Atof(*T[2]) : 0.f;
				for (int32 i = 0; i < NumKinds; ++i)
				{
					if (T.Num() == 3 && T[1].Equals(Names[i], ESearchCase::IgnoreCase)) Outset[i] = FMath::Clamp(V, -16.f, 64.f);
				}
				continue;
			}
			for (int32 i = 0; i < NumKinds; ++i)
			{
				if (!T[0].Equals(Names[i], ESearchCase::IgnoreCase)) continue;
				bool bOk = T.Num() == 5;
				for (int32 k = 0; bOk && k < 4; ++k)
				{
					bOk = FCString::IsNumeric(*T[k + 1]) && !T[k + 1].Contains(TEXT(".")) && !T[k + 1].StartsWith(TEXT("-"));
					if (bOk) Margins[i][k] = FCString::Atoi(*T[k + 1]);
				}
				Reason[i] = bOk ? FString() : TEXT("margins do not parse");
			}
		}

		TArray<FString> Parts;
		bool bProblem = false;
		for (int32 i = 0; i < NumKinds; ++i)
		{
			FString Why = Reason[i];
			UTexture2D* Tex = nullptr;
			FString ImageWhy;
			if (Why.IsEmpty() || Why.StartsWith(TEXT("no margins"))) Tex = LoadImage(Dir, Names[i], ImageWhy);
			if (!Tex && !ImageWhy.IsEmpty() && (Why.IsEmpty() || ImageWhy != TEXT("no image"))) Why = ImageWhy;
			if (Tex && Why.IsEmpty())
			{
				const int32 W = Tex->GetSizeX(), H = Tex->GetSizeY();
				if (Margins[i][0] + Margins[i][2] >= W || Margins[i][1] + Margins[i][3] >= H)
				{
					Why = FString::Printf(TEXT("margins %d %d %d %d do not fit the %dx%d image"), Margins[i][0], Margins[i][1], Margins[i][2], Margins[i][3], W, H);
				}
			}
			if (!Tex || !Why.IsEmpty())
			{
				// A margins line without an image, or an image without one, is only a missing kind; anything else is a mistake.
				bProblem |= Why != TEXT("no image") && !(Why.StartsWith(TEXT("no margins")) && !Tex);
				Parts.Add(FString::Printf(TEXT("%s flat (%s)"), Names[i], *Why));
				continue;
			}
			Tex->Filter = TF_Bilinear;
			Tex->AddressX = TA_Clamp;
			Tex->AddressY = TA_Clamp;
			Tex->UpdateResource();
			Tex->AddToRoot();
			FImage& Img = Images[i];
			Img.Texture = Tex;
			Img.Size = FIntPoint(Tex->GetSizeX(), Tex->GetSizeY());
			Img.Left = Margins[i][0];
			Img.Top = Margins[i][1];
			Img.Right = Margins[i][2];
			Img.Bottom = Margins[i][3];
			Img.PixelsPerDesign = PixelsPerDesign;
			Img.bMirrorRight = bMirror[i];
			Img.Outset = Outset[i];
			bValid[i] = true;
			Parts.Add(FString::Printf(TEXT("%s %dx%d (%d %d %d %d)%s%s"), Names[i], Img.Size.X, Img.Size.Y, Img.Left, Img.Top, Img.Right, Img.Bottom,
				Img.bMirrorRight ? TEXT(", mirrored right of centre") : TEXT(""),
				Img.Outset != 0.f ? *FString::Printf(TEXT(", outset %.0f"), Img.Outset) : TEXT("")));
		}
		const FString Summary = FString::Printf(TEXT("Skin from %s, scale %.2f%s: %s"), *Dir, PixelsPerDesign, bEnabled ? TEXT("") : TEXT(" (off: Leaf.Skin 0)"),
			*FString::Join(Parts, TEXT("; ")));
		if (bProblem)
		{
			UE_LOG(LogLeafline, Warning, TEXT("%s"), *Summary);
		}
		else
		{
			UE_LOG(LogLeafline, Log, TEXT("%s"), *Summary);
		}
	}

	const FImage* Get(EKind Kind)
	{
		if (!bLoaded) Reload();
		const int32 i = int32(Kind);
		return bEnabled && i >= 0 && i < NumKinds && bValid[i] ? &Images[i] : nullptr;
	}

	namespace
	{
		FAutoConsoleCommand CmdReloadSkin(
			TEXT("Leaf.ReloadSkin"),
			TEXT("Leaf.ReloadSkin [folder]: read the UI skin again (docs/ui-skin.md); a folder (absolute, or relative to the project) is used instead of Saved/Skin and UIRaw until the next reload without one."),
			FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				Reload(Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FString());
			}));

		FAutoConsoleCommand CmdSkin(
			TEXT("Leaf.Skin"),
			TEXT("Leaf.Skin [0|1]: draw the UI skin's images (1, default) or today's flat panels (0); no argument toggles."),
			FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				bEnabled = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !bEnabled;
				UE_LOG(LogLeafline, Log, TEXT("Leaf.Skin %d"), bEnabled ? 1 : 0);
			}));
	}
}
