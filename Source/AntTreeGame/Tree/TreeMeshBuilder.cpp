#include "Tree/TreeMeshBuilder.h"
#include "AntTreeGame.h"

namespace
{
	constexpr int32 CapRings = 4;

	void BuildBranch(const FTreeBranch& B, const FTreeParams& P, FTreeMeshData& Out)
	{
		const int32 NS = B.Samples.Num();
		if (NS < 2) return;

		// Tangents and parallel-transport frames so the rings do not twist.
		TArray<FVector, TInlineAllocator<256>> T, N;
		T.SetNum(NS);
		N.SetNum(NS);
		for (int32 i = 0; i < NS; ++i)
		{
			T[i] = (B.Samples[FMath::Min(i + 1, NS - 1)] - B.Samples[FMath::Max(i - 1, 0)]).GetSafeNormal();
			if (T[i].IsNearlyZero()) T[i] = i > 0 ? T[i - 1] : FVector::UpVector;
		}
		N[0] = FVector::CrossProduct(T[0], FMath::Abs(T[0].Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		for (int32 i = 1; i < NS; ++i)
		{
			const FVector Rotated = FQuat::FindBetweenNormals(T[i - 1], T[i]).RotateVector(N[i - 1]);
			N[i] = (Rotated - T[i] * FVector::DotProduct(Rotated, T[i])).GetSafeNormal();
			if (N[i].IsNearlyZero()) N[i] = N[i - 1];
		}

		float MaxR = 0.f;
		for (float R : B.SampleRadius) MaxR = FMath::Max(MaxR, R);
		const int32 M = FMath::Clamp(FMath::RoundToInt(2.f * PI * MaxR / P.AroundSpacing), P.MinAroundVerts, P.MaxAroundVerts);
		const int32 RingVerts = M + 1; // duplicate seam vertex for continuous UVs

		const FLinearColor BaseColor(FMath::Clamp(B.Level, 0, 3) / 3.f, B.Random, 0.f, 0.f);
		const float TreeH = FMath::Max(P.TrunkHeight, 1.f);

		auto AddRing = [&](const FVector& Center, const FVector& Tan, const FVector& Nrm, float Radius, float NormalTilt, float Along)
		{
			const FVector Bin = FVector::CrossProduct(Tan, Nrm);
			for (int32 j = 0; j <= M; ++j)
			{
				const double Theta = 2.0 * PI * j / M;
				const double C = FMath::Cos(Theta), S = FMath::Sin(Theta);
				const FVector Dir = Nrm * C + Bin * S;
				const FVector V = Center + Dir * Radius;
				Out.Vertices.Add(V);
				Out.Normals.Add((Dir * FMath::Cos(NormalTilt) + Tan * FMath::Sin(NormalTilt)).GetSafeNormal());
				Out.UV0.Add(FVector2D(Theta * FMath::Max(Radius, 1.f), Along));
				FLinearColor Col = BaseColor;
				Col.B = FMath::Clamp((float)V.Z / TreeH, 0.f, 1.f);
				Out.Colors.Add(Col);
				Out.Tangents.Add(FProcMeshTangent(-Nrm * S + Bin * C, false));
			}
		};

		auto Stitch = [&](int32 RingA, int32 RingB)
		{
			for (int32 j = 0; j < M; ++j)
			{
				const int32 A = RingA + j, Bv = RingA + j + 1, C = RingB + j, D = RingB + j + 1;
				// UE front faces: (v1 - v0) x (v2 - v0) points away from the viewer, i.e. into the tube.
				Out.Triangles.Append({ A, C, Bv, Bv, C, D });
			}
		};

		int32 PrevRing = INDEX_NONE;
		for (int32 i = 0; i < NS; ++i)
		{
			const int32 Lo = FMath::Max(i - 1, 0), Hi = FMath::Min(i + 1, NS - 1);
			const float DS = B.SampleDist[Hi] - B.SampleDist[Lo];
			const float DRDS = DS > UE_SMALL_NUMBER ? (B.SampleRadius[Hi] - B.SampleRadius[Lo]) / DS : 0.f;
			const int32 Ring = Out.Vertices.Num();
			AddRing(B.Samples[i], T[i], N[i], B.SampleRadius[i], FMath::Atan(-DRDS), B.SampleDist[i]);
			if (PrevRing != INDEX_NONE) Stitch(PrevRing, Ring);
			PrevRing = Ring;
		}

		// Rounded tip: a hemisphere of the last radius, like the field's capsule end.
		const FVector TipC = B.Samples.Last();
		const FVector TipT = T.Last();
		const float TipR = B.SampleRadius.Last();
		const float TipAlong = B.SampleDist.Last();
		for (int32 k = 1; k < CapRings; ++k)
		{
			const float Phi = 0.5f * PI * k / CapRings;
			const int32 Ring = Out.Vertices.Num();
			AddRing(TipC + TipT * (TipR * FMath::Sin(Phi)), TipT, N.Last(), TipR * FMath::Cos(Phi), Phi, TipAlong + TipR * Phi);
			Stitch(PrevRing, Ring);
			PrevRing = Ring;
		}
		const int32 Tip = Out.Vertices.Num();
		Out.Vertices.Add(TipC + TipT * TipR);
		Out.Normals.Add(TipT);
		Out.UV0.Add(FVector2D(0.0, TipAlong + TipR * 0.5f * PI));
		FLinearColor TipCol = BaseColor;
		TipCol.B = FMath::Clamp((float)(TipC.Z / TreeH), 0.f, 1.f);
		Out.Colors.Add(TipCol);
		Out.Tangents.Add(FProcMeshTangent(N.Last(), false));
		for (int32 j = 0; j < M; ++j)
		{
			Out.Triangles.Append({ PrevRing + j, Tip, PrevRing + j + 1 });
		}
		(void)RingVerts;
	}
}

void TreeMeshBuilder::Build(const FTreeSkeleton& S, FTreeMeshData& Out)
{
	const double StartTime = FPlatformTime::Seconds();
	Out.Reset();
	for (const FTreeBranch& B : S.Branches)
	{
		BuildBranch(B, S.Params, Out);
	}
	UE_LOG(LogLeafline, Log, TEXT("TreeMeshBuilder: %d vertices, %d triangles, %.1f ms"),
		Out.Vertices.Num(), Out.Triangles.Num() / 3, (FPlatformTime::Seconds() - StartTime) * 1000.0);
}
