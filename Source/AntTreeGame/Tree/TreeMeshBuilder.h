#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Tree/TreeTypes.h"

struct FTreeMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	/** Optional (empty for leaves). */
	TArray<FVector2D> UV1;
	/** Bark only: distance along the branch (the value in UV0.y) as (Hi, Lo), Hi = floor(Along / 256) * 256, Lo = Along - Hi. */
	TArray<FVector2D> UV2;
	/** Bark only: (radius without flare and collar, buried distance). See the namespace comment. */
	TArray<FVector2D> UV3;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	void Reset()
	{
		Vertices.Reset();
		Triangles.Reset();
		Normals.Reset();
		UV0.Reset();
		UV1.Reset();
		UV2.Reset();
		UV3.Reset();
		Colors.Reset();
		Tangents.Reset();
	}
};

/**
 * Smooth tubes around each branch centreline, with rounded tips matching the field's capsule ends.
 * UV0 = (arc length around, distance along) in cm. UV1 = (R cos theta, R sin theta) in cm: with UV0.y it places the
 * bark on a seamless cylinder for 3D noise (no seam around, physical scale everywhere, tips close to a point).
 * UV2 = (Hi, Lo) split of UV0.y: Hi = floor(along / 256) * 256, Lo = along - Hi. Both are exact in half floats and Hi + Lo
 * interpolates linearly, giving the along distance at sub-millimetre precision.
 * UV3 = (bark radius, buried distance) for the bark shader. x is the branch's BarkRadius at the ring (the radius without the
 * trunk's ground flare and without the collar; SampleRadius when BarkRadius is empty; cap rings and the tip use the last
 * sample's). y is the true signed distance in cm from the vertex to the surface of the branch it overlaps (its parent for a
 * child, its children's bases for a parent whose child's base ring lies wholly inside it), negative inside, NOT clamped:
 * an unclamped distance to a convex tube interpolates conservatively across the 300 cm triangles of the trunk. 10000 where
 * nothing is tested. The material hides y < -15. Values are half floats on the GPU.
 * Colour = (level/3, per-branch random, height on tree, buried). Alpha is a signed distance to the other branches the
 * vertex overlaps (its parent for a child, its children's bases for a parent): A = clamp(0.5 + d / 400, 0, 1), d in cm,
 * negative inside the other branch; 1 where nothing overlaps. The material treats A < 0.4625 (more than 15 cm inside)
 * as buried; the wide range and the margin keep the linear interpolation across large triangles from cutting real
 * surface at a junction. Colours reach the GPU linearly, so this is the exact 8-bit value.
 */
class FTreeField;

namespace TreeMeshBuilder
{
	/**
	 * With a field that has fillet zones (FTreeParams::bFillets), each limb, bough and root also gets a skirt over its
	 * junction: a band from the child's first ring clear of the fillet, over the field's rounded surface, to a curve on
	 * the parent clear of it. It lies on the field (inner rows 0.3 cm outside it), so the tubes' crease is hidden behind it.
	 * Looks only: the ants walk on the field without fillets (ATreeActor::GetField), so they sink into the skirt a little. Both edges share the tubes' surface exactly (the child ring's own
	 * vertices; points on the parent's facets with its interpolated vertex data), so nothing steps where they meet. The
	 * child half carries the child's bark data continued down the band (UV1 and bark radius of the ring, along decreasing
	 * with the distance down), the parent half the parent's, interpolated as its tube does; they switch with a hard seam
	 * where the surface is equally far from both tubes (where the crease was). UV3.y = 10000 (never buried), alpha 1.
	 * Without a field (the grove, Leaf.MeshGap) there are no skirts. OutNoSkirt receives the branches whose fillet got no
	 * skirt (the caller removes those fillets from the field, so the ant never floats over a crease it cannot see).
	 * With OutGroundSkirt, the trunk and roots also get a skirt where they meet the ground (FTreeParams::FilletGroundK):
	 * its bark half goes into Out, its ground half into OutGroundSkirt for the ground's mesh section (UV0 = world XY,
	 * black, the ground's vertex data).
	 */
	void Build(const FTreeSkeleton& Skeleton, FTreeMeshData& Out, const FTreeField* Field = nullptr, TArray<int32>* OutNoSkirt = nullptr,
		FTreeMeshData* OutGroundSkirt = nullptr);
}
