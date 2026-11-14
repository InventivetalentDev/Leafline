#include "World/GroundRelief.h"

// A named namespace, not an anonymous one: unity builds merge this file with others that have their own Noise or Hash.
namespace LeafReliefDetail
{
	// Off by default (14 Nov, 18:15): on and off differed by 3 to 6 grey levels in every test view (the ground beyond 160 m
	// lies behind thick fog), so it bought nothing for the risk of grove feet on slopes. Leaf.Relief 1 turns it on.
	bool GReliefOn = false;
	int32 GStamp = 1;
	FVector2D GCentre = FVector2D::ZeroVector;
	FVector2D GOffset = FVector2D::ZeroVector;

	uint32 Hash2(int32 X, int32 Y)
	{
		uint32 H = uint32(X) * 0x8da6b343u ^ uint32(Y) * 0xd8163841u;
		H ^= H >> 13;
		H *= 0x9e3779b1u;
		H ^= H >> 16;
		return H;
	}

	/** Smooth value noise in [-1, 1] with a quintic fade (continuous slope), unit cells. */
	double Noise(double X, double Y)
	{
		const double Fx = FMath::Floor(X), Fy = FMath::Floor(Y);
		const int32 Ix = int32(Fx), Iy = int32(Fy);
		const double Tx = X - Fx, Ty = Y - Fy;
		const double Ux = Tx * Tx * Tx * (Tx * (Tx * 6.0 - 15.0) + 10.0);
		const double Uy = Ty * Ty * Ty * (Ty * (Ty * 6.0 - 15.0) + 10.0);
		auto V = [](uint32 H) { return double(H & 0xffffu) / 32767.5 - 1.0; };
		const double A = V(Hash2(Ix, Iy)), B = V(Hash2(Ix + 1, Iy)), C = V(Hash2(Ix, Iy + 1)), D = V(Hash2(Ix + 1, Iy + 1));
		return FMath::Lerp(FMath::Lerp(A, B, Ux), FMath::Lerp(C, D, Ux), Uy);
	}

	double SmoothStep(double A, double B, double X)
	{
		const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** One band of swells: wavelength (cm), height (cm, half the peak to trough), and the radius by which it has faded out
	 *  because the ground rings (4% wider each beyond 80 m) get too coarse to carry it (four rings per wavelength). */
	struct FBand { double Wavelength; double Height; double Rotate; };
	constexpr FBand Bands[] = {
		// Slopes stay gentle (about 6% at most per band); 1.5 to 4 m (first try) did not show from 120 m up.
		{ 8000.0, 250.0, 0.0 },     // low banks, 80 m
		{ 24000.0, 700.0, 0.9 },    // swells, 240 m
		{ 90000.0, 2500.0, 2.1 },   // rises, 900 m
		{ 300000.0, 5000.0, 3.7 },  // the land's lie, 3 km: lifts and drops the far ground
	};
}

using namespace LeafReliefDetail;

namespace LeafRelief
{
	bool IsOn() { return GReliefOn; }

	void SetOn(bool bOn)
	{
		if (bOn != GReliefOn) ++GStamp;
		GReliefOn = bOn;
	}

	int32 Stamp() { return GStamp; }

	void SetFrame(const FVector2D& Centre, int32 Seed)
	{
		const uint32 H = Hash2(Seed, 7177);
		const FVector2D Offset(double(H & 0xffffu) * 37.0, double(H >> 16) * 41.0);
		if (!Centre.Equals(GCentre, 1.0) || !Offset.Equals(GOffset, 1.0)) ++GStamp;
		GCentre = Centre;
		GOffset = Offset;
	}

	float Height(double X, double Y)
	{
		if (!GReliefOn) return 0.f;
		const FVector2D Rel(X - GCentre.X, Y - GCentre.Y);
		const double R = Rel.Size();
		if (R <= StartRadius) return 0.f;
		const double Ramp = SmoothStep(StartRadius, FullRadius, R);
		const FVector2D Q = Rel + GOffset;
		double H = 0.0;
		for (const FBand& B : Bands)
		{
			// Rings 4% apart radially: a band needs four per wavelength, so it fades out from 0.75 to 1 of that radius.
			const double Reach = B.Wavelength / (4.0 * 0.04);
			const double Keep = 1.0 - SmoothStep(0.75 * Reach, Reach, R);
			if (Keep <= 0.0) continue;
			const double C = FMath::Cos(B.Rotate), S = FMath::Sin(B.Rotate);
			const double U = (C * Q.X - S * Q.Y) / B.Wavelength, V = (S * Q.X + C * Q.Y) / B.Wavelength;
			// The far bands grow with the distance, so near the walking field the ground only swells gently.
			const double Grow = B.Wavelength > 50000.0 ? SmoothStep(FullRadius, FullRadius + B.Wavelength, R) : 1.0;
			H += B.Height * Keep * Grow * Noise(U, V);
		}
		return float(H * Ramp);
	}

	FVector Normal(double X, double Y)
	{
		if (!GReliefOn) return FVector::UpVector;
		constexpr double E = 100.0;
		const double Dx = (Height(X + E, Y) - Height(X - E, Y)) / (2.0 * E);
		const double Dy = (Height(X, Y + E) - Height(X, Y - E)) / (2.0 * E);
		return FVector(-Dx, -Dy, 1.0).GetSafeNormal();
	}

	float Slope(double X, double Y)
	{
		const FVector N = Normal(X, Y);
		return N.Z > 0.01 ? float(FVector2D(N.X, N.Y).Size() / N.Z) : 100.f;
	}
}
