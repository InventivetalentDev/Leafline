#include "Tree/LeafSet.h"
#include "AntTreeGame.h"
#include "Tree/TreeField.h"
#include "Tree/TreeSkeleton.h"
#include "Tree/TreeMeshBuilder.h"

namespace
{
	constexpr int32 LeafSegments = 7;
}

float FTreeLeaf::HalfWidthAt(float Along) const
{
	const float U = FMath::Clamp(Along / FMath::Max(Length, 1.f), 0.f, 1.f);
	return 0.5f * Width * FMath::Sin(PI * FMath::Pow(U, 0.75f)) * (1.f - 0.15f * U);
}

void FLeafSet::Reset()
{
	Leaves.Reset();
	ChunkLeaves.Reset();
	Grid.Reset();
	BranchLeaves.Reset();
}

void FLeafSet::AddDecorLeaf(const FTreeLeaf& Leaf)
{
	if (ChunkLeaves.Num() <= Leaf.Chunk) ChunkLeaves.SetNum(Leaf.Chunk + 1);
	FTreeLeaf Decor = Leaf;
	Decor.Richness = 1.f; // vertex alpha 0: never shown as a rich leaf
	ChunkLeaves[Leaf.Chunk].Add(Leaves.Add(Decor));
}

FIntVector FLeafSet::Cell(const FVector& P) const
{
	return FIntVector(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize), FMath::FloorToInt(P.Z / CellSize));
}

void FLeafSet::Build(const FTreeSkeleton& S, const FTreeField& Field)
{
	const double StartTime = FPlatformTime::Seconds();
	Reset();
	const FTreeParams& P = S.Params;
	FRandomStream R(S.Seed * 7919 + 13);
	TMap<int32, int32> LimbToChunk;

	for (int32 b = 0; b < S.Branches.Num(); ++b)
	{
		const FTreeBranch& B = S.Branches[b];
		if (!B.bLeafy || B.Samples.Num() < 2) continue;

		int32 A = b;
		while (S.Branches[A].Level > 1 && S.Branches[A].Parent != INDEX_NONE) A = S.Branches[A].Parent;
		int32 Chunk;
		if (const int32* Found = LimbToChunk.Find(A)) Chunk = *Found;
		else Chunk = LimbToChunk.Add(A, ChunkLeaves.AddDefaulted());

		const float Len = B.Length();
		const float Spacing = B.Level >= 3 ? P.LeafSpacingTwig : P.LeafSpacingBough;
		float Az = R.FRandRange(0.f, 2.f * PI);
		for (float D = Len * (B.Level >= 3 ? 0.15f : 0.5f); D <= Len - 5.f; D += Spacing * R.FRandRange(0.8f, 1.2f))
		{
			Az += 2.39996f;
			float Rad;
			FVector Tan;
			const FVector C = TreeSkeleton::SampleAt(B, D / Len, &Rad, &Tan);
			const FVector Ref = FMath::Abs(Tan.Z) < 0.95 ? FVector::UpVector : FVector::ForwardVector;
			const FVector U1 = (Ref - Tan * FVector::DotProduct(Ref, Tan)).GetSafeNormal();
			const FVector U2 = FVector::CrossProduct(Tan, U1);
			const FVector Perp = U1 * FMath::Cos(Az) + U2 * FMath::Sin(Az);

			FTreeLeaf L;
			L.Branch = b;
			L.Chunk = Chunk;
			L.BranchS = D;
			L.Base = C + Perp * Rad;
			L.Length = R.FRandRange(P.LeafLengthMin, P.LeafLengthMax);
			L.Width = L.Length * P.LeafWidthRatio * R.FRandRange(0.9f, 1.1f);
			FVector Dir = Perp * 0.8 + Tan * 0.5 + FVector::UpVector * 0.1 + R.VRand() * 0.15;
			Dir.Z -= 0.15;
			Dir.Normalize();
			FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
			if (Side.SizeSquared() < 1e-3) Side = FVector::CrossProduct(Dir, Tan);
			Side.Normalize();
			FVector Normal = FVector::CrossProduct(Side, Dir).GetSafeNormal(); // leans upward by construction
			const FQuat Twist(Dir, R.FRandRange(-0.5f, 0.5f));
			L.Dir = Dir;
			L.Side = Twist.RotateVector(Side);
			L.Normal = Twist.RotateVector(Normal);
			L.Origin = L.Base + Dir * P.LeafStalk;

			// Skip leaves that would pass through wood.
			if (Field.Distance(L.Point(L.Length * 0.5f, 0.f)) < 8.0 || Field.Distance(L.Point(L.Length, 0.f)) < 8.0 ||
				Field.Distance(L.Point(L.Length * 0.25f, 0.f)) < 5.0)
			{
				continue;
			}
			const int32 Idx = Leaves.Add(L);
			ChunkLeaves[Chunk].Add(Idx);
			Grid.FindOrAdd(Cell(L.Origin)).Add(Idx);
			BranchLeaves.FindOrAdd(b).Add(Idx);
		}
	}

	// Richness: 1x at the lowest leafy branches up to about 3x at high outer tips, from the tree's shape alone. Height in
	// the crown counts most; how far out along its limb a leaf sits (distance from the trunk axis, relative to the limb's
	// other leaves) counts more the higher it is.
	if (Leaves.Num() > 0)
	{
		double ZMin = TNumericLimits<double>::Max(), ZMax = -TNumericLimits<double>::Max();
		for (const FTreeLeaf& L : Leaves)
		{
			ZMin = FMath::Min(ZMin, L.Base.Z);
			ZMax = FMath::Max(ZMax, L.Base.Z);
		}
		const FTreeBranch* Trunk = S.Branches.Num() > 0 && S.Branches[0].Level == 0 && S.Branches[0].Samples.Num() > 1 ? &S.Branches[0] : nullptr;
		auto AxisAt = [Trunk, &S](double Z)
		{
			if (!Trunk) return FVector2D(S.TrunkBase.X, S.TrunkBase.Y);
			// The trunk is close to vertical, so its samples rise monotonically.
			const TArray<FVector>& Smp = Trunk->Samples;
			int32 Lo = 0, Hi = Smp.Num() - 1;
			if (Z <= Smp[Lo].Z) return FVector2D(Smp[Lo].X, Smp[Lo].Y);
			if (Z >= Smp[Hi].Z) return FVector2D(Smp[Hi].X, Smp[Hi].Y);
			while (Hi - Lo > 1)
			{
				const int32 Mid = (Lo + Hi) / 2;
				if (Smp[Mid].Z < Z) Lo = Mid; else Hi = Mid;
			}
			const double Span = Smp[Hi].Z - Smp[Lo].Z;
			const FVector A = FMath::Lerp(Smp[Lo], Smp[Hi], Span > UE_SMALL_NUMBER ? (Z - Smp[Lo].Z) / Span : 0.0);
			return FVector2D(A.X, A.Y);
		};
		TArray<float> Out;
		Out.SetNumUninitialized(Leaves.Num());
		TArray<FVector2f> ChunkOut; // min, max distance from the trunk axis per limb
		ChunkOut.Init(FVector2f(TNumericLimits<float>::Max(), 0.f), ChunkLeaves.Num());
		for (int32 i = 0; i < Leaves.Num(); ++i)
		{
			const FTreeLeaf& L = Leaves[i];
			Out[i] = (float)FVector2D::Distance(FVector2D(L.Base.X, L.Base.Y), AxisAt(L.Base.Z));
			FVector2f& CO = ChunkOut[L.Chunk];
			CO.X = FMath::Min(CO.X, Out[i]);
			CO.Y = FMath::Max(CO.Y, Out[i]);
		}
		const double ZSpan = FMath::Max(ZMax - ZMin, 100.0);
		TArray<float> Sorted;
		Sorted.Reserve(Leaves.Num());
		double Sum = 0.0;
		for (int32 i = 0; i < Leaves.Num(); ++i)
		{
			FTreeLeaf& L = Leaves[i];
			const FVector2f& CO = ChunkOut[L.Chunk];
			const float H01 = FMath::Clamp((float)((L.Base.Z - ZMin) / ZSpan), 0.f, 1.f);
			const float Out01 = FMath::Clamp((Out[i] - CO.X) / FMath::Max(CO.Y - CO.X, 100.f), 0.f, 1.f);
			L.Richness = 1.f + 2.f * H01 * (0.4f + 0.6f * Out01);
			Sorted.Add(L.Richness);
			Sum += L.Richness;
		}
		Sorted.Sort();
		UE_LOG(LogLeafline, Log, TEXT("LeafSet: richness min %.2f, median %.2f, max %.2f, mean %.2f (leaves from %.0f to %.0f m)"),
			Sorted[0], Sorted[Sorted.Num() / 2], Sorted.Last(), Sum / Sorted.Num(), ZMin / 100.0, ZMax / 100.0);
	}
	UE_LOG(LogLeafline, Log, TEXT("LeafSet: %d leaves in %d chunks, %.1f ms"), Leaves.Num(), ChunkLeaves.Num(),
		(FPlatformTime::Seconds() - StartTime) * 1000.0);
}

int32 FLeafSet::FindNearest(const FVector& P, float Reach) const
{
	const FIntVector C = Cell(P);
	int32 Best = INDEX_NONE;
	double BestD = Reach;
	for (int32 X = -1; X <= 1; ++X)
		for (int32 Y = -1; Y <= 1; ++Y)
			for (int32 Z = -1; Z <= 1; ++Z)
			{
				const TArray<int32>* List = Grid.Find(C + FIntVector(X, Y, Z));
				if (!List) continue;
				for (int32 i : *List)
				{
					const FTreeLeaf& L = Leaves[i];
					if (!L.CanCut()) continue;
					// The ant cuts near the base of the blade, reaching from the twig.
					const FVector A = L.Base, B = L.Point(L.Length * 0.3f, 0.f);
					const FVector AB = B - A;
					const double T = FMath::Clamp(FVector::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
					const double D = FVector::Dist(P, A + AB * T);
					if (D < BestD)
					{
						BestD = D;
						Best = i;
					}
				}
			}
	return Best;
}

float FLeafSet::SizeCap(int32 Index, float UsePerCut) const
{
	if (!Leaves.IsValidIndex(Index) || !Leaves[Index].CanCut()) return 0.f;
	// A cut of size s uses UsePerCut * (0.35 + 0.65 s); never allow more than what is left.
	return FMath::Clamp((Leaves[Index].Remaining / FMath::Max(UsePerCut, 0.01f) - 0.35f) / 0.65f, 0.f, 1.f);
}

int32 FLeafSet::PiecesLeft(int32 Index, float Size01, float UsePerCut) const
{
	if (!Leaves.IsValidIndex(Index)) return 0;
	// Replays ApplyBite on a copy: a cut needs CanCut (fewer than 3 bites, more than 0.16 left) and SizeCap >= the size.
	const float Use = FMath::Max(UsePerCut, 0.01f);
	const float Size = FMath::Clamp(Size01, 0.f, 1.f);
	float Remaining = Leaves[Index].Remaining;
	int32 Bites = Leaves[Index].Bites;
	int32 Pieces = 0;
	while (Bites < 3 && Remaining > 0.16f)
	{
		const float Cap = FMath::Clamp((Remaining / Use - 0.35f) / 0.65f, 0.f, 1.f);
		if (Cap < Size - 0.001f) break;
		Remaining = FMath::Max(0.f, Remaining - Use * (0.35f + 0.65f * Size));
		++Bites;
		++Pieces;
	}
	return Pieces;
}

void FLeafSet::PlanBite(int32 Index, float DiameterCm, FVector2f& OutCenter, float& OutRadius) const
{
	if (!Leaves.IsValidIndex(Index)) return;
	const FTreeLeaf& L = Leaves[Index];
	const int32 K = FMath::Min<int32>(L.Bites, 2);
	const float SideSign = (K % 2 == 0) ? 1.f : -1.f;
	// The first bite is nearest the stalk, where the ant stands, and later ones work outward (30%, 52%, 74% along), so the
	// piece comes out close to the mouth.
	const float Along = L.Length * FMath::Clamp(0.30f + 0.22f * K, 0.25f, 0.9f);
	OutCenter = FVector2f(Along, SideSign * L.HalfWidthAt(Along) * 0.95f);
	OutRadius = DiameterCm * 0.55f;
}

bool FLeafSet::ApplyBite(int32 Index, float Size01, float DiameterCm, float UsePerCut)
{
	if (!Leaves.IsValidIndex(Index)) return false;
	FTreeLeaf& L = Leaves[Index];
	if (!L.CanCut()) return false;
	const int32 K = L.Bites;
	PlanBite(Index, DiameterCm, L.BiteCenter[K], L.BiteRadius[K]);
	L.Bites = K + 1;
	L.Remaining = FMath::Max(0.f, L.Remaining - UsePerCut * (0.35f + 0.65f * Size01));
	return true;
}

void FLeafSet::BuildChunkMesh(int32 Chunk, FTreeMeshData& Out, int32 Segments) const
{
	Out.Reset();
	if (!ChunkLeaves.IsValidIndex(Chunk)) return;
	const int32 K = Segments > 1 ? Segments : LeafSegments;

	for (int32 Idx : ChunkLeaves[Chunk])
	{
		const FTreeLeaf& L = Leaves[Idx];
		const float Fold = 0.05f * L.Width;

		// Outline: midrib points 0..K, edge points 1..K-1 on each side, with bites notched out of the edges.
		TArray<FVector, TInlineAllocator<32>> Pts;
		TArray<FVector2D, TInlineAllocator<32>> UVs;
		auto EdgeAcross = [&L](float Along, float Across)
		{
			for (int32 b = 0; b < L.Bites; ++b)
			{
				const FVector2f C = L.BiteCenter[b];
				const float R = L.BiteRadius[b];
				const float DX = Along - C.X;
				if (DX * DX >= R * R) continue;
				if (FMath::Sign(Across) != FMath::Sign(C.Y)) continue;
				const float H = FMath::Sqrt(R * R - DX * DX);
				const float Inner = C.Y - FMath::Sign(C.Y) * H; // circle boundary on the midrib side
				if (FMath::Sign(Inner) != FMath::Sign(Across)) Across = 0.f; // the bite reaches past the midrib
				else if (FMath::Abs(Across) > FMath::Abs(Inner)) Across = Inner;
			}
			return Across;
		};
		const int32 MidStart = 0;
		for (int32 k = 0; k <= K; ++k)
		{
			const float Along = L.Length * k / K;
			Pts.Add(L.Point(Along, 0.f) + L.Normal * (Fold * FMath::Sin(PI * k / K)));
			UVs.Add(FVector2D(float(k) / K, 0.5));
		}
		const int32 RightStart = Pts.Num();
		for (int32 k = 1; k < K; ++k)
		{
			const float Along = L.Length * k / K;
			const float Across = EdgeAcross(Along, L.HalfWidthAt(Along));
			Pts.Add(L.Point(Along, Across));
			UVs.Add(FVector2D(float(k) / K, 0.5 + Across / L.Width));
		}
		const int32 LeftStart = Pts.Num();
		for (int32 k = 1; k < K; ++k)
		{
			const float Along = L.Length * k / K;
			const float Across = EdgeAcross(Along, -L.HalfWidthAt(Along));
			Pts.Add(L.Point(Along, Across));
			UVs.Add(FVector2D(float(k) / K, 0.5 + Across / L.Width));
		}
		auto Edge = [&](int32 SideStart, int32 k) { return (k == 0 || k == K) ? MidStart + k : SideStart + k - 1; };

		// One face: the leaf material is two-sided foliage and lights the back itself. (A second, back-facing copy
		// z-fought with this one, and the wind sway, which moves vertices along their normals, pushed the two copies
		// apart in opposite directions: two leaves cutting through each other, and doubled shadows.)
		for (int32 Face = 0; Face < 1; ++Face)
		{
			const FVector N = L.Normal;
			const int32 V0 = Out.Vertices.Num();
			for (int32 i = 0; i < Pts.Num(); ++i)
			{
				Out.Vertices.Add(Pts[i]);
				Out.Normals.Add(N);
				Out.UV0.Add(UVs[i]);
				// Alpha: richness, 0 (1x) .. 1 (3x); M_Leaf turns it pale young green.
				Out.Colors.Add(FLinearColor(1.f, (Idx % 97) / 97.f, FMath::Clamp((float)(Pts[i].Z / 30000.0), 0.f, 1.f),
					FMath::Clamp((L.Richness - 1.f) * 0.5f, 0.f, 1.f)));
				Out.Tangents.Add(FProcMeshTangent(L.Dir, false));
			}
			auto AddTri = [&](int32 A, int32 B, int32 C)
			{
				if (A == B || B == C || A == C) return;
				// UE front faces: (b - a) x (c - a) points away from the viewer.
				const FVector Cross = FVector::CrossProduct(Pts[B] - Pts[A], Pts[C] - Pts[A]);
				if (FVector::DotProduct(Cross, N) > 0.0) Out.Triangles.Append({ V0 + A, V0 + C, V0 + B });
				else Out.Triangles.Append({ V0 + A, V0 + B, V0 + C });
			};
			for (int32 SideStart : { RightStart, LeftStart })
			{
				for (int32 k = 0; k < K; ++k)
				{
					const int32 A = MidStart + k, B = MidStart + k + 1, C = Edge(SideStart, k + 1), D = Edge(SideStart, k);
					AddTri(A, B, C);
					AddTri(A, C, D);
				}
			}
		}
	}
}
